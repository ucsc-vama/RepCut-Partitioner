//
// Created by Haoyuan Wang on 11/8/22.
//

#include "ClusterGraph.h"
#include "ConeTrie.h"
#include "Log.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_set>
#include <vector>

using namespace repcut;

// ---------------------------------------------------------------------------
// Phase A: Mark cones.
//
// For each sink in `dag->sinkNodes` (cone id = its index), BFS upstream over
// valid predecessors exactly like the original `_collect_cones`, but instead
// of materializing the cone's node list we only descend each visited vertex
// one level deeper in the persistent cone-id trie.  After this pass every
// vertex holds a pointer to the trie leaf whose path is its cone-id set.
//
// Cones are processed in strictly increasing cone-id order so the trie path
// for any vertex is its canonical (sorted, unique) set of cone ids.  Vertices
// sharing the same set of cones share the same leaf node.
// ---------------------------------------------------------------------------
void ClusterGraph::_mark_cones() {
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Mark cones: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto numVtxes = dag->numVertices();
    const auto numSinks = dag->sinkNodes.size();
    assert(numSinks > 0);

    coneTrie = std::make_unique<ConeTrie>();
    vtxToNode.assign(numVtxes, coneTrie->root());

    std::unordered_set<uint32_t> coneVisited;
    std::vector<uint32_t> fringe;
    std::vector<uint32_t> fringeNext;

    // Cones are processed in strictly increasing cone-id order, so each
    // vertex's root-to-leaf trie path is its canonical (sorted, unique)
    // cone-id set.  See the ordering invariant in include/ConeTrie.h.
    // Do not parallelize this loop without re-establishing that invariant.
    for (uint32_t cone_id = 0; cone_id < numSinks; ++cone_id) {
        const uint32_t seed = dag->sinkNodes[cone_id];

        coneVisited.clear();
        fringe.clear();

        coneVisited.insert(seed);
        fringe.push_back(seed);
        vtxToNode[seed] = coneTrie->visit(vtxToNode[seed], cone_id);

        while (!fringe.empty()) {
            fringeNext.clear();
            for (const auto vtx : fringe) {
                for (const auto nid : dag->inNeigh[vtx]) {
                    if (!dag->valid[nid]) continue;
                    if (coneVisited.insert(nid).second) {
                        vtxToNode[nid] = coneTrie->visit(vtxToNode[nid], cone_id);
                        fringeNext.push_back(nid);
                    }
                }
            }
            std::swap(fringe, fringeNext);
        }
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Mark cones: Done in %llums (trie nodes = %zu)\n",
             duration.count(), coneTrie->nodeCount());
}

// ---------------------------------------------------------------------------
// Phase B: Collect clusters.
//
// A cluster is a connected component (using both in- and out-edges) within
// the subgraph induced on vertices that share the same trie leaf, i.e. the
// same cone-id set.  This reproduces the original connectivity-based
// clustering (a strict "group-by identical cone-id set" would collapse
// disconnected components that happen to share a set).
//
// Cluster ids are assigned so that the first `numSinks` ids correspond to
// the connected components containing sink 0, sink 1, ... in `dag->sinkNodes`
// order.  Since two distinct sinks cannot be ancestors of each other (each
// sink has out_degree 0), they cannot share a cone-id set, so the first
// numSinks clusters are exactly one per cone, matching the invariant used
// downstream by writeHMetisFile.  Remaining clusters are numbered in
// increasing lowest-unassigned vertex-id order.
// ---------------------------------------------------------------------------
void ClusterGraph::_collect_clusters() {
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Collect clusters: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto numVtxes = dag->numVertices();
    const auto numSinks = dag->sinkNodes.size();

    idToClusterId.assign(numVtxes, -1);
    for (uint32_t nid = 0; nid < numVtxes; ++nid) {
        if (!dag->valid[nid]) {
            idToClusterId[nid] = -2;
        }
    }

    // Iterative flood fill restricted to neighbors pointing at the same trie
    // leaf as the seed.  Original code used recursion; we use an explicit
    // stack so deeply nested netlists cannot overflow the call stack.
    auto floodFill = [&](uint32_t seed, uint32_t cluster_id) {
        auto& cluster = this->clusters[cluster_id];
        std::vector<uint32_t> stk;
        stk.reserve(64);

        assert(idToClusterId[seed] == -1);
        idToClusterId[seed] = static_cast<int32_t>(cluster_id);
        cluster.push_back(seed);
        stk.push_back(seed);

        while (!stk.empty()) {
            const uint32_t u = stk.back();
            stk.pop_back();
            auto leaf = vtxToNode[u];

            for (const auto w : dag->inNeigh[u]) {
                if (idToClusterId[w] == -1 && vtxToNode[w] == leaf) {
                    idToClusterId[w] = static_cast<int32_t>(cluster_id);
                    cluster.push_back(w);
                    stk.push_back(w);
                }
            }
            for (const auto w : dag->outNeigh[u]) {
                if (idToClusterId[w] == -1 && vtxToNode[w] == leaf) {
                    idToClusterId[w] = static_cast<int32_t>(cluster_id);
                    cluster.push_back(w);
                    stk.push_back(w);
                }
            }
        }
    };

    // 1. Sink first, in sink order (preserves cone_id == cluster_id invariant).
    for (uint32_t i = 0; i < numSinks; ++i) {
        const uint32_t seed = dag->sinkNodes[i];
        uint32_t cluster_id = static_cast<uint32_t>(clusters.size());
        clusters.emplace_back();
        floodFill(seed, cluster_id);
        // First numSinks cluster ids are the sink clusters, by construction.
        assert(cluster_id == i);
        sinkNodes.push_back(cluster_id);
    }

    // 2. Remaining valid, unassigned vertices in vertex-id order.
    for (uint32_t v = 0; v < numVtxes; ++v) {
        if (idToClusterId[v] != -1) continue;
        uint32_t cluster_id = static_cast<uint32_t>(clusters.size());
        clusters.emplace_back();
        floodFill(v, cluster_id);
    }

    // Derive clusterIdToPins[cid] directly from the trie path of any vertex
    // in the cluster (they are all equal by construction, sorted & unique).
    clusterIdToPins.clear();
    clusterIdToPins.reserve(clusters.size());
    for (auto& cluster : clusters) {
        assert(!cluster.empty());
        auto leaf = vtxToNode[cluster.front()];
        clusterIdToPins.push_back(coneTrie->pathConeIds(leaf));
    }

    // Trie + per-vertex trie pointers are no longer needed downstream.
    vtxToNode.clear();
    vtxToNode.shrink_to_fit();
    coneTrie.reset();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Collect clusters: Done in %llums (%zu clusters)\n",
             duration.count(), clusters.size());
}

// Cluster weight = 1 + sum of valid member weights.  The +1 floor keeps
// KaHyPar happy since it does not accept weight 0.  Replaces the former
// bundled-property write on the boost adjacency_list.
void ClusterGraph::_update_cluster_weight() {
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Update cluster weight: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto numClusters = clusters.size();
    nodeWeight.assign(numClusters, 0.0f);

    for (uint32_t cluster_id = 0; cluster_id < numClusters; ++cluster_id) {
        float cluster_weight = 1;  // +1 floor, see comment above
        for (auto& stmt_id : clusters[cluster_id]) {
            if (dag->valid[stmt_id]) {
                assert(dag->weight[stmt_id] >= 0);
                cluster_weight += dag->weight[stmt_id];
            }
        }
        nodeWeight[cluster_id] = cluster_weight;
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Update cluster weight: Done in %llums\n", duration.count());
}

// ---------------------------------------------------------------------------
// Phase C: Derive cones_cg_nodes without re-walking the original cones.
//
// For each cluster, `clusterIdToPins[cid]` is already the exact list of cone
// ids that touch the cluster (its trie path).  `cones_cg_nodes[cone_id]` is
// therefore the inverse mapping: for every (cluster, cone) pair implied by
// the pins, append the cluster id to that cone's cluster list.  Each such
// pair is enumerated exactly once, so no deduplication is needed (the trie
// paths are unique sorted sequences).
// ---------------------------------------------------------------------------
void ClusterGraph::_update_cluster_cone() {
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Update cluster cones: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto numCones = dag->sinkNodes.size();
    cones_cg_nodes.assign(numCones, {});

    for (uint32_t cid = 0; cid < clusters.size(); ++cid) {
        for (auto cone_id : clusterIdToPins[cid]) {
            cones_cg_nodes[cone_id].push_back(cid);
        }
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Update cluster cones: Done in %llums\n", time_ms);
}


void ClusterGraph::collapseFromDAG(const DirectedAcyclicGraph &dag) {
    rcp_log(log_level, REPCUT_LOG_INFO, "Collapse cluster graph: Start\n");
    auto start = std::chrono::system_clock::now();

    this -> dag = &dag;
    // 1. find sink vtxs — already done in dag

    // 2. mark cones (build persistent cone-id trie + per-vertex pointers)
    this->_mark_cones();

    // 3. collect clusters (connected components within same-leaf groups)
    this->_collect_clusters();

    // 4. compute cluster weight
    this->_update_cluster_weight();

    // 5. build inverse mapping (cone → touching clusters)
    this->_update_cluster_cone();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    rcp_log(log_level, REPCUT_LOG_INFO, "Collapse cluster graph: Done in %llums\n", time_ms);
}

// ---------------------------------------------------------------------------
// Stream cluster graph to the hMetis-format file expected by MtKaHyPar.
// Mirrors the wire format previously produced by HyperGraph::writeTohMetisFile
// (header `numEdges numNodes 11`, then weighted edge lines, then per-node
// weight lines).  Cone clusters (cone_id in [0, numCones)) become hypergraph
// vertices; non-cone clusters become hyperedges whose pins are their
// `clusterIdToPins` cone-id list (1-indexed on the wire).  Node weight
// formula matches HyperGraph::buildFromClusterGraph: own cluster weight plus
// a proportional share of each connected non-self cluster's weight (divided
// by that cluster's pin count).
// ---------------------------------------------------------------------------
void ClusterGraph::writeHMetisFile(const char* filename) {
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Write to hMetis graph file: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto numCones = dag->sinkNodes.size();
    assert(numCones == cones_cg_nodes.size());
    const auto numEdges = clusters.size() - numCones;
    const auto numNodes = numCones;

    // Compute node weights upfront so the wire order is header, then all
    // edges, then all node weights (matching HyperGraph's layout).  Only
    // numCones uint32_t of transient storage — negligible vs HyperGraph.
    std::vector<uint32_t> nodeWeights;
    nodeWeights.reserve(numNodes);
    for (uint32_t cone_id = 0; cone_id < numCones; ++cone_id) {
        auto cone_weight = static_cast<uint32_t>(nodeWeight[cone_id]);
        uint32_t connected_cluster_weights = 0;
        for (auto& cluster_id : cones_cg_nodes[cone_id]) {
            if (cluster_id != cone_id) {
                auto pin_count = clusterIdToPins[cluster_id].size();
                auto cluster_weight = static_cast<uint32_t>(nodeWeight[cluster_id]);
                connected_cluster_weights += (cluster_weight / pin_count);
            }
        }
        nodeWeights.push_back(cone_weight + connected_cluster_weights);
    }

    std::ofstream ofs(filename);

    // header: numEdges numNodes "11"
    ofs << numEdges << " " << numNodes << " 11\n";

    // edges: one line per non-cone cluster, `weight pin1 pin2 ...` (pins 1-indexed)
    std::string line;
    for (uint32_t cluster_id = numCones; cluster_id < clusters.size(); ++cluster_id) {
        auto edge_weight = static_cast<uint32_t>(nodeWeight[cluster_id]);

        line.clear();
        line += std::to_string(edge_weight);
        for (auto& pin : clusterIdToPins[cluster_id]) {
            line += ' ';
            line += std::to_string(pin + 1);
        }
        line += '\n';
        ofs << line;
    }

    // nodes: one weight per line
    for (auto& w : nodeWeights) {
        ofs << w << "\n";
    }

    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Write to hMetis graph file: Done in %llums\n", duration.count());
}
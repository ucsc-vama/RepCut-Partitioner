//
// Created by Haoyuan Wang on 11/8/22.
//

#include "dag.h"
#include "rcp_common.h"

#include <cassert>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <unordered_set>

#include "cluster_graph.h"
#include "cone_trie.h"
#include <iostream>
#include <fstream>
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
    BOOST_LOG_TRIVIAL(trace) << "Mark cones: Start";
    auto start = std::chrono::system_clock::now();

    const auto numVtxes = boost::num_vertices(dag->graph);
    const auto numSinks = dag->sinkNodes.size();
    assert(numSinks > 0);

    coneTrie = std::make_unique<ConeTrie>();
    vtxToNode.assign(numVtxes, coneTrie->root());

    std::unordered_set<uint32_t> coneVisited;
    std::vector<uint32_t> fringe;
    std::vector<uint32_t> fringeNext;

    // Cones are processed in strictly increasing cone-id order, so each
    // vertex's root-to-leaf trie path is its canonical (sorted, unique)
    // cone-id set.  See the ordering invariant in include/cone_trie.h.
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
                for (auto inEdges = boost::in_edges(vtx, dag->graph);
                     inEdges.first != inEdges.second; ++inEdges.first) {
                    const auto nid = static_cast<uint32_t>(boost::source(*inEdges.first, dag->graph));
                    if (!dag->graph[nid].valid) continue;
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
    BOOST_LOG_TRIVIAL(trace) << "Mark cones: Done in " << duration.count() << "ms"
                             << " (trie nodes = " << coneTrie->nodeCount() << ")";
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
// order.  This preserves the cone_id == cluster_id invariant relied upon by
// hyper_graph.cpp.  Remaining clusters are numbered in increasing lowest
// unassigned vertex-id order, matching the original residual scan.
// ---------------------------------------------------------------------------
void ClusterGraph::_collect_clusters() {
    BOOST_LOG_TRIVIAL(trace) << "Collect clusters: Start";
    auto start = std::chrono::system_clock::now();

    const auto numVtxes = boost::num_vertices(dag->graph);
    const auto numSinks = dag->sinkNodes.size();

    idToClusterId.assign(numVtxes, -1);
    for (uint32_t nid = 0; nid < numVtxes; ++nid) {
        if (!dag->graph[nid].valid) {
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

            for (auto inEdges = boost::in_edges(u, dag->graph);
                 inEdges.first != inEdges.second; ++inEdges.first) {
                const auto w = static_cast<uint32_t>(boost::source(*inEdges.first, dag->graph));
                if (idToClusterId[w] == -1 && vtxToNode[w] == leaf) {
                    idToClusterId[w] = static_cast<int32_t>(cluster_id);
                    cluster.push_back(w);
                    stk.push_back(w);
                }
            }
            for (auto outEdges = boost::out_edges(u, dag->graph);
                 outEdges.first != outEdges.second; ++outEdges.first) {
                const auto w = static_cast<uint32_t>(boost::target(*outEdges.first, dag->graph));
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
    BOOST_LOG_TRIVIAL(trace) << "Collect clusters: Done in " << duration.count() << "ms"
                             << " (" << clusters.size() << " clusters)";
}

void ClusterGraph::_build_cluster_graph() {
    BOOST_LOG_TRIVIAL(trace) << "Build cluster graph: Start";
    auto start = std::chrono::system_clock::now();

    auto numNodes = this->clusters.size();
    for (size_t i = 0; i < numNodes; i++) {
        auto newVtx = boost::add_vertex(graph);
        assert(newVtx == i);
    }

    for (uint32_t cluster_id = 0; cluster_id < this->clusters.size(); cluster_id++) {
        //
        std::unordered_set<uint32_t> cluster_outNeighs;

        for (auto &nid: this->clusters[cluster_id]) {
            // save all outNeighs
            for (auto outEdges = boost::out_edges(nid, dag->graph); outEdges.first != outEdges.second; outEdges.first++) {
                auto outNeigh_id = boost::target(*outEdges.first, dag->graph);
                cluster_outNeighs.insert(outNeigh_id);
            }
        }

        for (auto &nid: this->clusters[cluster_id]) {
            // remove all inNeighs
            for (auto inEdges = boost::in_edges(nid, dag->graph); inEdges.first != inEdges.second; inEdges.first++) {
                auto inNeigh_id = boost::source(*inEdges.first, dag->graph);
                if (cluster_outNeighs.contains(inNeigh_id)) {
                    cluster_outNeighs.erase(inNeigh_id);
                }
            }
        }

        // Assert: if this cluster is confirmed to be a sink cluster,
        // it must exist in this->sinkNodes
        if (cluster_outNeighs.empty()) {
            assert(std::find(this->sinkNodes.begin(), this->sinkNodes.end(), cluster_id) != this->sinkNodes.end());
        } else {
            // Has descendent(s)
            std::unordered_set<uint32_t> descendent_clusters;
            for (auto &outNeigh_id: cluster_outNeighs) {
                int32_t outNeigh_cluster_id = this->idToClusterId[outNeigh_id];
                // cluster_id == -1 => unvisited
                // Shouldn't happen here
                assert(outNeigh_cluster_id != -1);
                // cluster_id == -2 => invalid
                if (outNeigh_cluster_id >= 0) {
                    descendent_clusters.insert(static_cast<uint32_t>(outNeigh_cluster_id));
                }
            }
            // add edge
            for (auto edge_target: descendent_clusters) {
                boost::add_edge(cluster_id, edge_target, graph);
            }
        }
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Build cluster graph: Done in " << time_ms << "ms";
}


void ClusterGraph::_update_cluster_weight() {
    BOOST_LOG_TRIVIAL(trace) << "Update cluster weight: Start";
    auto start = std::chrono::system_clock::now();

    auto numNodes = boost::num_vertices(graph);
    for (int cluster_id = 0; cluster_id < numNodes; cluster_id++) {
        // Weight starts at 1 to make KaHyPar happy
        float cluster_weight = 1;

        for (auto& stmt_id: this->clusters[cluster_id]) {
            if (dag->graph[stmt_id].valid) {
                // For every valid nodes, weight must >= 0
                assert(dag->graph[stmt_id].weight >= 0);
                cluster_weight += dag->graph[stmt_id].weight;
            }
        }

        graph[cluster_id].weight = cluster_weight;
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Update cluster weight: Done in " << time_ms << "ms";
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
    BOOST_LOG_TRIVIAL(trace) << "Update cluster cones: Start";
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
    BOOST_LOG_TRIVIAL(trace) << "Update cluster cones: Done in " << time_ms << "ms";
}


void ClusterGraph::collapseFromDAG(DirectedAcyclicGraph *dag) {
    BOOST_LOG_TRIVIAL(info) << "Collapse cluster graph: Start";
    auto start = std::chrono::system_clock::now();

    this -> dag = dag;
    // 1. find sink vtxs
    // Already done in dag

    // 2. mark cones (build persistent cone-id trie + per-vertex pointers)
    this->_mark_cones();

    // 3. collect clusters (connected components within same-leaf groups)
    this->_collect_clusters();

    // 4. build cluster graph
    this->_build_cluster_graph();

    // 5. computer cluster weight
    this->_update_cluster_weight();

    // 6. calculate cones in cluster graph
    this->_update_cluster_cone();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(info) << "Collapse cluster graph: Done in " << time_ms << "ms";
}
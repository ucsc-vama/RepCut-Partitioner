//
// Created by Haoyuan Wang on 11/8/22.
//

#include "rcp_common.h"

#include <chrono>
#include <algorithm>
#include <numeric>

#include "cluster_graph.h"
#include "rcp_util.h"
#include <iostream>
#include <fstream>

#include <boost/graph/topological_sort.hpp>

using namespace repcut;

// Recursively collect cluster
void ClusterGraph::_collect_cluster_worker(uint32_t cluster_id, uint32_t seed) {

    if (this->idToClusterId[seed] == -1) {
        // unvisited
        this->clusters[cluster_id].push_back(seed);
        this->idToClusterId[seed] = static_cast<int32_t>(cluster_id);

        std::vector<uint32_t> connected_vtxs;
        for (auto inEdges = boost::in_edges(seed, dag->graph); inEdges.first != inEdges.second; inEdges.first++) {
            auto vtx = boost::source(*inEdges.first, dag->graph);
            if (this->idToClusterId[vtx] == -1) {
                connected_vtxs.push_back(vtx);
            }
        }
        for (auto outEdges = boost::out_edges(seed, dag->graph); outEdges.first != outEdges.second; outEdges.first++) {
            auto vtx = boost::target(*outEdges.first, dag->graph);
            if (this->idToClusterId[vtx] == -1) {
                connected_vtxs.push_back(vtx);
            }
        }

        for (auto& vtx: connected_vtxs) {
            if (this->idToConeId[vtx] == this->idToConeId[seed]) {
                // Same cluster
                _collect_cluster_worker(cluster_id, vtx);
            }
        }
    }
}



void ClusterGraph::_collect_cones() {
    BOOST_LOG_TRIVIAL(trace) << "Collect cones: Start";
    auto start = std::chrono::system_clock::now();

    auto numVtxes = boost::num_vertices(dag->graph);

    std::unordered_map<uint32_t, std::vector<uint32_t>> cone_cache;

    std::vector<bool> visited;
    std::vector<uint32_t> topo_order;
    visited.resize(numVtxes, false);
    topo_order.reserve(numVtxes);
    boost::topological_sort(dag->graph, std::back_inserter(topo_order));
    std::reverse(topo_order.begin(), topo_order.end());

    uint32_t vtxCnt = 0;
    for (auto vtx: topo_order) {
        std::unordered_set<uint32_t> dep_nodes;
        dep_nodes.insert(vtx);
        for (auto inEdges = boost::in_edges(vtx, dag->graph); inEdges.first != inEdges.second; inEdges.first++) {
            auto nid = boost::source(*inEdges.first, dag->graph);
            if (dag->graph[nid].valid) {
                assert(cone_cache.contains(nid));
                dep_nodes.insert(cone_cache[nid].begin(), cone_cache[nid].end());
            }
        }

        std::vector<uint32_t> dep_nodes_vec;
        dep_nodes_vec.assign(dep_nodes.begin(), dep_nodes.end());
        cone_cache[vtx] = std::move(dep_nodes_vec);
        visited[vtx] = true;
        vtxCnt++;

        // release memory
        // cone_cache can be very big, and most contents are rarely used.
        // scan for unused items every 16383 vtxes
        // (16383 is just a magic number)
        if ((vtxCnt & 0x3FFF) == 0) {
            std::vector<uint32_t> unused_cache;
            for (const auto &item: cone_cache) {
                auto key = item.first;
                if (boost::out_degree(key, dag->graph) == 0) continue;
                // if all users are visited, this vtx can be removed from cache
                bool allUsersVisited = true;
                for (auto outEdges = boost::out_edges(key, dag->graph); outEdges.first != outEdges.second; outEdges.first++) {
                    auto target = boost::target(*outEdges.first, dag->graph);
                    if (!visited[target]) {
                        allUsersVisited = false;
                        break;
                    }
                }
                if (allUsersVisited) unused_cache.push_back(key);
            }
//            std::cout << "Remove " << unused_cache.size() << " unused cache elements\n";
            for (auto ev: unused_cache) {
                cone_cache.erase(ev);
            }
        }
    }

    for (auto& cone_seed: dag->sinkNodes) {
        this->cones_original_nodes.push_back(cone_cache[cone_seed]);
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Collect cones: Done in " << time_ms << "ms";
}


void ClusterGraph::_collect_clusters() {
    BOOST_LOG_TRIVIAL(trace) << "Collect clusters: Start";
    auto start = std::chrono::system_clock::now();

    // mark cone id
    auto dagNumVtxes = boost::num_vertices(dag->graph);
    this->idToConeId.assign(dagNumVtxes, std::unordered_set<uint32_t>());

    for (uint32_t cid = 0; cid < this->cones_original_nodes.size(); cid++) {
        for (auto& nid: this->cones_original_nodes[cid]) {
            this->idToConeId[nid].insert(cid);
        }
    }

    // init this->idToClusterId
    // -1: unvisited
    // -2 invalid
    this->idToClusterId.assign(dagNumVtxes, -1);

    for (uint32_t nid = 0; nid < dagNumVtxes; nid++) {
        if (!(dag->graph[nid].valid)) {
            this->idToClusterId[nid] = -2;
        }
    }

    // First, collect all clusters for all sink nodes
    for (auto& sink_vtx: dag->sinkNodes) {
        // starts from 0
        uint32_t cluster_id = this->clusters.size();
        if (cluster_id >= INT32_MAX) {
            BOOST_LOG_TRIVIAL(fatal) << "Cluster id too large";
            exit(-1);
        }
        this->clusters.emplace_back(std::vector<uint32_t>());
        assert(cluster_id + 1 == this->clusters.size());
        this->_collect_cluster_worker(cluster_id, sink_vtx);
    }

    // Collect clusters for all remaining nodes
    assert(this->idToClusterId.size() == dagNumVtxes);
    uint32_t cluster_seed = 0;
    while (true) {
        while ((this->idToClusterId[cluster_seed] != -1) && (cluster_seed < dagNumVtxes)) {
            cluster_seed ++;
        }

        if (cluster_seed >= dagNumVtxes) {
            break;
        }

        uint32_t cluster_id = this->clusters.size();
        if (cluster_id >= INT32_MAX) {
            BOOST_LOG_TRIVIAL(fatal) << "Cluster id too large";
            exit(-1);
        }
        this->clusters.emplace_back(std::vector<uint32_t>());
        this->_collect_cluster_worker(cluster_id, cluster_seed);
    }


    // It's guaranteed first nodes are sink nodes.
    for (uint32_t sink_cluster_id = 0; sink_cluster_id < dag->sinkNodes.size(); sink_cluster_id++) {
        this->sinkNodes.push_back(sink_cluster_id);
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Collect clusters: Done in " << time_ms << "ms";
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

void ClusterGraph::_update_cluster_cone() {
    BOOST_LOG_TRIVIAL(trace) << "Update cluster cones: Start";
    auto start = std::chrono::system_clock::now();

    assert(!this->cones_original_nodes.empty());
    assert(this->cones_cg_nodes.empty());

    for (auto& cone: this->cones_original_nodes) {
        std::unordered_set<uint32_t> cone_clusters;
        for (auto& nid: cone) {
            assert(dag->graph[nid].valid);
            auto node_cluster_id = this->idToClusterId[nid];
            assert(node_cluster_id >= 0);
            cone_clusters.insert(node_cluster_id);
        }
        std::vector<uint32_t> cone_clusters_vec;
        cone_clusters_vec.assign(cone_clusters.begin(), cone_clusters.end());
        this->cones_cg_nodes.push_back(std::move(cone_clusters_vec));
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

    // 2. collect cones
    this->_collect_cones();

    // 3. collect clusters
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

void ClusterGraph::constructParts(const int nparts, const std::vector<uint32_t>& _coneIdToPartId) {
    assert(_coneIdToPartId.size() == this -> cones_original_nodes.size());
    this -> coneIdToPartId = _coneIdToPartId;
    this -> partitions.clear();
    this -> partitions.assign(nparts, SBitSet());
    for (uint32_t cone_id = 0; cone_id < coneIdToPartId.size(); cone_id++) {
        uint32_t part_id = coneIdToPartId[cone_id];

        for (auto& cluster_id: this -> cones_cg_nodes[cone_id]) {
            this -> partitions[part_id].insert(cluster_id);
        }
    }
}

PartitionStatistics* ClusterGraph::reportPartitionStatus() {
    assert(!this -> partitions.empty());

    auto ret = new PartitionStatistics();

    ret -> nparts = this -> partitions.size();
    for (auto vtxes = boost::vertices(dag->graph); vtxes.first != vtxes.second; vtxes.first++) {
        auto v = *(vtxes.first);
        if (dag->graph[v].valid) {
            ret->sg_size++;
            ret->sg_weight += dag->graph[v].weight;
        }
    }


    uint32_t total_part_size = 0;
    float total_part_weight = 0;

    for (auto & partition : this -> partitions) {
        uint32_t part_size = 0;
        float part_weight = 0;

        auto part_clusters = partition.get_elems();
        for (auto& cid: *part_clusters) {
            part_size += this -> clusters[cid].size();
            part_weight += this -> graph[cid].weight;
        }
        delete part_clusters;

        total_part_size += part_size;
        total_part_weight += part_weight;

        ret -> partition_size.push_back(part_size);
        ret -> partition_weights.push_back(part_weight);
    }

    ret -> total_part_size = total_part_size;
    ret -> replication_size = (ret -> total_part_size) - (ret -> sg_size);
    ret -> replication_rate_size = static_cast<float>(static_cast<float>(ret -> replication_size) * 100.0 / (ret -> sg_size));
    ret -> ib_factor_size = calculate_ib_factor(ret -> partition_size);

    ret -> total_part_weight = total_part_weight;
    ret -> replication_weight = (ret -> total_part_weight) - (ret -> sg_weight);
    ret -> replication_rate_weight = static_cast<float>(static_cast<float>(ret -> replication_weight) * 100.0 / (ret -> sg_weight));
    ret -> ib_factor_weight = calculate_ib_factor(ret -> partition_weights);

    return ret;
}


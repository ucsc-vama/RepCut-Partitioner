//
// Created by Haoyuan Wang on 11/8/22.
//

#include "dag.h"
#include "rcp_common.h"

#include <cassert>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "cluster_graph.h"
#include "rcp_util.h"
#include <iostream>
#include <fstream>

#include <unordered_set>
#include <utility>
#include <thread>
#include <vector>

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
            if (verticesHasSameConeIds(vtx, seed)) {
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

    auto collect_cone_worker = [](const uint32_t seed, std::vector<uint32_t> &cone_node_vec, const RawGraph &graph) {
        std::unordered_set<uint32_t> cone_nodes;
        std::unordered_set<uint32_t> fringe, fringe_next;

        cone_nodes.reserve(1024);
        fringe.reserve(128);
        fringe_next.reserve(128);

        fringe.insert(seed);


        while(!fringe.empty()) {
            fringe_next.clear();
            for (auto vtx: fringe) {
                if (!cone_nodes.contains(vtx)) {
                    cone_nodes.insert(vtx);

                    for (auto inEdges = boost::in_edges(vtx, graph); inEdges.first != inEdges.second; inEdges.first++) {
                        auto nid = boost::source(*inEdges.first, graph);
                        if (graph[nid].valid) {
                            fringe_next.insert(nid);
                        }
                    }      
                }
            }

            std::swap(fringe, fringe_next);
        }

        cone_node_vec.reserve(cone_nodes.size());
        cone_node_vec.assign(cone_nodes.begin(), cone_nodes.end());
    };




    std::mutex write_back_lock;

    auto numSinkVtxes = dag->sinkNodes.size();
    assert(numSinkVtxes > 0);

    cones_original_nodes.resize(numSinkVtxes);



    const size_t chunk_size = 1000;
    std::atomic<size_t> next_index(0);
    std::vector<std::thread> threads;

    for (size_t t = 0; t < parallel_threads; ++t) {

        threads.emplace_back([&] {
            std::vector<std::vector<uint32_t>> results;
            results.resize(chunk_size);

            while (true) {
                size_t i = next_index.fetch_add(1);

                size_t start = i * chunk_size;
                size_t end = std::min(start + chunk_size, numSinkVtxes);

                if (start >= end) return;

                for (auto j = start; j < end; j++) {
                    auto seed = dag->sinkNodes.at(j);
                    auto result_i = j - start;
                    assert(result_i < chunk_size);
                    assert(results[result_i].size() == 0);
                    collect_cone_worker(seed, results[result_i], dag->graph);
                    assert(results[result_i].size() > 0);
                }

                write_back_lock.lock();
                for (auto j = start; j < end; j++) {
                    std::swap(cones_original_nodes[j], results[j - start]);
                }
                write_back_lock.unlock();
            }
        });
    }

    for (auto& t : threads) t.join();

    assert(cones_original_nodes.size() == dag->sinkNodes.size());


    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Collect cones: Done in " << time_ms << "ms";
}


//// Util function that simply add all coneId in a cone to vtxId
void ClusterGraph::insertConeIds(uint32_t coneId, const std::vector<uint32_t> &cone) {
    std::unordered_set<uint32_t> coneVtxStorageIds;
    std::unordered_map<uint32_t, uint32_t> coneVtxStorageIdOldToNew;
    std::unordered_map<uint32_t, uint32_t> coneVtxStorageIdUserCount;
    for (auto vtx: cone) {
        auto vtxConeIdStorage = idToConeIdStorage[vtx];
        coneVtxStorageIds.insert(vtxConeIdStorage);
        if (!coneVtxStorageIdUserCount.contains(vtxConeIdStorage)) {
            coneVtxStorageIdUserCount[vtxConeIdStorage] = 0;
        }
        coneVtxStorageIdUserCount[vtxConeIdStorage]++;
    }
    // create new cone id set
    for (const auto &ei: coneVtxStorageIds) {
        if (coneVtxStorageIdUserCount[ei] != idToConeIdsReferenceCount[ei]) {
            // need copy
            assert(!coneIdsStorage.contains(nextStorageId));
            coneIdsStorage[nextStorageId] = coneIdsStorage[ei];
            coneIdsStorage[nextStorageId].insert(coneId);
            coneVtxStorageIdOldToNew[ei] = nextStorageId;
            nextStorageId++;
        } else {
            // No copy needed
            coneIdsStorage[ei].insert(coneId);
            coneVtxStorageIdOldToNew[ei] = ei;
        }

    }
    for (auto vtx: cone) {
        auto oldPtr = idToConeIdStorage[vtx];
        auto newPtr = coneVtxStorageIdOldToNew[oldPtr];
        if (oldPtr != newPtr) {
            idToConeIdStorage[vtx] = newPtr;
            // update reference count
            assert(idToConeIdsReferenceCount[oldPtr] != 0);
            idToConeIdsReferenceCount[oldPtr] -= 1;
            idToConeIdsReferenceCount[newPtr] += 1;
        }

    }
    // release unused
    for (const auto &ei: coneVtxStorageIds) {
        if (idToConeIdsReferenceCount[ei] == 0) {
            // can be removed
            coneIdsStorage.erase(ei);
            idToConeIdsReferenceCount.erase(ei);
        }
    }
}
std::unordered_set<uint32_t>& ClusterGraph::getConeIds(uint32_t vtxId) {
    return coneIdsStorage[idToConeIdStorage[vtxId]];
}
bool ClusterGraph::verticesHasSameConeIds(uint32_t vtx1, uint32_t vtx2) {
    auto storageId1 = idToConeIdStorage[vtx1];
    auto storageId2 = idToConeIdStorage[vtx2];
    auto ret = storageId1 == storageId2;
//    auto ret2 = coneIdsStorage[storageId1] == coneIdsStorage[storageId2];
//    assert(ret == ret2);
    return ret;
}



void ClusterGraph::_collect_clusters() {
    BOOST_LOG_TRIVIAL(trace) << "Collect clusters: Start";
    auto start = std::chrono::system_clock::now();

    // mark cone id
    auto dagNumVtxes = boost::num_vertices(dag->graph);

    coneIdsStorage[0] = {};
    idToConeIdStorage.resize(dagNumVtxes, 0);
    idToConeIdsReferenceCount[0] = dagNumVtxes;
    auto numCones = this->cones_original_nodes.size();

    for (uint32_t cid = 0; cid < numCones; cid++) {
        insertConeIds(cid, this->cones_original_nodes[cid]);

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

    for (const auto& cluster: clusters) {
        assert(!cluster.empty());
        auto pins = getConeIds(cluster[0]);
        assert(!pins.empty());
        clusterIdToPins.push_back(pins);
    }
//    idToConeId.clear();
//    idToConeId.resize(0);
    coneIdsStorage.clear();
    coneIdsStorage.rehash(0);
    idToConeIdsReferenceCount.clear();
    idToConeIdsReferenceCount.rehash(0);
    idToConeIdStorage.clear();
    idToConeIdStorage.resize(0);

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

    // v1: 2mega, 40:19s, 14253MB
    // v2: 2mega, 22:39, 7335MB

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


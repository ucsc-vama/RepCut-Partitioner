//
// Created by Haoyuan Wang on 11/8/22.
//

#include "rcp_common.h"
#include "commandline_options.h"

#include <chrono>
#include <algorithm>
#include <numeric>

#include "cluster_graph.h"
#include "rcp_util.h"
#include <iostream>
#include <fstream>

// Recursively collect cones
void ClusterGraph::_collect_cone_worker(std::unordered_map<uint32_t, std::vector<uint32_t>>& cache, uint32_t seed) {
    if (!cache.contains(seed)) {
        //
        std::unordered_set<uint32_t> dep_nodes;
        dep_nodes.insert(seed);
        for (auto& nid: dag->inNeigh[seed]) {
            if (dag->nodeValid[nid]) {
                // for all valid nodes
                _collect_cone_worker(cache, nid);
                dep_nodes.insert(cache[nid].begin(), cache[nid].end());
            }
        }
        std::vector<uint32_t> dep_nodes_vec;
        dep_nodes_vec.assign(dep_nodes.begin(), dep_nodes.end());
        cache[seed] = std::move(dep_nodes_vec);
    }
}

// Recursively collect cluster
void ClusterGraph::_collect_cluster_worker(uint32_t cluster_id, uint32_t seed) {

    if (this->idToClusterId[seed] == -1) {
        // unvisited
        this->clusters[cluster_id].push_back(seed);
        this->idToClusterId[seed] = static_cast<int32_t>(cluster_id);

        std::vector<uint32_t> connected_vtxs;
        for (auto& vtx: dag->inNeigh[seed]) {
            if (this->idToClusterId[vtx] == -1) {
                connected_vtxs.push_back(vtx);
            }
        }
        for (auto& vtx: dag->outNeigh[seed]) {
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

    std::unordered_map<uint32_t, std::vector<uint32_t>> cone_cache;
    for (auto& cone_seed: dag->sinkNodes) {
        _collect_cone_worker(cone_cache, cone_seed);
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
    this->idToConeId.assign(dag->numNodes, std::unordered_set<uint32_t>());

    for (uint32_t cid = 0; cid < this->cones_original_nodes.size(); cid++) {
        for (auto& nid: this->cones_original_nodes[cid]) {
            this->idToConeId[nid].insert(cid);
        }
    }

    // init this->idToClusterId
    // -1: unvisited
    // -2 invalid
    this->idToClusterId.assign(dag->numNodes, -1);

    for (uint32_t nid = 0; nid < dag-> numNodes; nid++) {
        if (!(dag->nodeValid[nid])) {
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
    assert(this->idToClusterId.size() == dag->numNodes);
    uint32_t cluster_seed = 0;
    while (true) {
        while ((this->idToClusterId[cluster_seed] != -1) && (cluster_seed < dag->numNodes)) {
            cluster_seed ++;
        }

        if (cluster_seed >= dag->numNodes) {
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

    // numEdge: Cannot determine yet
    this->numEdges = 0;
    this->numNodes = this->clusters.size();
    this->inNeigh.assign(this->numNodes, std::vector<uint32_t>());
    this->outNeigh.assign(this->numNodes, std::vector<uint32_t>());
    this->weight.assign(this->numNodes, 0);

    for (uint32_t cluster_id = 0; cluster_id < this->clusters.size(); cluster_id++) {
        //
        std::unordered_set<uint32_t> cluster_outNeighs;

        for (auto &nid: this->clusters[cluster_id]) {
            // save all outNeighs
            cluster_outNeighs.insert(dag->outNeigh[nid].begin(), dag->outNeigh[nid].end());
        }

        for (auto &nid: this->clusters[cluster_id]) {
            // remove all inNeighs
            for (auto &inNeigh_id: dag->inNeigh[nid]) {
                if (cluster_outNeighs.contains(inNeigh_id)) {
                    cluster_outNeighs.erase(inNeigh_id);
                }
            }
        }

        // Assert: if this cluster is confirmed to be a sink cluster,
        // it must exist in this->sinkNodes
        if (cluster_outNeighs.empty()) {
            assert(std::find(this->sinkNodes.begin(), this->sinkNodes.end(), cluster_id) != this->sinkNodes.end());} else {
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
            // build outNeigh
            this->outNeigh[cluster_id].assign(descendent_clusters.begin(), descendent_clusters.end());
        }

        // Last step: Build inNeight from outNeigh
        for (auto &dst_cid: this->outNeigh[cluster_id]) {
            this->inNeigh[dst_cid].push_back(cluster_id);
            this->numEdges ++;
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

    for (int cluster_id = 0; cluster_id < this->numNodes; cluster_id++) {
        // Weight starts at 1 to make KaHyPar happy
        float cluster_weight = 1;

        for (auto& stmt_id: this->clusters[cluster_id]) {
            if (dag->nodeValid[stmt_id]) {
                // For every valid nodes, weight must >= 0
                assert(dag->weight[stmt_id] >= 0);
                cluster_weight += dag->weight[stmt_id];
            }
        }

        this->weight[cluster_id] = cluster_weight;
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
            assert(dag->nodeValid[nid]);
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


void ClusterGraph::collapseFromDAG(const DirectedAcyclicGraph *dag) {
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

void ClusterGraph::constructParts(const std::vector<uint32_t>& _coneIdToPartId) {
    assert(_coneIdToPartId.size() == this -> cones_original_nodes.size());
    this -> coneIdToPartId = _coneIdToPartId;
    this -> partitions.clear();
    this -> partitions.assign(opts.nparts, SBitSet());
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
    ret -> sg_size = std::count_if(dag->nodeValid.begin(), dag->nodeValid.end(), [](bool in) {return in;});
    ret -> sg_weight = std::accumulate(this -> weight.begin(), this -> weight.end(), static_cast<float>(0));


    uint32_t total_part_size = 0;
    float total_part_weight = 0;

    for (auto & partition : this -> partitions) {
        uint32_t part_size = 0;
        float part_weight = 0;

        auto part_clusters = partition.get_elems();
        for (auto& cid: *part_clusters) {
            part_size += this -> clusters[cid].size();
            part_weight += this -> weight[cid];
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

void ClusterGraph::saveToFile(const char *filename) {
    auto ofs = std::ofstream(opts.work_directory / filename);
    for (auto pid: this -> coneIdToPartId) {
        ofs << pid << "\n";
    }
    ofs.close();
}

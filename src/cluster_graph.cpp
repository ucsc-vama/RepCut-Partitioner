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
    // this->idToConeId.insert(this->idToConeId.end(), dag->numNodes, std::set<uint32_t>());
    this->idToConeId.assign(dag->numNodes, std::set<uint32_t>());

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
        std::set<uint32_t> cluster_outNeighs;

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
            assert(std::find(this->sinkNodes.begin(), this->sinkNodes.end(), cluster_id) != this->sinkNodes.end());
        } else {
            // Has descendent(s)
            std::set<uint32_t> descendent_clusters;
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

void ClusterGraph::constructParts(const std::vector<uint32_t>& coneIdToPartId) {
    assert(coneIdToPartId.size() == this -> cones_original_nodes.size());
    this -> partitions.clear();
    this -> partitions.assign(opts.nparts, SBitSet());
    for (uint32_t cone_id = 0; cone_id < coneIdToPartId.size(); cone_id++) {
        uint32_t part_id = coneIdToPartId[cone_id];

        for (auto& cluster_id: this -> cones_cg_nodes[cone_id]) {
            this -> partitions[part_id].insert(cluster_id);
        }
    }
}

void ClusterGraph::reportPartitionStatus() {
    assert(!this -> partitions.empty());

    uint32_t sg_size = this -> idToClusterId.size();
    uint32_t sg_valid_size = std::count_if(dag->nodeValid.begin(), dag->nodeValid.end(), [](bool in) {return in;});
    float sg_weight = std::accumulate(this -> weight.begin(), this -> weight.end(), static_cast<float>(0));

    uint32_t total_part_size = 0;
    float total_part_weight = 0;

    std::vector<uint32_t> v_part_size;
    std::vector<float> v_part_weight;

    for (uint32_t pid = 0; pid < this -> partitions.size(); pid++) {
        uint32_t part_size = 0;
        float part_weight = 0;

        auto part_clusters = this -> partitions[pid].get_elems();
        for (auto& cid: *part_clusters) {
            part_size += this -> clusters[cid].size();
            part_weight += this -> weight[cid];
        }
        delete part_clusters;

        total_part_size += part_size;
        total_part_weight += part_weight;
        v_part_size.push_back(part_size);
        v_part_weight.push_back(part_weight);

        std::cout << "Pid: " << pid << ", part size: " << part_size << ", part weight: " << part_weight << "\n";
    }

    std::cout << "Total node count is " << total_part_size << ", original statement graph has " << sg_valid_size << " valid nodes" << "\n";
    std::cout << "Total node counts (whole design) is " << sg_size << "\n";

    uint32_t duplicated_stmts = total_part_size - sg_valid_size;
    uint32_t part_size_max = *std::max_element(v_part_size.begin(), v_part_size.end());
    uint32_t part_size_min = *std::min_element(v_part_size.begin(), v_part_size.end());
    std::cout << "Duplication stmt cost: " << duplicated_stmts << " (" << (static_cast<float>(duplicated_stmts) * 100.0 / sg_valid_size) << "%)\n";
    std::cout << "Partition size: max: " << part_size_max << ", min: " << part_size_min << ", avg: " << total_part_size / v_part_size.size() << "\n";

    float duplicated_weights = total_part_weight - sg_weight;
    float part_weight_max = *std::max_element(v_part_weight.begin(), v_part_weight.end());
    float part_weight_min = *std::min_element(v_part_weight.begin(), v_part_weight.end());
    std::cout << "Total node weight (whole design) is " << sg_weight << "\n";
    std::cout << "Duplication weight cost: " << duplicated_weights << " (" << (static_cast<float>(duplicated_weights) * 100.0 / sg_weight) << "%)\n";
    std::cout << "Partition weight: max: " << part_weight_max << ", min: " << part_weight_min << ", avg: " << total_part_weight / v_part_weight.size() << std::endl;

    std::cout << "Weight ib factor: " << calculate_ib_factor(v_part_weight) << std::endl;
}
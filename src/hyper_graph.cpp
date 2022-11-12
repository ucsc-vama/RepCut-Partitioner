
#include "rcp_common.h"
#include "hyper_graph.h"

#include <fstream>
#include <boost/algorithm/string.hpp>

void HyperGraph::addNode(uint32_t node_id, uint32_t node_weight) {
    assert(node_weight > 0);
    if (node_id >= this -> nodes.size()) {
        // Grow space
        uint32_t space_to_grow = node_id - this -> nodes.size() + 1;
        this -> nodes.insert(this -> nodes.end(), space_to_grow, std::vector<uint32_t>());
        this -> nodeWeight.insert(this -> nodeWeight.end(), space_to_grow, -1);
    }
    this -> nodeWeight[node_id] = node_weight;
}

void HyperGraph::addEdge(const std::vector<uint32_t>& edge, uint32_t edge_weight) {
    assert(edge_weight > 0);
    assert(!edge.empty());

    uint32_t edge_id = this -> edges.size();
    // Copy
    this -> edges.push_back(edge);
    this -> edgeWeight.push_back(edge_weight);

    for (auto& node_id: edge) {
        this -> nodes[node_id].push_back(edge_id);
    }
}


void HyperGraph::buildFromClusterGraph(const ClusterGraph *cg) {
    BOOST_LOG_TRIVIAL(trace) << "Build hyper graph: Start";
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> hePinCount;
    for (auto& cluster: cg -> clusters) {
        assert(!cluster.empty());
        uint32_t pin_count = cg -> idToConeId[cluster[0]].size();
        assert(pin_count > 0);
        hePinCount.push_back(pin_count);
    }

    // Add nodes
    assert(cg -> cones_original_nodes.size() == cg -> cones_cg_nodes.size());
    for (uint32_t cone_id = 0; cone_id < cg -> cones_cg_nodes.size(); cone_id++) {
        uint32_t cone_weight = cg -> weight[cone_id];
        std::unordered_set<uint32_t> connected_clusters;
        for (auto& cluster_id: cg -> cones_cg_nodes[cone_id]) {
            if (cluster_id != cone_id) {
                connected_clusters.insert(cluster_id);
            }
        }

        uint32_t connected_cluster_weights = 0;
        for (auto& cluster_id: connected_clusters) {
            auto pin_count = hePinCount[cluster_id];
            auto cluster_weight = cg -> weight[cluster_id];
            connected_cluster_weights += (cluster_weight / pin_count);
        }

        uint32_t node_weight = cone_weight + connected_cluster_weights;
        this ->addNode(cone_id, node_weight);
    }

    // Add edges
    assert(cg -> cones_cg_nodes.size() <= cg -> clusters.size());
    for (uint32_t cluster_id = cg -> cones_cg_nodes.size(); cluster_id < cg -> clusters.size(); cluster_id++) {
        uint32_t edge_weight = cg -> weight[cluster_id];
        uint32_t cone_idx = cg -> clusters[cluster_id][0];
        std::vector<uint32_t> pins;
        for (auto& pin: cg -> idToConeId[cone_idx]) {
            pins.push_back(pin);
        }
        this ->addEdge(pins, edge_weight);
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Build hyper graph: Done in " << time_ms << "ms";
}

void HyperGraph::writeTohMetisFile(const char* filename) {
    BOOST_LOG_TRIVIAL(trace) << "Write to hMetis: Start";
    auto start = std::chrono::system_clock::now();

    std::ofstream ofs(filename);

    // write header
    ofs << this -> edges.size() << " " << this -> nodes.size() << " 11\n";

    // write edges
    for (uint32_t edge_id = 0; edge_id < this -> edges.size(); edge_id++) {
        std::vector<std::string> tokens;
        uint32_t edge_weight = this -> edgeWeight[edge_id];
        tokens.push_back(std::to_string(edge_weight));
        // Note: node id + 1 to make KaHyPar happy
        std::for_each(this -> edges[edge_id].begin(), this -> edges[edge_id].end(),
                      [&] (uint32_t nid) {tokens.push_back(std::to_string(nid + 1));});
        std::string line = boost::algorithm::join(tokens, " ");
        ofs << line << "\n";
    }

    // write nodes
    std::for_each(this -> nodeWeight.begin(), this -> nodeWeight.end(),
                  [&](uint32_t weight) {ofs << weight << "\n";});

    // Done
    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Write to hMetis: Done in " << time_ms << "ms";
}

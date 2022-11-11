//
// Created by Haoyuan Wang on 11/11/22.
//

#ifndef RCP_HYPER_GRAPH_H
#define RCP_HYPER_GRAPH_H

#include "rcp_common.h"
#include "cluster_graph.h"

class HyperGraph {
public:
    std::vector<std::vector<uint32_t>> nodes;
    std::vector<std::vector<uint32_t>> edges;

    // Weight should be > 0. A weight < 0 is invalid
    // Weight == 0 is not supported by KaHyPar
    std::vector<uint32_t> nodeWeight;
    std::vector<uint32_t> edgeWeight;

    void addNode(uint32_t node_id, uint32_t node_weight);

    void addEdge(const std::vector<uint32_t>& edge, uint32_t edge_weight);

    void buildFromClusterGraph(const ClusterGraph* cg);

    void writeTohMetisFile(const char* filename);
};


#endif //RCP_HYPER_GRAPH_H

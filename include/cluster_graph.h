//
// Created by Haoyuan Wang on 11/8/22.
//

#ifndef RCP_COLLAPSE_GRAPH_H
#define RCP_COLLAPSE_GRAPH_H

#include "rcp_common.h"

#include "dag.h"
#include "SBitSet.h"

class ClusterGraph : public DirectedAcyclicGraph {
private:
    void _collect_cone_worker(const DirectedAcyclicGraph* dag, std::unordered_map<uint32_t, std::vector<uint32_t>>& cache, uint32_t seed);
    void _collect_cluster_worker(const DirectedAcyclicGraph* dag, uint32_t cluster_id, uint32_t seed);

    void _collect_cones(const DirectedAcyclicGraph* dag);
    void _collect_clusters(const DirectedAcyclicGraph* dag);

    void _build_cluster_graph(const DirectedAcyclicGraph* dag);
    void _update_cluster_weight(const DirectedAcyclicGraph* dag);

    void _update_cluster_cone(const DirectedAcyclicGraph* dag);
public:
    // Cone nodes in Stmt DAG
    std::vector<std::vector<uint32_t>> cones_original_nodes;
    // Cone nodes in Cluster Graph
    std::vector<std::vector<uint32_t>> cones_cg_nodes;
    // Clusters should be non-overlapping
    std::vector<std::vector<uint32_t>> clusters;

    // -1: unvisited
    // -2 invalid
    std::vector<int32_t> idToClusterId;
    std::vector<std::set<uint32_t>> idToConeId;

    void collapseFromDAG(const DirectedAcyclicGraph* dag);
};

#endif //RCP_COLLAPSE_GRAPH_H

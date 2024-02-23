//
// Created by Haoyuan Wang on 11/8/22.
//

#ifndef RCP_COLLAPSE_GRAPH_H
#define RCP_COLLAPSE_GRAPH_H

#include "rcp_common.h"

#include "dag.h"
#include "SBitSet.h"

#include "partition_stat.h"

namespace repcut {
    class ClusterGraph : public DirectedAcyclicGraph {
    private:
        void _collect_cone_worker(std::unordered_map<uint32_t, std::vector<uint32_t>>& cache, uint32_t seed);
        void _collect_cluster_worker(uint32_t cluster_id, uint32_t seed);

        void _collect_cones();
        void _collect_clusters();

        void _build_cluster_graph();
        void _update_cluster_weight();

        void _update_cluster_cone();
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
        std::vector<std::unordered_set<uint32_t>> idToConeId;

        std::vector<SBitSet> partitions;
        std::vector<uint32_t> coneIdToPartId;

        const DirectedAcyclicGraph* dag = nullptr;

        // Cluster weight:
        // std::vector<uint32_t> weight;

        void collapseFromDAG(const DirectedAcyclicGraph* dag);

        void constructParts(const int nparts, const std::vector<uint32_t>& coneIdToPartId);

        PartitionStatistics* reportPartitionStatus();

    };
}




#endif //RCP_COLLAPSE_GRAPH_H

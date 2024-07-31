//
// Created by Haoyuan Wang on 11/8/22.
//

#ifndef RCP_COLLAPSE_GRAPH_H
#define RCP_COLLAPSE_GRAPH_H

#include "rcp_common.h"

#include "dag.h"
#include "SBitSet.h"

#include "partition_stat.h"
#include <memory>

namespace repcut {
    class ClusterGraph {
    private:
        void _collect_cluster_worker(uint32_t cluster_id, uint32_t seed);

        void _collect_cones();
        void _collect_clusters();

        void _build_cluster_graph();
        void _update_cluster_weight();

        void _update_cluster_cone();


        // Two level storage to reduce memory needs
        // idToConeIds stores real set of cone ids
        // while idToConeIdStorage keeps index to idToConeIds
        std::unordered_map<uint32_t, std::unordered_set<uint32_t>> coneIdsStorage;
        std::unordered_map<uint32_t, uint32_t> idToConeIdsReferenceCount;
        std::vector<uint32_t> idToConeIdStorage;
        uint32_t nextStorageId = 1;
        // Util function that simply add all coneId in a cone to vtxId
        void insertConeIds(uint32_t coneId, const std::vector<uint32_t> &cone);
        std::unordered_set<uint32_t>& getConeIds(uint32_t vtxId);
        bool verticesHasSameConeIds(uint32_t vtx1, uint32_t vtx2);
    public:
        RawGraph graph;
        std::vector<uint32_t> sinkNodes;

        // Cone nodes in Stmt DAG
        std::vector<std::vector<uint32_t>> cones_original_nodes;
        // Cone nodes in Cluster Graph
        std::vector<std::vector<uint32_t>> cones_cg_nodes;
        // Clusters should be non-overlapping
        std::vector<std::vector<uint32_t>> clusters;

        // -1: unvisited
        // -2 invalid
        std::vector<int32_t> idToClusterId;



        std::vector<SBitSet> partitions;
        std::vector<uint32_t> coneIdToPartId;

        DirectedAcyclicGraph* dag = nullptr;

        std::vector<std::unordered_set<uint32_t>> clusterIdToPins;

        // Cluster weight:
        // std::vector<uint32_t> weight;

        void collapseFromDAG(DirectedAcyclicGraph* dag);

        void constructParts(const int nparts, const std::vector<uint32_t>& coneIdToPartId);

        PartitionStatistics* reportPartitionStatus();

    };
}




#endif //RCP_COLLAPSE_GRAPH_H

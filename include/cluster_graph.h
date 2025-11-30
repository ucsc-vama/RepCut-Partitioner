//
// Created by Haoyuan Wang on 11/8/22.
//

#ifndef RCP_COLLAPSE_GRAPH_H
#define RCP_COLLAPSE_GRAPH_H

#include "rcp_common.h"

#include "dag.h"
#include "SBitSet.h"

#include "partition_stat.h"
#include "cone_trie.h"

#include <memory>

namespace repcut {
    class ClusterGraph {
    private:
        // Build the persistent cone-id trie and per-vertex trie pointers.
        // Replaces the old `_collect_cones` materialization of cones.
        void _mark_cones();
        void _collect_clusters();
        void _build_cluster_graph();
        void _update_cluster_weight();
        void _update_cluster_cone();

        // Persistent trie mapping each vertex to the leaf whose root-to-leaf
        // path is its (sorted, unique) cone-id set.  Vertices belonging to the
        // same set of cones share a leaf, so set equality is pointer equality.
        std::unique_ptr<ConeTrie> coneTrie;
        std::vector<ConeTrie::Node*> vtxToNode;

    public:
        RawGraph graph;
        std::vector<uint32_t> sinkNodes;

        // Cone nodes in Cluster Graph (original cones are not retained).
        std::vector<std::vector<uint32_t>> cones_cg_nodes;
        // Clusters should be non-overlapping
        std::vector<std::vector<uint32_t>> clusters;

        // -1: unvisited
        // -2 invalid
        std::vector<int32_t> idToClusterId;

        std::vector<SBitSet> partitions;
        std::vector<uint32_t> coneIdToPartId;

        DirectedAcyclicGraph* dag = nullptr;

        // Pins per cluster: the (sorted, unique) set of cone ids touching the
        // cluster.  Stored as a sorted vector of cone ids (derived from the
        // trie path), which is more compact than an unordered_set.
        std::vector<std::vector<uint32_t>> clusterIdToPins;

        // Cluster weight:
        // std::vector<uint32_t> weight;
        uint32_t parallel_threads = 1;

        void collapseFromDAG(DirectedAcyclicGraph* dag);

        void constructParts(const int nparts, const std::vector<uint32_t>& coneIdToPartId);

        PartitionStatistics* reportPartitionStatus();

    };
}




#endif //RCP_COLLAPSE_GRAPH_H
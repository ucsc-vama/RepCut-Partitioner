#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

#include "DAG.h"
#include "repcut.h"

#include "ConeTrie.h"

namespace repcut {
    // Intermediate structure: DAG of clusters (groups sharing the same cone
    // set).  writeHMetisFile derives the intersection hypergraph from this.
    class ClusterGraph
    {
    private:
        // Collapse phases.  See collapseFromDAG for the orchestration.
        void _mark_cones();
        void _collect_clusters();
        void _update_cluster_weight();
        void _update_cluster_cone();

        // Trie mapping vertex -> leaf with its cone-id set; equality = pointer equality.
        std::unique_ptr<ConeTrie> coneTrie;
        std::vector<ConeTrie::Node*> vtxToNode;

    public:
        std::vector<uint32_t> sinkNodes;

        // Cone nodes in Cluster Graph (original cones are not retained).
        std::vector<std::vector<uint32_t>> cones_cg_nodes;
        // Clusters should be non-overlapping
        std::vector<std::vector<uint32_t>> clusters;

        // -1: unvisited
        // -2 invalid
        std::vector<int32_t> idToClusterId;

        // Non-owning DAG pointer (set once, must not outlive the DAG).
        const DirectedAcyclicGraph* dag = nullptr;

        // Sorted unique cone ids per cluster, derived from the trie path.
        std::vector<std::vector<uint32_t>> clusterIdToPins;

        // Per-cluster weight (float).
        std::vector<float> nodeWeight;

        uint32_t parallel_threads = 1;

        // Log threshold carried from RepCutContext.
        RepCutLogLevel log_level = REPCUT_LOG_SILENT;

        void collapseFromDAG(const DirectedAcyclicGraph& dag);

        // Write hMetis-format file (nodes=cone clusters, edges=non-cone clusters).
        void writeHMetisFile(const char* filename);
    };
} // namespace repcut

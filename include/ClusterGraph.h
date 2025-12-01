//
// Created by Haoyuan Wang on 11/8/22.
//

#ifndef RCP_COLLAPSE_GRAPH_H
#define RCP_COLLAPSE_GRAPH_H

#include <cstdint>
#include <memory>
#include <vector>

#include "DAG.h"

#include "ConeTrie.h"

namespace repcut {
    // The ClusterGraph is the intermediate structure from RepCut's algorithm
    // (§ "Generating the Intersection Hypergraph" of the ASPLOS'23 paper): a
    // DAG whose vertices are clusters (groups of DAG vertices sharing the
    // same set of cones) and whose edges are inter-cluster dependencies.
    // `writeHMetisFile` derives the intersection hypergraph from this graph
    // and writes the hMetis input for MtKaHyPar; the cluster graph itself is
    // not a hypergraph.
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

        // Non-owning borrow of the design DAG.  Set once by collapseFromDAG
        // and only read thereafter (const-correctness enforced via
        // pointer-to-const).  Not a smart pointer because ClusterGraph does
        // not own (and must not outlive) the DAG.
        const DirectedAcyclicGraph* dag = nullptr;

        // Pins per cluster: the (sorted, unique) set of cone ids touching the
        // cluster.  Stored as a sorted vector of cone ids (derived from the
        // trie path), which is more compact than an unordered_set.
        std::vector<std::vector<uint32_t>> clusterIdToPins;

        // Cluster weight:
        // std::vector<uint32_t> weight;
        uint32_t parallel_threads = 1;

        void collapseFromDAG(const DirectedAcyclicGraph& dag);

        // Stream the cluster graph to an hMetis-format file, directly from
        // the cluster graph's fields.  This replaces the intermediate
        // HyperGraph buffer that was built only to be serialized.  Byte
        // output matches HyperGraph::writeTohMetisFile:
        //   - hypergraph nodes = cone clusters (cone_id in [0, numCones))
        //   - hypergraph edges = non-cone clusters (cluster_id in [numCones, ...))
        //   - node/edge weights derived from graph[cid].weight (floor cast)
        //   - pin list per edge = clusterIdToPins[cid] (already sorted ascending)
        //   - pin ids written 1-indexed (hMetis/KaHyPar convention)
        void writeHMetisFile(const char* filename);
    };
}




#endif //RCP_COLLAPSE_GRAPH_H
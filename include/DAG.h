#pragma once

#include <cstdint>
#include <vector>

namespace repcut {

    // Flat representation of the design DAG.  Replaces the earlier
    // boost::adjacency_list<bidirectionalS> storage with plain vectors.
    // The graph is append-only during `buildFromFile` and frozen thereafter,
    // so a CSR-like layout is sufficient and far lighter than BGL.
    class DirectedAcyclicGraph {
    public:
        // Per-vertex properties, indexed by vertex id.
        std::vector<float> weight;       // numVertices; 0 for invalid nodes
        std::vector<bool>  valid;        // numVertices

        // Adjacency, indexed by vertex id.  Both directions are kept because
        // cone-marking and reconstruction walk predecessors (inNeigh) while
        // cluster flooding needs both directions for connected-component
        // grouping.  Insertion order in these vectors matches the order edges
        // appear in the input file, which is deterministic.
        std::vector<std::vector<uint32_t>> inNeigh;
        std::vector<std::vector<uint32_t>> outNeigh;

        // Valid vertices with empty outNeigh, populated by findSinkNodes().
        std::vector<uint32_t> sinkNodes;

        void buildFromFile(const char* filename);
        void findSinkNodes();

        size_t numVertices() const { return weight.size(); }
    };
}
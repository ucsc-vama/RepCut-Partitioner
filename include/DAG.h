#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "repcut.h"

namespace repcut {

    // Flat CSR-like representation of the design DAG.
    class DirectedAcyclicGraph
    {
    public:
        // Per-vertex properties, indexed by vertex id.
        std::vector<float> weight; // numVertices; 0 for invalid nodes
        std::vector<bool> valid;   // numVertices

        // Adjacency (both directions kept for cone-marking and flood fill).
        std::vector<std::vector<uint32_t>> inNeigh;
        std::vector<std::vector<uint32_t>> outNeigh;

        // Valid vertices with empty outNeigh, populated by findSinkNodes().
        std::vector<uint32_t> sinkNodes;

        // Log threshold carried from RepCutContext.  Defaults to SILENT.
        RepCutLogLevel log_level = REPCUT_LOG_SILENT;

        // Parse the graph file into this DAG.  Returns true on success,
        // false on malformed input (with an error logged via rcp_log).
        bool buildFromFile(const char* filename);

        void findSinkNodes();

        size_t numVertices() const { return weight.size(); }
    };
} // namespace repcut

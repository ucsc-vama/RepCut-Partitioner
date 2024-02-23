#pragma once

#include "rcp_common.h"

namespace repcut {
    class DirectedAcyclicGraph {
    public:
        uint32_t numNodes = 0;
        uint32_t numEdges = 0;

        std::vector<float> weight;

        std::vector<bool> nodeValid;

        std::vector<uint32_t> sinkNodes;

        std::vector<std::string> node_stmts;

        std::vector<std::vector<uint32_t>> inNeigh;
        std::vector<std::vector<uint32_t>> outNeigh;


        void buildFromFile(const char* filename);

        bool checkCorrectness();

        void findSinkNodes();

    };
}

#pragma once

#include "rcp_common.h"
#include <boost/graph/adjacency_list.hpp>

namespace repcut {

    struct RawGraphNodeProperty {
    public:
        float weight;
        bool valid;
        std::string stmt;
    };

    // Note: use boost::vecS (std::vector) to ensure vertex_descriptor is integer and also incremental

    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, RawGraphNodeProperty, boost::no_property, boost::no_property, boost::listS> RawGraph;

    class DirectedAcyclicGraph {
    public:
        RawGraph graph;

        std::vector<uint32_t> sinkNodes;

        void buildFromFile(const char* filename);

        void findSinkNodes();

    };
}

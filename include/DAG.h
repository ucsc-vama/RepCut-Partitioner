#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

namespace repcut {

    struct RawGraphNodeProperty {
    public:
        float weight;
        bool valid;
        // The source statement label parsed from the graph file.  Kept for
        // debugging only (e.g. dumping a node's identity); never read by the
        // partitioning pipeline.  Commented out by default because retaining
        // a std::string per node on designs with millions of vertices is a
        // substantial memory cost with no runtime benefit.
        // std::string stmt;
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
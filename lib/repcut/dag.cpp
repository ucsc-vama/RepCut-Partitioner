
#include "dag.h"
#include <fstream>
#include <string>

#include <stdexcept>
#include <iostream>
#include <chrono>

#include <boost/algorithm/string.hpp>

using namespace repcut;

void DirectedAcyclicGraph::buildFromFile(const char *filename) {
    BOOST_LOG_TRIVIAL(trace) << "Build DAG from file: Start";
    auto start = std::chrono::system_clock::now();

    std::ifstream file(filename);
    std::string line;
    uint32_t lineno = 0;

    std::vector<std::string> split_line;

    while (std::getline(file, line))
    {
        split_line.clear();

        boost::split(split_line, line, boost::is_any_of(" "));


        if (lineno == 0) {
            // header

            for (auto & i : split_line) {
                boost::trim(i);
            }

            if (split_line.size() != 2) {
                BOOST_LOG_TRIVIAL(fatal) << "Incorrect header at line " << lineno << ": " << line;
                exit(-1);
            }

//            size_t numEdges = std::stoi(split_line[0]);
            size_t numNodes = std::stoi(split_line[1]);

            for (std::size_t i = 0; i < numNodes; ++i) {
                auto newVtx = boost::add_vertex(graph);
                assert(newVtx == i);
            }
        } else{
            // normal line
            if (split_line.size() < 2) {
                BOOST_LOG_TRIVIAL(fatal) << "Too few token(s) in line " << lineno << ": " << line;
                exit(-1);
            }

            for (uint32_t i = 1; i < split_line.size(); i++) {
                boost::trim(split_line[i]);
            }

            uint32_t node_id = lineno - 1;

            RawGraphNodeProperty vp;
            vp.weight = std::stof(split_line[1]);
            // vp.stmt = split_line[0];  // debug only; see RawGraphNodeProperty

            if (vp.weight < 0) {
                // An invalid node
                vp.weight = 0;
                vp.valid = false;
            } else {
                vp.valid = true;
                // has edge(s)
                if (split_line.size() > 1) {
                    for (uint32_t i = 2; i < split_line.size(); ++i) {
                        uint32_t num = std::stoi(split_line[i]);
                        if (num < 0) {
                            BOOST_LOG_TRIVIAL(fatal) << "Node ID must be 0 or positive integer: Line " << lineno;
                            exit(-1);
                        }
                        uint32_t dst_node = num;
                        // New edge: node_id -> dst_node
                        boost::add_edge(node_id, dst_node, graph);
                    }
                }
            }
            graph[node_id] = vp;
        }

        lineno ++;
    }


    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Build DAG from file: Done in " << time_ms << "ms";

}



void DirectedAcyclicGraph::findSinkNodes() {
    BOOST_LOG_TRIVIAL(trace) << "Find all sink vtxs: Start";
    auto start = std::chrono::system_clock::now();

    auto numNodes = boost::num_vertices(graph);
    for (uint32_t nid = 0; nid < numNodes; nid++) {
        if (graph[nid].valid) {
            // for all valid nodes
            if (boost::out_degree(nid, graph) == 0) {
                // No out edges. This is a sink node
                sinkNodes.push_back(nid);
            }
        }
    }
    BOOST_LOG_TRIVIAL(debug) << "Found " << sinkNodes.size() << " sink nodes\n";

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Find all sink vtxs: Done in " << time_ms << "ms";
}

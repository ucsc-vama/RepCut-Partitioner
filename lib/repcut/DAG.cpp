#include "DAG.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>

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

            // Reserve capacity for all per-vertex storage up front.  We do not
            // allocate the adjacency sub-vectors themselves (they grow lazily),
            // but reserving the top-level slots avoids reallocation/copy of the
            // outer vector as vertices stream in.
            weight.resize(numNodes);
            valid.resize(numNodes);
            inNeigh.resize(numNodes);
            outNeigh.resize(numNodes);
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

            float node_weight = std::stof(split_line[1]);
            // vp.stmt = split_line[0];  // debug only

            if (node_weight < 0) {
                // An invalid node
                weight[node_id] = 0;
                valid[node_id] = false;
            } else {
                weight[node_id] = node_weight;
                valid[node_id] = true;
                // Edges (if any) are tokens [2, split_line.size()).  Reserve
                // the out-neighbor vector up front so push_back never
                // reallocates mid-line.
                if (split_line.size() > 2) {
                    outNeigh[node_id].reserve(split_line.size() - 2);
                    for (uint32_t i = 2; i < split_line.size(); ++i) {
                        int num = std::stoi(split_line[i]);
                        if (num < 0) {
                            BOOST_LOG_TRIVIAL(fatal) << "Node ID must be 0 or positive integer: Line " << lineno;
                            exit(-1);
                        }
                        uint32_t dst_node = static_cast<uint32_t>(num);
                        // New edge: node_id -> dst_node
                        outNeigh[node_id].push_back(dst_node);
                        inNeigh[dst_node].push_back(node_id);
                    }
                }
            }
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

    const auto n = numVertices();
    for (uint32_t nid = 0; nid < n; nid++) {
        if (valid[nid]) {
            // for all valid nodes
            if (outNeigh[nid].empty()) {
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
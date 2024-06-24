
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
            this -> numEdges = std::stoi(split_line[0]);
            this -> numNodes = std::stoi(split_line[1]);

            // Allocate memory
            this -> weight.resize(this -> numNodes);
            this -> nodeValid.resize(this -> numNodes);
            this -> inNeigh.resize(this -> numNodes);
            this -> outNeigh.resize(this -> numNodes);
        } else{
            // normal line
            if (split_line.size() < 2) {
                BOOST_LOG_TRIVIAL(fatal) << "Too few token(s) in line " << lineno << ": " << line;
                exit(-1);
            }

            for (uint32_t i = 1; i < split_line.size(); i++) {
                boost::trim(split_line[i]);
            }

            float node_weight = std::stof(split_line[1]);
            uint32_t node_id = lineno - 1;
            this->node_stmts.push_back(split_line[0]);

            if (node_weight < 0) {
                // An invalid node
                this -> weight[node_id] = 0;
                this -> nodeValid[node_id] = false;
            } else {
                this -> weight[node_id] = node_weight;
                this -> nodeValid[node_id] = true;

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
                        this -> inNeigh[dst_node].push_back(node_id);
                        this -> outNeigh[node_id].push_back(dst_node);
                    }
                }
            }
        }

        lineno ++;
    }

    if (lineno - 1 != this -> numNodes) {
        BOOST_LOG_TRIVIAL(fatal) << "Incorrect format: " << this -> numNodes
            << " Nodes declared, but " << lineno - 1 << " Nodes found";
        exit(-1);
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Build DAG from file: Done in " << time_ms << "ms";

}


bool DirectedAcyclicGraph::checkCorrectness() {

    BOOST_LOG_TRIVIAL(trace) << "Check input correctness: Start";
    auto start = std::chrono::system_clock::now();


    // Node ids should be continued integers starting from 0
    // No edge should connect to an invalid node

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Check input correctness: Done in " << time_ms << "ms";

    return true;
}

void DirectedAcyclicGraph::findSinkNodes() {
    BOOST_LOG_TRIVIAL(trace) << "Find all sink vtxs: Start";
    auto start = std::chrono::system_clock::now();

    for (uint32_t nid = 0; nid < numNodes; nid++) {
        if (nodeValid[nid]) {
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

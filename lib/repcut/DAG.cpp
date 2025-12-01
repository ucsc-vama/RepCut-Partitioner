#include "DAG.h"
#include "StringUtils.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/log/trivial.hpp>

using namespace repcut;

// Parse a uint32_t from a string_view using strto* (no std::string alloc,
// no exceptions).  On failure, logs a fatal message naming the line and
// exits.  `what` describes the field for the error message.
static uint32_t parse_uint(std::string_view tok, uint32_t lineno, const char* what) {
    std::string buf(tok);  // strto* needs NUL-terminated
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(buf.c_str(), &end, 10);
    if (errno != 0 || end == buf.c_str() || *end != '\0' || v < 0) {
        BOOST_LOG_TRIVIAL(fatal) << "Invalid " << what << " at line " << lineno << ": '" << tok << "'";
        exit(-1);
    }
    return static_cast<uint32_t>(v);
}

static float parse_float(std::string_view tok, uint32_t lineno, const char* what) {
    std::string buf(tok);
    char* end = nullptr;
    errno = 0;
    float v = std::strtof(buf.c_str(), &end);
    if (errno != 0 || end == buf.c_str() || *end != '\0') {
        BOOST_LOG_TRIVIAL(fatal) << "Invalid " << what << " at line " << lineno << ": '" << tok << "'";
        exit(-1);
    }
    return v;
}

void DirectedAcyclicGraph::buildFromFile(const char *filename) {
    BOOST_LOG_TRIVIAL(trace) << "Build DAG from file: Start";
    auto start = std::chrono::system_clock::now();

    std::ifstream file(filename);
    std::string line;
    uint32_t lineno = 0;

    while (std::getline(file, line))
    {
        TokenView tv(line);

        if (lineno == 0) {
            // header: numEdges numNodes
            auto a = tv.next();
            auto b = tv.next();
            if (a.empty() || b.empty()) {
                BOOST_LOG_TRIVIAL(fatal) << "Incorrect header at line " << lineno << ": " << line;
                exit(-1);
            }
            // numEdges is parsed for formatcompatibility but unused.
            (void)parse_uint(a, lineno, "numEdges");
            size_t numNodes = parse_uint(b, lineno, "numNodes");

            weight.resize(numNodes);
            valid.resize(numNodes);
            inNeigh.resize(numNodes);
            outNeigh.resize(numNodes);
        } else {
            // normal line: Label Weight out0 out1 ...
            auto label_tok = tv.next();
            auto weight_tok = tv.next();
            if (label_tok.empty() || weight_tok.empty()) {
                BOOST_LOG_TRIVIAL(fatal) << "Too few token(s) in line " << lineno << ": " << line;
                exit(-1);
            }

            uint32_t node_id = lineno - 1;
            float node_weight = parse_float(weight_tok, lineno, "node weight");

            if (node_weight < 0) {
                // An invalid node
                weight[node_id] = 0;
                valid[node_id] = false;
            } else {
                weight[node_id] = node_weight;
                valid[node_id] = true;
                // Pre-reserve the out-neighbor vector to the exact remaining
                // token count so push_back never reallocates mid-line.
                outNeigh[node_id].reserve(tv.count());
                while (!tv.done()) {
                    auto tok = tv.next();
                    uint32_t dst = parse_uint(tok, lineno, "node id");
                    outNeigh[node_id].push_back(dst);
                    inNeigh[dst].push_back(node_id);
                }
            }
        }

        lineno++;
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
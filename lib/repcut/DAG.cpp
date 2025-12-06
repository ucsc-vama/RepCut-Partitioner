#include "DAG.h"
#include "Log.h"
#include "StringUtils.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace repcut;

// Parse uint32_t from string_view using strto* (no exceptions).
static uint32_t parse_uint(std::string_view tok, uint32_t lineno, const char* what, RepCutLogLevel ll)
{
    std::string buf(tok); // strto* needs NUL-terminated
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(buf.c_str(), &end, 10);
    if (errno != 0 || end == buf.c_str() || *end != '\0' || v < 0) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Invalid %s at line %u: '%.*s'\n", what, lineno, static_cast<int>(tok.size()),
                tok.data());
        exit(-1);
    }
    return static_cast<uint32_t>(v);
}

static float parse_float(std::string_view tok, uint32_t lineno, const char* what, RepCutLogLevel ll)
{
    std::string buf(tok);
    char* end = nullptr;
    errno = 0;
    float v = std::strtof(buf.c_str(), &end);
    if (errno != 0 || end == buf.c_str() || *end != '\0') {
        rcp_log(ll, REPCUT_LOG_ERROR, "Invalid %s at line %u: '%.*s'\n", what, lineno, static_cast<int>(tok.size()),
                tok.data());
        exit(-1);
    }
    return v;
}

void DirectedAcyclicGraph::buildFromFile(const char* filename)
{
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Build DAG from file: Start\n");
    auto start = std::chrono::system_clock::now();

    std::ifstream file(filename);
    std::string line;
    uint32_t lineno = 0;

    while (std::getline(file, line)) {
        TokenView tv(line);

        if (lineno == 0) {
            // header: numEdges numNodes
            auto a = tv.next();
            auto b = tv.next();
            if (a.empty() || b.empty()) {
                rcp_log(log_level, REPCUT_LOG_ERROR, "Incorrect header at line %u: %s\n", lineno, line.c_str());
                exit(-1);
            }
            // numEdges is parsed for format compatibility but unused.
            (void)parse_uint(a, lineno, "numEdges", log_level);
            size_t numNodes = parse_uint(b, lineno, "numNodes", log_level);

            weight.resize(numNodes);
            valid.resize(numNodes);
            inNeigh.resize(numNodes);
            outNeigh.resize(numNodes);
        }
        else {
            // line: Label Weight out0 out1 ...
            auto label_tok = tv.next();
            auto weight_tok = tv.next();
            if (label_tok.empty() || weight_tok.empty()) {
                rcp_log(log_level, REPCUT_LOG_ERROR, "Too few token(s) at line %u: %s\n", lineno, line.c_str());
                exit(-1);
            }

            uint32_t node_id = lineno - 1;
            float node_weight = parse_float(weight_tok, lineno, "node weight", log_level);

            if (node_weight < 0) {
                weight[node_id] = 0;
                valid[node_id] = false;
            }
            else {
                weight[node_id] = node_weight;
                valid[node_id] = true;
                outNeigh[node_id].reserve(tv.count());
                outNeigh[node_id].reserve(tv.count());
                while (!tv.done()) {
                    auto tok = tv.next();
                    uint32_t dst = parse_uint(tok, lineno, "node id", log_level);
                    outNeigh[node_id].push_back(dst);
                    inNeigh[dst].push_back(node_id);
                }
            }
        }

        lineno++;
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Build DAG from file: Done in %llums\n", duration.count());
}

void DirectedAcyclicGraph::findSinkNodes()
{
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Find all sink vtxs: Start\n");
    auto start = std::chrono::system_clock::now();

    const auto n = numVertices();
    for (uint32_t nid = 0; nid < n; nid++) {
        if (valid[nid] && outNeigh[nid].empty())
            sinkNodes.push_back(nid);
    }
    rcp_log(log_level, REPCUT_LOG_INFO, "Found %zu sink nodes\n", sinkNodes.size());

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_DEBUG, "Find all sink vtxs: Done in %llums\n", duration.count());
}
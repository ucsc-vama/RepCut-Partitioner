//
// Created by Haoyuan Wang on 11/11/22.
//

#include "RepCutPartitioner.h"

#include "ClusterGraph.h"
#include "Log.h"
#include "StringUtils.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>

#include "process.hpp"

using namespace repcut;

namespace fs = std::filesystem;

// Verify and resolve the MtKaHyPar binary before any expensive work.
// Spawns `<bin> --version` (or `MtKaHyPar --version` if `bin` is null) and
// checks the exit status.  A zero (or any non-127-style) exit confirms the
// binary exists, is executable, and can run on this host.  Resolved binary
// name is cached in `mtkahypar_bin_resolved` for later use by _callMtKaHyPar.
bool RepCutPartitioner::prepareMtKaHyParBin(const char* bin) {
    const std::string resolved = (bin != nullptr && bin[0] != '\0')
        ? std::string(bin)
        : std::string("MtKaHyPar");

    rcp_log(log_level, REPCUT_LOG_DEBUG, "Verifying MtKaHyPar binary: %s\n", resolved.c_str());

    std::vector<std::string> args;
    args.push_back(resolved);
    args.push_back("--version");

    // Discard the child's stdout/stderr (only the exit status matters).
    // If the binary doesn't exist, exec will fail in the child and the
    // process returns a non-zero exit code.
    auto noop = [](const char*, size_t) {};
    TinyProcessLib::Process proc(args, "", noop, noop, false);
    const auto rc = proc.get_exit_status();

    if (rc != 0) {
        rcp_log(log_level, REPCUT_LOG_ERROR,
                 "MtKaHyPar binary '%s' not found or not usable (exit code %ld). "
                 "Set ctx.mtkahypar_bin to the MtKaHyPar executable, or install it on $PATH.\n",
                 resolved.c_str(), static_cast<long>(rc));
        return false;
    }

    mtkahypar_bin_resolved = resolved;
    rcp_log(log_level, REPCUT_LOG_DEBUG, "MtKaHyPar binary verified: %s\n", resolved.c_str());
    return true;
}


// Build the cluster graph from the design DAG and stream it to an hMetis
// file.  The cluster graph is stack-allocated and destroyed before
// returning so its memory does not overlap with the MtKaHyPar call or the
// reconstruction pass.
void RepCutPartitioner::_buildAndWriteHmetis(DirectedAcyclicGraph& dag) {
    rcp_log(log_level, REPCUT_LOG_INFO, "Collapse into cluster graph\n");
    ClusterGraph cluster_graph;
    cluster_graph.parallel_threads = cluster_parallel_threads;
    cluster_graph.log_level = log_level;
    cluster_graph.collapseFromDAG(dag);

    hmetis_path = fs::path(work_directory) / "parts.hmetis";
    cluster_graph.writeHMetisFile(hmetis_path.c_str());
}

void RepCutPartitioner::_callMtKaHyPar() {
    rcp_log(log_level, REPCUT_LOG_INFO, "Call MtKaHyPar\n");
    assert(this->parallel_threads > 0);
    assert(!mtkahypar_bin_resolved.empty() && "prepareMtKaHyParBin must be called before _callMtKaHyPar");

    std::vector<std::string> args;

    args.push_back(mtkahypar_bin_resolved);

    args.push_back("-t");
    args.push_back(std::to_string(this -> parallel_threads));
    args.push_back("-h");
    args.push_back(this -> hmetis_path.string());
    args.push_back("-k");
    args.push_back(std::to_string(this -> desired_parts));
    args.push_back("-e");
    args.push_back(std::to_string(this -> kahypar_imbalance_factor));
    args.push_back("--preset-type");
    args.push_back("default");
    args.push_back("--seed");
    args.push_back(std::to_string(this -> kahypar_seed));
    args.push_back("--mode");
    args.push_back("direct");
    args.push_back("-o");
    args.push_back("km1");
    args.push_back("--write-partition-file=true");

    std::ostringstream oss;
    for (const auto &s: args) {
        oss << s << " ";
    }
    rcp_log(log_level, REPCUT_LOG_INFO, "MtKaHyPar cmd: %s\n", oss.str().c_str());

    TinyProcessLib::Process kahypar_process(args, "");
    auto ret_code = kahypar_process.get_exit_status();

    if (ret_code != 0) {
        rcp_log(log_level, REPCUT_LOG_ERROR, "MtKaHyPar returns non-zero code: %d\n", ret_code);
        exit(-1);
    }
}


void RepCutPartitioner::_parseKaHyParResult() {
    auto kahypar_output_fullpath = fs::path(work_directory) / this -> mtkahypar_output_filename;

    auto file_status = fs::status(kahypar_output_fullpath);
    if (!fs::exists(file_status) || !fs::is_regular_file(file_status)) {
        rcp_log(log_level, REPCUT_LOG_ERROR,
                 "KaHyPar result file %s does not exist or is not a regular file!\n",
                 kahypar_output_fullpath.c_str());
        exit(-1);
    }

    std::ifstream kFile(kahypar_output_fullpath);
    std::string line;
    uint32_t lineno = 0;

    this -> coneIdToPartId.clear();
    this -> partIdToConeId.clear();
    this -> partIdToConeId.assign(this -> desired_parts, std::vector<uint32_t>());

    while (std::getline(kFile, line)) {
        TokenView tv(line);
        auto tok = tv.next();
        if (tok.empty()) {
            rcp_log(log_level, REPCUT_LOG_ERROR, "Empty partition id at line %u\n", lineno);
            exit(-1);
        }
        std::string buf(tok);  // strtol needs NUL-terminated
        char* end = nullptr;
        errno = 0;
        long pid_long = std::strtol(buf.c_str(), &end, 10);
        if (errno != 0 || end == buf.c_str() || *end != '\0') {
            rcp_log(log_level, REPCUT_LOG_ERROR,
                     "Invalid partition id at line %u: '%.*s'\n",
                     lineno, static_cast<int>(tok.size()), tok.data());
            exit(-1);
        }
        int32_t pid = static_cast<int32_t>(pid_long);
        // cone ${lineno} is assigned to partition ${pid}
        assert(pid < this->desired_parts);
        this -> coneIdToPartId.push_back(pid);
        this -> partIdToConeId[pid].push_back(lineno);

        lineno++;
    }
}

// Reconstruct per-partition DAG node sets by BFS upstream from each sink
// assigned to a partition.  A non-sink cluster touches cone c iff its nodes
// are ancestors of sink(c), so {ancestors of sink(c) : coneIdToPartId[c] == pid}
// is the set of nodes partition pid must simulate (replicated nodes appear
// in multiple partitions exactly when their descendant cones span parts).
//
// `vis` (per-partition unordered_set) serves as both the BFS visited marker
// and the dedup container; reused across partitions via clear().
void RepCutPartitioner::_reconstruct(DirectedAcyclicGraph& dag) {
    rcp_log(log_level, REPCUT_LOG_INFO, "Reconstruct partitions\n");
    auto start = std::chrono::system_clock::now();

    assert(coneIdToPartId.size() == dag.sinkNodes.size());

    partitions.clear();
    partitions.assign(desired_parts, std::vector<uint32_t>());

    std::unordered_set<uint32_t> vis;
    std::vector<uint32_t> fringe;
    std::vector<uint32_t> sinksForPart;
    vis.reserve(1 << 14);
    fringe.reserve(1 << 14);

    for (uint32_t pid = 0; pid < desired_parts; ++pid) {
        auto& part = partitions[pid];

        sinksForPart.clear();
        for (uint32_t cone_id = 0; cone_id < coneIdToPartId.size(); ++cone_id) {
            if (coneIdToPartId[cone_id] == pid) {
                sinksForPart.push_back(dag.sinkNodes[cone_id]);
            }
        }

        vis.clear();
        fringe.clear();
        for (uint32_t s : sinksForPart) {
            if (vis.insert(s).second) {
                fringe.push_back(s);
            }
        }

        while (!fringe.empty()) {
            const uint32_t v = fringe.back();
            fringe.pop_back();

            // vis already contains v (we insert on push).  Skip invalid nodes
            // but still keep them in vis so we don't re-traverse their edges.
            if (!dag.valid[v]) continue;
            part.push_back(v);

            for (const auto u : dag.inNeigh[v]) {
                if (vis.insert(u).second) {
                    fringe.push_back(u);
                }
            }
        }

        // Match historical output order (ascending ids) so rcp_output.txt
        // remains diffable against prior runs.
        std::sort(part.begin(), part.end());
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_INFO, "Reconstruct: Done in %llums\n", duration.count());
}

void RepCutPartitioner::partition(DirectedAcyclicGraph& dag, const int nparts) {
    rcp_log(log_level, REPCUT_LOG_INFO, "RepCut Partitioner: Start\n");
    auto start = std::chrono::system_clock::now();

    this -> desired_parts = nparts;

    // 1. Collapse DAG to cluster graph and write hMetis file (frees the
    //    cluster graph before returning).
    _buildAndWriteHmetis(dag);

    // Output filename MtKaHyPar will produce.  Format must match what the
    // MtKaHyPar binary writes for the given hmetis input.
    // MtKaHyPar's output filename mirrors its input filename with the partition
// parameters appended.  Matches the historical boost::format template
// "%1%.part%2%.epsilon%3%.seed%4%.KaHyPar".
    this -> mtkahypar_output_filename = std::format(
        "{}.part{}.epsilon{}.seed{}.KaHyPar",
        hmetis_path.filename().string(),
        this->desired_parts,
        this->kahypar_imbalance_factor,
        this->kahypar_seed);

    // 2. Call MtKaHyPar on the written hMetis file.
    _callMtKaHyPar();

    // 3. Parse MtKaHyPar's output into coneIdToPartId / partIdToConeId.
    _parseKaHyParResult();

    // 4. Reconstruct per-partition DAG node sets (upstream BFS).
    _reconstruct(dag);

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    rcp_log(log_level, REPCUT_LOG_INFO, "RepCut Partitioner: Done in %llums\n", time_ms);
}

std::unique_ptr<PartitionStatistics> RepCutPartitioner::reportPartitionStatus(DirectedAcyclicGraph& dag) {
    assert(!this->partitions.empty());

    auto ret = std::make_unique<PartitionStatistics>();
    ret->nparts = this->partitions.size();

    // Whole-design totals (only valid nodes).
    const auto n = dag.numVertices();
    for (uint32_t v = 0; v < n; ++v) {
        if (dag.valid[v]) {
            ret->sg_size++;
            ret->sg_weight += dag.weight[v];
        }
    }

    uint32_t total_part_size = 0;
    float total_part_weight = 0;

    for (auto& part : this->partitions) {
        uint32_t part_size = 0;
        float part_weight = 0;

        for (auto& nid : part) {
            if (dag.valid[nid]) {
                part_size += 1;
                part_weight += dag.weight[nid];
            }
        }

        total_part_size += part_size;
        total_part_weight += part_weight;

        ret->partition_size.push_back(part_size);
        ret->partition_weights.push_back(part_weight);
    }

    ret->total_part_size = total_part_size;
    ret->replication_size = ret->total_part_size - ret->sg_size;
    ret->replication_rate_size = static_cast<float>(ret->replication_size) * 100.0f / ret->sg_size;
    ret->ib_factor_size = calculate_ib_factor(ret->partition_size);

    ret->total_part_weight = total_part_weight;
    ret->replication_weight = ret->total_part_weight - ret->sg_weight;
    ret->replication_rate_weight = static_cast<float>(ret->replication_weight) * 100.0f / ret->sg_weight;
    ret->ib_factor_weight = calculate_ib_factor(ret->partition_weights);

    return ret;
}

void RepCutPartitioner::saveToFile(const char* filename) {
    rcp_log(log_level, REPCUT_LOG_INFO, "Write to output file: Start\n");
    auto start = std::chrono::system_clock::now();

    auto ofs = std::ofstream(work_directory + "/" + filename);

    for (uint32_t pid = 0; pid < partitions.size(); pid++) {
        for (auto& sg_id : this->partitions[pid]) {
            ofs << sg_id << ',';
        }
        ofs << "\n";
    }

    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_INFO, "Write to output file: Done in %llums\n", duration.count());
}
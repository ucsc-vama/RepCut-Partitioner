#include "RepCutPartitioner.h"

#include "ClusterGraph.h"
#include "Log.h"
#include "StringUtils.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_set>

#include "process.hpp"

using namespace repcut;

namespace fs = std::filesystem;

// Verify MtKaHyPar binary exists before any expensive work.
bool RepCutPartitioner::prepareMtKaHyParBin(const char* bin)
{
    const std::string resolved = (bin != nullptr && bin[0] != '\0') ? std::string(bin) : std::string("MtKaHyPar");

    rcp_log(log_level, REPCUT_LOG_DEBUG, "Verifying MtKaHyPar binary: %s\n", resolved.c_str());

    std::vector<std::string> args;
    args.push_back(resolved);
    args.push_back("--version");

    // Discard stdout/stderr (exit status is all that matters).
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

// Build cluster graph from DAG and write hMetis file.
bool RepCutPartitioner::_buildAndWriteHmetis(DirectedAcyclicGraph& dag)
{
    rcp_log(log_level, REPCUT_LOG_INFO, "Collapse into cluster graph\n");
    ClusterGraph cluster_graph;
    cluster_graph.log_level = log_level;
    cluster_graph.collapseFromDAG(dag);

    hmetis_path = fs::path(work_directory) / "parts.hmetis";
    cluster_graph.writeHMetisFile(hmetis_path.c_str());
    return true;
}

bool RepCutPartitioner::_callMtKaHyPar()
{
    rcp_log(log_level, REPCUT_LOG_INFO, "Call MtKaHyPar\n");
    assert(this->parallel_threads > 0);
    assert(!mtkahypar_bin_resolved.empty() && "prepareMtKaHyParBin must be called before _callMtKaHyPar");

    std::vector<std::string> args;

    args.push_back(mtkahypar_bin_resolved);

    args.push_back("-t");
    args.push_back(std::to_string(this->parallel_threads));
    args.push_back("-h");
    args.push_back(this->hmetis_path.string());
    args.push_back("-k");
    args.push_back(std::to_string(this->desired_parts));
    args.push_back("-e");
    args.push_back(std::to_string(this->kahypar_imbalance_factor));
    args.push_back("--preset-type");
    args.push_back("default");
    args.push_back("--seed");
    args.push_back(std::to_string(this->kahypar_seed));
    args.push_back("--mode");
    args.push_back("direct");
    args.push_back("-o");
    args.push_back("km1");
    args.push_back("--write-partition-file=true");

    std::ostringstream oss;
    for (const auto& s : args) {
        oss << s << " ";
    }
    rcp_log(log_level, REPCUT_LOG_INFO, "MtKaHyPar cmd: %s\n", oss.str().c_str());

    // Forward MtKaHyPar output only at info/debug log level.
    using Reader = std::function<void(const char*, size_t)>;
    auto noop = [](const char*, size_t) {};
    Reader fwd = [this](const char* bytes, size_t n) {
        rcp_log(log_level, REPCUT_LOG_INFO, "%.*s", static_cast<int>(n), bytes);
    };
    const bool verbose = rcp_should_log(log_level, REPCUT_LOG_INFO);
    TinyProcessLib::Process kahypar_process(args, "", verbose ? fwd : noop, verbose ? fwd : noop, false);
    auto ret_code = kahypar_process.get_exit_status();

    if (ret_code != 0) {
        rcp_log(log_level, REPCUT_LOG_ERROR, "MtKaHyPar returns non-zero code: %d\n", ret_code);
        return false;
    }
    return true;
}

bool RepCutPartitioner::_parseKaHyParResult()
{
    auto kahypar_output_fullpath = fs::path(work_directory) / this->mtkahypar_output_filename;

    auto file_status = fs::status(kahypar_output_fullpath);
    if (!fs::exists(file_status) || !fs::is_regular_file(file_status)) {
        rcp_log(log_level, REPCUT_LOG_ERROR, "KaHyPar result file %s does not exist or is not a regular file!\n",
                kahypar_output_fullpath.c_str());
        return false;
    }

    std::ifstream kFile(kahypar_output_fullpath);
    std::string line;
    uint32_t lineno = 0;

    this->coneIdToPartId.clear();
    this->partIdToConeId.clear();
    this->partIdToConeId.assign(this->desired_parts, std::vector<uint32_t>());

    while (std::getline(kFile, line)) {
        TokenView tv(line);
        auto tok = tv.next();
        if (tok.empty()) {
            rcp_log(log_level, REPCUT_LOG_ERROR, "Empty partition id at line %u\n", lineno);
            return false;
        }
        std::string buf(tok); // strtol needs NUL-terminated
        char* end = nullptr;
        errno = 0;
        long pid_long = std::strtol(buf.c_str(), &end, 10);
        if (errno != 0 || end == buf.c_str() || *end != '\0') {
            rcp_log(log_level, REPCUT_LOG_ERROR, "Invalid partition id at line %u: '%.*s'\n", lineno,
                    static_cast<int>(tok.size()), tok.data());
            return false;
        }
        int32_t pid = static_cast<int32_t>(pid_long);
        // cone ${lineno} is assigned to partition ${pid}
        assert(pid < this->desired_parts);
        this->coneIdToPartId.push_back(pid);
        this->partIdToConeId[pid].push_back(lineno);

        lineno++;
    }
    return true;
}

bool RepCutPartitioner::_reconstruct(DirectedAcyclicGraph& dag)
{
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

            // Skip invalid nodes (keep in vis to avoid re-traversing edges).
            if (!dag.valid[v])
                continue;
            part.push_back(v);

            for (const auto u : dag.inNeigh[v]) {
                if (vis.insert(u).second) {
                    fringe.push_back(u);
                }
            }
        }

        // Sort ascending for diffable output.
        std::sort(part.begin(), part.end());
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_INFO, "Reconstruct: Done in %llums\n", duration.count());
    return true;
}

bool RepCutPartitioner::partition(DirectedAcyclicGraph& dag, const int nparts)
{
    rcp_log(log_level, REPCUT_LOG_INFO, "RepCut Partitioner: Start\n");
    auto start = std::chrono::system_clock::now();

    this->desired_parts = nparts;

    // 1. Collapse DAG to cluster graph and write hMetis file.
    if (!_buildAndWriteHmetis(dag)) return false;

    this->mtkahypar_output_filename =
        std::format("{}.part{}.epsilon{}.seed{}.KaHyPar", hmetis_path.filename().string(), this->desired_parts,
                    this->kahypar_imbalance_factor, this->kahypar_seed);

    // 2. Call MtKaHyPar on the written hMetis file.
    if (!_callMtKaHyPar()) return false;

    // 3. Parse MtKaHyPar's output into coneIdToPartId / partIdToConeId.
    if (!_parseKaHyParResult()) return false;

    // 4. Reconstruct per-partition DAG node sets (upstream BFS).
    if (!_reconstruct(dag)) return false;

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    rcp_log(log_level, REPCUT_LOG_INFO, "RepCut Partitioner: Done in %llums\n", time_ms);
    return true;
}

std::unique_ptr<PartitionStatistics> RepCutPartitioner::reportPartitionStatus(DirectedAcyclicGraph& dag)
{
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
    ret->replication_rate_size = (ret->sg_size > 0)
        ? static_cast<float>(ret->replication_size) * 100.0f / ret->sg_size
        : 0.0f;
    ret->ib_factor_size = calculate_ib_factor(ret->partition_size);

    ret->total_part_weight = total_part_weight;
    ret->replication_weight = ret->total_part_weight - ret->sg_weight;
    ret->replication_rate_weight = (ret->sg_weight != 0.0f)
        ? static_cast<float>(ret->replication_weight) * 100.0f / ret->sg_weight
        : 0.0f;
    ret->ib_factor_weight = calculate_ib_factor(ret->partition_weights);

    return ret;
}

void RepCutPartitioner::saveToFile(const char* filename)
{
    rcp_log(log_level, REPCUT_LOG_INFO, "Write to output file: Start\n");
    auto start = std::chrono::system_clock::now();

    auto ofs = std::ofstream(work_directory + "/" + filename);

    // Reuse std::string to avoid per-integer operator<< overhead.
    std::string line;
    for (uint32_t pid = 0; pid < partitions.size(); pid++) {
        line.clear();
        for (auto sg_id : this->partitions[pid]) {
            line += std::to_string(sg_id);
            line += ',';
        }
        line += '\n';
        ofs << line;
    }

    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    rcp_log(log_level, REPCUT_LOG_INFO, "Write to output file: Done in %llums\n", duration.count());
}
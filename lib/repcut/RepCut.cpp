// Public C entry point for librepcut.

#include "repcut.h"

#include "DAG.h"
#include "Log.h"
#include "RepCutPartitioner.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

using namespace repcut;

static int run(const struct RepCutContext* ctx, struct RepCutStatistics* stats_out)
{
    if (ctx == nullptr) {
        std::fprintf(stderr, "repcut_run: context is null\n");
        return 1;
    }

    const RepCutLogLevel ll = ctx->log_level;

    // Validate inputs
    if (ctx->graph_filename == nullptr || ctx->graph_filename[0] == '\0') {
        rcp_log(ll, REPCUT_LOG_ERROR, "graph_filename is empty\n");
        return 1;
    }
    if (ctx->work_directory == nullptr || ctx->work_directory[0] == '\0') {
        rcp_log(ll, REPCUT_LOG_ERROR, "work_directory is empty\n");
        return 1;
    }
    if (ctx->nparts <= 0) {
        rcp_log(ll, REPCUT_LOG_ERROR, "nparts must be > 0 (got %d)\n", ctx->nparts);
        return 1;
    }

    if (ctx->parallel_threads <= 0) {
        rcp_log(ll, REPCUT_LOG_ERROR, "parallel_threads must be > 0\n");
        return 1;
    }
    if (!std::isfinite(ctx->target_ib) || ctx->target_ib < 0) {
        rcp_log(ll, REPCUT_LOG_ERROR, "target_ib must be finite and nonnegative\n");
        return 1;
    }
    if (ctx->seed < -1) {
        rcp_log(ll, REPCUT_LOG_ERROR, "seed must be -1 or nonnegative (got %d)\n", ctx->seed);
        return 1;
    }

    if (!fs::exists(ctx->graph_filename) || !fs::is_regular_file(ctx->graph_filename)) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Input graph file %s does not exist or is not a regular file\n",
                ctx->graph_filename);
        return 1;
    }
    if (!fs::exists(ctx->work_directory) || !fs::is_directory(ctx->work_directory)) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Work directory %s does not exist or is not a directory\n", ctx->work_directory);
        return 1;
    }

    // Acquire work-directory lock
    const fs::path lock_path = fs::path(ctx->work_directory) / ".lock";
    if (fs::exists(lock_path)) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Work directory %s is already in use (lock file %s exists)\n",
                ctx->work_directory, lock_path.c_str());
        return 1;
    }
    {
        std::ofstream lk(lock_path);
        if (!lk) {
            rcp_log(ll, REPCUT_LOG_ERROR, "Cannot create lock file %s\n", lock_path.c_str());
            return 1;
        }
        lk << "repcut\n";
    }

    // RAII lock guard — removes the lock on normal return (including early
    // error returns below).  No exit() calls anywhere in the library, so
    // destructors always run.
    struct LockGuard
    {
        fs::path p;
        ~LockGuard()
        {
            std::error_code ec;
            if (fs::exists(p, ec))
                fs::remove(p, ec);
        }
    } guard{lock_path};

    // Early backend check (fail fast before graph traversal).
    auto rcp = std::make_unique<RepCutPartitioner>();
    rcp->log_level = ll;
    rcp->parallel_threads = ctx->parallel_threads;
    if (!rcp->prepareMtKaHyPar(ctx->mtkahypar_bin)) {
        return 1;
    }

    // Build DAG
    auto dag = std::make_unique<DirectedAcyclicGraph>();
    dag->log_level = ll;
    if (!dag->buildFromFile(ctx->graph_filename)) {
        return 1;
    }
    dag->findSinkNodes();

    // Run partitioner
    rcp->kahypar_imbalance_factor = ctx->target_ib;
    rcp->kahypar_seed = ctx->seed;
    rcp->set_work_directory(ctx->work_directory);
    if (!rcp->partition(*dag, ctx->nparts)) {
        return 1;
    }

    // Write output file
    rcp->saveToFile("rcp_output.txt");

    // Optional stats
    if (stats_out != nullptr) {
        auto stat = rcp->reportPartitionStatus(*dag);
        stats_out->nparts = stat->nparts;
        stats_out->ib_factor_weight = stat->ib_factor_weight;
        stats_out->sg_weight = stat->sg_weight;
        stats_out->total_part_weight = stat->total_part_weight;
        stats_out->replication_weight = stat->replication_weight;
        stats_out->replication_rate_weight = stat->replication_rate_weight;
        stats_out->ib_factor_size = stat->ib_factor_size;
        stats_out->sg_size = stat->sg_size;
        stats_out->total_part_size = stat->total_part_size;
        stats_out->replication_size = stat->replication_size;
        stats_out->replication_rate_size = stat->replication_rate_size;
    }

    rcp_log(ll, REPCUT_LOG_INFO, "Done\n");
    return 0;
}

extern "C" int repcut_run(const struct RepCutContext* ctx, struct RepCutStatistics* stats_out)
{
    // MtKaHyPar owns process-global state (including its RNG and thread pool).
    // Reject overlap rather than silently serializing independent callers.
    static std::atomic_flag busy = ATOMIC_FLAG_INIT;
    if (busy.test_and_set(std::memory_order_acquire)) {
        std::fprintf(stderr, "repcut_run: another call is already active in this process\n");
        return 1;
    }
    struct BusyGuard
    {
        std::atomic_flag& flag;
        ~BusyGuard() { flag.clear(std::memory_order_release); }
    } guard{busy};

    // No C++ exception may cross the public C ABI. Local resource and workdir
    // guards unwind first, so a failed call does not prevent a later retry.
    try {
        return run(ctx, stats_out);
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "repcut_run: %s\n", error.what());
    }
    catch (...) {
        std::fprintf(stderr, "repcut_run: unknown exception\n");
    }
    return 1;
}

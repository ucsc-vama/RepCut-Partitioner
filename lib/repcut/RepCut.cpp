//
// Public C entry point for librepcut.
//
// repcut_run runs the full RepCut pipeline on a design graph and writes the
// partitioned output.  It is reentrant across distinct work_directories;
// a `.lock` file is used as a concurrent-use safeguard.
//

#include "repcut.h"

#include "DAG.h"
#include "Log.h"
#include "RepCutPartitioner.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

using namespace repcut;

extern "C" int repcut_run(const struct RepCutContext* ctx, struct RepCutStatistics* stats_out) {
    if (ctx == nullptr) {
        std::fprintf(stderr, "repcut_run: context is null\n");
        return 1;
    }

    const RepCutLogLevel ll = ctx->log_level;

    // ---- Validate inputs ----
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

    if (!fs::exists(ctx->graph_filename) || !fs::is_regular_file(ctx->graph_filename)) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Input graph file %s does not exist or is not a regular file\n",
                 ctx->graph_filename);
        return 1;
    }
    if (!fs::exists(ctx->work_directory) || !fs::is_directory(ctx->work_directory)) {
        rcp_log(ll, REPCUT_LOG_ERROR, "Work directory %s does not exist or is not a directory\n",
                 ctx->work_directory);
        return 1;
    }

    // ---- Acquire work-directory lock ----
    const fs::path lock_path = fs::path(ctx->work_directory) / ".lock";
    if (fs::exists(lock_path)) {
        rcp_log(ll, REPCUT_LOG_ERROR,
                 "Work directory %s is already in use (lock file %s exists)\n",
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

    // RAII guard removes the lock on normal return.  Note: the inner pipeline
    // may call exit(-1) on hard failures (e.g. MtKaHyPar nonzero exit), which
    // bypasses destructors — the lock file will be left behind in that case.
    // Successful returns remove it cleanly here.
    struct LockGuard {
        fs::path p;
        ~LockGuard() {
            std::error_code ec;
            if (fs::exists(p, ec)) fs::remove(p, ec);
        }
    } guard{lock_path};

    // ---- Early MtKaHyPar sanity check, before traversing the graph ----
    // Fails fast if the binary is missing so the user doesn't wait through
    // the DAG parse + cluster collapse just to learn the binary isn't there.
    auto rcp = std::make_unique<repcut::RepCutPartitioner>();
    rcp->log_level = ll;
    if (!rcp->prepareMtKaHyParBin(ctx->mtkahypar_bin)) {
        return 1;
    }

    // ---- Build DAG ----
    auto dag = std::make_unique<repcut::DirectedAcyclicGraph>();
    dag->log_level = ll;
    dag->buildFromFile(ctx->graph_filename);
    dag->findSinkNodes();

    // ---- Run partitioner ----
    rcp->kahypar_imbalance_factor = ctx->target_ib;
    rcp->cluster_parallel_threads = ctx->parallel_threads;
    rcp->parallel_threads = ctx->parallel_threads;
    rcp->kahypar_seed = ctx->seed;
    rcp->set_work_directory(ctx->work_directory);
    rcp->partition(*dag, ctx->nparts);

    // ---- Write output file ----
    rcp->saveToFile("rcp_output.txt");

    // ---- Optional stats ----
    if (stats_out != nullptr) {
        auto stat = rcp->reportPartitionStatus(*dag);
        stats_out->nparts                 = stat->nparts;
        stats_out->ib_factor_weight       = stat->ib_factor_weight;
        stats_out->sg_weight              = stat->sg_weight;
        stats_out->total_part_weight      = stat->total_part_weight;
        stats_out->replication_weight     = stat->replication_weight;
        stats_out->replication_rate_weight = stat->replication_rate_weight;
        stats_out->ib_factor_size          = stat->ib_factor_size;
        stats_out->sg_size                 = stat->sg_size;
        stats_out->total_part_size         = stat->total_part_size;
        stats_out->replication_size        = stat->replication_size;
        stats_out->replication_rate_size   = stat->replication_rate_size;
    }

    rcp_log(ll, REPCUT_LOG_INFO, "Done\n");
    return 0;
}
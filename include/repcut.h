// Public C interface to librepcut (RepCut partitioning pipeline).
// Sequential calls are supported; overlapping calls in one process are rejected.

#ifndef REPCUT_H
#define REPCUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RepCutLogLevel {
    REPCUT_LOG_SILENT = 0, // default: no output on success
    REPCUT_LOG_ERROR = 1,  // errors always print (cannot be suppressed)
    REPCUT_LOG_WARN = 2,
    REPCUT_LOG_INFO = 3,  // -v
    REPCUT_LOG_DEBUG = 4, // -vv
};

// Zero-initialize before setting fields.
struct RepCutContext
{
    const char* graph_filename;    // required: path to Metis-like graph file
    const char* work_directory;    // required: writable dir; must be unique per concurrent run
    int nparts;                    // required: partition count
    float target_ib;               // default 0.03
    int parallel_threads;          // default 1
    int seed;                      // nonnegative fixed seed; -1 = choose randomly, logged at INFO
    enum RepCutLogLevel log_level; // default SILENT

    // Optional MtKaHyPar-compatible executable. A nonempty value overrides
    // the linked backend. NULL/empty uses linked MtKaHyPar, or searches PATH
    // for "MtKaHyPar" in executable-only builds.
    // An explicit executable is checked with --version before graph traversal.
    const char* mtkahypar_bin;

    // Default false. Save parts.hmetis and cone assignments in work_directory.
    bool save_intermediate_files;
};

// Aggregated partitioning statistics.  Optional out-parameter of repcut_run;
// pass NULL if not needed.  All aggregate fields; per-partition breakdown is
// not exposed at the C ABI level (C++ callers can use RepCutPartitioner::
// reportPartitionStatus for the full PartitionStatistics object).
struct RepCutStatistics
{
    uint32_t nparts;

    float ib_factor_weight;
    float sg_weight;
    float total_part_weight;
    float replication_weight;
    float replication_rate_weight;

    float ib_factor_size;
    uint32_t sg_size;
    uint32_t total_part_size;
    uint32_t replication_size;
    float replication_rate_size;
};

// Run the full RepCut pipeline.  Returns 0 on success, non-zero on failure.
// On success, writes "rcp_output.txt" under ctx->work_directory.  If
// stats_out is non-NULL, populated with aggregate partition statistics.
// Only one call may be active per process, even with distinct work directories.
// The first linked-backend call initializes MtKaHyPar's process-wide thread
// pool; later linked calls must request the same parallel_threads. Per-call
// resources are released, but the pool and TBB concurrency limit remain until
// process exit. The host must not initialize or use MtKaHyPar independently
// of librepcut.
// MtKaHyPar 1.7 does not reset all cached RNG state when reseeding, so repeated
// linked calls may produce different partitions with the same seed, even at
// one thread. For reproducibility, use a fixed seed, one thread, and a fresh
// process for each run with the same backend.
int repcut_run(const struct RepCutContext* ctx, struct RepCutStatistics* stats_out);

#ifdef __cplusplus
}
#endif

#endif // REPCUT_H

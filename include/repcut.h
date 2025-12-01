//
// Public C interface to librepcut.
//
// librepcut implements the RepCut partitioning pipeline (collapse design DAG
// to cluster graph, build intersection hypergraph, call MtKaHyPar, reconstruct
// per-partition DAG node sets).  This header exposes a reentrant entry point
// suitable for embedding into other tools.
//
// Reentrancy: repcut_run() may be called concurrently from multiple threads
// *only if* each call uses a distinct work_directory.  A `.lock` file is
// created in work_directory at entry and removed at exit as a safeguard
// against accidental concurrent use of the same directory; repcut_run()
// returns non-zero immediately if the lock already exists.
//
// Logging: by default repcut_run writes nothing to stderr on success.  Errors
// always print to stderr regardless of log_level.  Info/debug output is gated
// by the log_level field of RepCutContext.
//

#ifndef REPCUT_H
#define REPCUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RepCutLogLevel {
    REPCUT_LOG_SILENT = 0,   // default: no output on success
    REPCUT_LOG_ERROR  = 1,   // errors always print (cannot be suppressed)
    REPCUT_LOG_WARN   = 2,
    REPCUT_LOG_INFO   = 3,    // -v
    REPCUT_LOG_DEBUG  = 4,    // -vv
};

struct RepCutContext {
    const char* graph_filename;          // required: path to Metis-like graph file
    const char* work_directory;          // required: writable dir; must be unique per concurrent run
    int          nparts;                // required: partition count
    float        target_ib;             // default 0.03
    int          parallel_threads;      // default 1 (only 1 is deterministic for MtKaHyPar)
    int          seed;                 // -1 = MtKaHyPar picks
    enum RepCutLogLevel log_level;     // default SILENT
};

// Aggregated partitioning statistics.  Optional out-parameter of repcut_run;
// pass NULL if not needed.  All aggregate fields; per-partition breakdown is
// not exposed at the C ABI level (C++ callers can use RepCutPartitioner::
// reportPartitionStatus for the full PartitionStatistics object).
struct RepCutStatistics {
    uint32_t nparts;

    float    ib_factor_weight;
    float    sg_weight;
    float    total_part_weight;
    float    replication_weight;
    float    replication_rate_weight;

    float    ib_factor_size;
    uint32_t sg_size;
    uint32_t total_part_size;
    uint32_t replication_size;
    float    replication_rate_size;
};

// Run the full RepCut pipeline.  Returns 0 on success, non-zero on failure.
// On success, writes "rcp_output.txt" under ctx->work_directory.  If
// stats_out is non-NULL, populated with aggregate partition statistics.
int repcut_run(const struct RepCutContext* ctx, struct RepCutStatistics* stats_out);

#ifdef __cplusplus
}
#endif

#endif  // REPCUT_H
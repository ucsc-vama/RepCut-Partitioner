#include "CommandlineOptions.h"
#include "repcut.h"

#include <cstdio>
#include <iostream>
#include <algorithm>

int main(int argc, char** argv) {
    if (!parse_commandline_options(argc, argv)) {
        return 1;
    }

    struct RepCutContext ctx;
    ctx.graph_filename    = opts.graph_filename.c_str();
    ctx.work_directory    = opts.work_directory.c_str();
    ctx.nparts            = opts.nparts;
    ctx.target_ib         = opts.target_ib;
    ctx.parallel_threads  = opts.parallel_threads;
    ctx.seed              = opts.seed;
    ctx.log_level         = resolve_log_level();
    // Empty string → NULL so the library searches $PATH for "MtKaHyPar".
    ctx.mtkahypar_bin     = opts.mtkahypar_bin.empty() ? nullptr : opts.mtkahypar_bin.c_str();

    struct RepCutStatistics stat{};
    const int rc = repcut_run(&ctx, &stat);
    if (rc != 0) {
        return rc;
    }

    // CLI-only formatting of the aggregate stats.  The library does not print
    // stats itself; this is the host tool's responsibility.
    std::printf("================== Report Partition Statistics ==================\n");
    std::printf("Total node count is %u, original statement graph has %u valid nodes\n",
                stat.total_part_size, stat.sg_size);
    std::printf("Duplication stmt cost: %u (%.2f%%)\n",
                stat.replication_size, stat.replication_rate_size);
    std::printf("Duplication weight cost: %.2f (%.2f%%)\n",
                stat.replication_weight, stat.replication_rate_weight);
    std::printf("Weight ib factor: %f\n", stat.ib_factor_weight);
    std::printf("=================================================================\n");

    return 0;
}
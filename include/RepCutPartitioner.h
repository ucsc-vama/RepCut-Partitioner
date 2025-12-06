//
// Created by Haoyuan Wang on 11/11/22.
//

#ifndef RCP_REP_CUT_PARTITIONER_H
#define RCP_REP_CUT_PARTITIONER_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "DAG.h"
#include "Util.h"
#include "repcut.h"

namespace fs = std::filesystem;

namespace repcut {
    class RepCutPartitioner {
    private:
        void _buildAndWriteHmetis(DirectedAcyclicGraph& dag);
        void _callMtKaHyPar();
        void _parseKaHyParResult();
        void _reconstruct(DirectedAcyclicGraph& dag);

        // hmetis file path (written by _buildAndWriteHmetis, read by _callMtKaHyPar).
        fs::path hmetis_path;
        // Output filename produced by MtKaHyPar.
        fs::path mtkahypar_output_filename;
        // Resolved MtKaHyPar binary path (set by prepareMtKaHyParBin).
        std::string mtkahypar_bin_resolved;

    public:
        // Parameters.
        float kahypar_imbalance_factor = 0.015;
        int32_t kahypar_seed = -1;
        uint32_t desired_parts = 0;
        int parallel_threads = -1;
        uint32_t cluster_parallel_threads = 1;

        std::string work_directory;
        void set_work_directory(const std::string& work_dir) {work_directory = work_dir;};

        // Log threshold carried from RepCutContext.
        RepCutLogLevel log_level = REPCUT_LOG_SILENT;

        // Verify and resolve the MtKaHyPar binary.  If `bin` is non-NULL it
        // is used verbatim; otherwise $PATH is searched for "MtKaHyPar".  The
        // binary is run with --version to confirm it exists and is executable
        // before any expensive graph traversal work begins.  Returns true on
        // success; on failure logs an explicit error and returns false.
        bool prepareMtKaHyParBin(const char* bin);

        // Result: cone (sink) → partition id, as reported by MtKaHyPar.
        std::vector<uint32_t> coneIdToPartId;
        std::vector<std::vector<uint32_t>> partIdToConeId;

        // Result: per-partition sets of DAG node ids that the partition must
        // simulate (replicated ancestors included).  Sorted ascending.
        std::vector<std::vector<uint32_t>> partitions;

        // Single high-level entry point: take a design DAG and a target
        // partition count, run the full RepCut pipeline (collapse to cluster
        // graph, write hMetis, call MtKaHyPar, parse result, reconstruct
        // per-partition DAG node sets via upstream BFS).  Fills `partitions`
        // and `coneIdToPartId`.  Requires prepareMtKaHyParBin() to have been
        // called successfully beforehand.
        void partition(DirectedAcyclicGraph& dag, const int nparts);

        // Compute statistics over `partitions` against the design DAG.
        // Returns a unique_ptr so the caller does not have to manage lifetime
        // manually.
        std::unique_ptr<PartitionStatistics> reportPartitionStatus(DirectedAcyclicGraph& dag);

        // Write `partitions` (one comma-separated line of DAG node ids per
        // partition) to `${work_directory}/${filename}`.
        void saveToFile(const char* filename);
    };
}

#endif //RCP_REP_CUT_PARTITIONER_H
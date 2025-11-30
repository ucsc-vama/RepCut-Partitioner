//
// Created by Haoyuan Wang on 11/11/22.
//

#ifndef RCP_REP_CUT_PARTITIONER_H
#define RCP_REP_CUT_PARTITIONER_H

#include "rcp_common.h"

namespace repcut {
    class RepCutPartitioner {
    private:
        void _callMtKaHyPar();
        void _parseKaHyParResult();
    public:
        // Path to the pre-written hMetis-format input file.  The caller is
        // responsible for writing it (e.g. ClusterGraph::writeHMetisFile)
        // before invoking partition().
        fs::path hmetis_path;
        fs::path mtkahypar_output_filename;

        std::string mtkahypar_cmd = "MtKaHyPar";

        float kahypar_imbalance_factor = 0.015;
        int32_t kahypar_seed = -1;
        uint32_t desired_parts = 0;
        int parallel_threads = -1;

        // result
        std::vector<uint32_t> coneIdToPartId;
        std::vector<std::vector<uint32_t>> partIdToConeId;

        std::string work_directory;

        void set_work_directory(const std::string& work_dir) {work_directory = work_dir;};

        void partition(const int nparts);
    };
}

#endif //RCP_REP_CUT_PARTITIONER_H
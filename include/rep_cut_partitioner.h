//
// Created by Haoyuan Wang on 11/11/22.
//

#ifndef RCP_REP_CUT_PARTITIONER_H
#define RCP_REP_CUT_PARTITIONER_H

#include "rcp_common.h"
#include "hyper_graph.h"

namespace repcut {
    class RepCutPartitioner {
    private:
        void _callMtKaHyPar();
        void _parseKaHyParResult();
    public:
        // parameters
        fs::path hmetis_filename = "parts.hmetis";
        // fs::path kahypar_config_filename = "KaHyPar.config";
        fs::path mtkahypar_output_filename;

        std::string mtkahypar_cmd = "MtKaHyPar";

        float kahypar_imbalance_factor = 0.015;
        int32_t kahypar_seed = -1;
        uint32_t desired_parts = 0;
        int parallel_threads = -1;

        HyperGraph* hg = nullptr;

        // result
        std::vector<uint32_t> coneIdToPartId;
        std::vector<std::vector<uint32_t>> partIdToConeId;

        std::string work_directory;

        void set_work_directory(const std::string& work_dir) {work_directory = work_dir;};

        void write_hmetis();

        void partition(const int nparts);
    };
}

#endif //RCP_REP_CUT_PARTITIONER_H

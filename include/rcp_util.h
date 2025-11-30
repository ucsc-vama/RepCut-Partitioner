//
// Created by Haoyuan Wang on 11/14/22.
//

#ifndef RCP_RCP_UTIL_H
#define RCP_RCP_UTIL_H

#include "rcp_common.h"

namespace repcut {
    float calculate_ib_factor(std::vector<uint32_t>& dat);
    float calculate_ib_factor(std::vector<float>& dat);

    // Aggregated statistics for a partitioning result.  Reconstructed by
    // Reconstructor::reportPartitionStatus from the per-partition DAG node
    // lists.  Formerly partition_stat.h.
    class PartitionStatistics {
    public:
        uint32_t nparts;

        float ib_factor_weight;
        float sg_weight;
        float total_part_weight;
        float replication_weight;
        float replication_rate_weight;
        std::vector<float> partition_weights;

        float ib_factor_size;
        uint32_t sg_size;
        uint32_t total_part_size;
        uint32_t replication_size;
        float replication_rate_size;
        std::vector<uint32_t> partition_size;

        void print_stat();
    };
}


#endif //RCP_RCP_UTIL_H
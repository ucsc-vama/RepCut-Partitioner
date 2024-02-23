//
// Created by Haoyuan Wang on 12/13/22.
//

#include "partition_stat.h"
#include <iostream>

using namespace repcut;

void PartitionStatistics::print_stat() {
    std::cout << "================== Report Partition Statistics ==================\n";
    for (uint32_t pid = 0; pid < nparts; pid++) {
        std::cout << "Pid: " << pid
            << ", part size: " << partition_size[pid]
            << ", part weight: " << partition_weights[pid] << "\n";
    }
    std::cout << "Total node count is " << total_part_size << ", original statement graph has " << sg_size << " valid nodes" << "\n";

    uint32_t part_size_max = *std::max_element(partition_size.begin(), partition_size.end());
    uint32_t part_size_min = *std::min_element(partition_size.begin(), partition_size.end());
    std::cout << "Duplication stmt cost: " << replication_size << " (" << replication_rate_size << "%)\n";
    std::cout << "Partition size: max: " << part_size_max << ", min: " << part_size_min << ", avg: " << total_part_size / nparts << "\n";

    float part_weight_max = *std::max_element(partition_weights.begin(), partition_weights.end());
    float part_weight_min = *std::min_element(partition_weights.begin(), partition_weights.end());
    std::cout << "Total node weight (whole design) is " << sg_weight << "\n";
    std::cout << "Duplication weight cost: " << replication_weight << " (" << replication_rate_weight << "%)\n";
    std::cout << "Partition weight: max: " << part_weight_max << ", min: " << part_weight_min << ", avg: " << total_part_weight / static_cast<float>(nparts) << std::endl;

    std::cout << "Weight ib factor: " << ib_factor_weight << std::endl;
    std::cout << "=================================================================\n";
}
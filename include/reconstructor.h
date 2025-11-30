//
// Created by Haoyuan Wang on 12/15/22.
//

#ifndef RCP_RECONSTRUCTOR_H
#define RCP_RECONSTRUCTOR_H

#include "rcp_common.h"
#include "dag.h"
#include "partition_stat.h"

namespace repcut {
    class Reconstructor {
    public:
        // id: id in input DAG
        std::vector<std::vector<uint32_t>> partitions;

        std::string work_directory;

        void set_work_directory(const std::string& work_dir) {work_directory = work_dir;};

        // Reconstruct each partition by BFS upstream from the sink vertices
        // assigned to it.  Performs dedup via a per-partition unordered_set
        // (BFS visited set + dedup combined).  Output ids per partition are
        // sorted ascending to match the historical SBitSet-based output order.
        void construct(const int nparts,
                       DirectedAcyclicGraph* dag,
                       const std::vector<uint32_t>& coneIdToPartId);

        // Compute per-partition statistics over the reconstructed partitions.
        // Replaces ClusterGraph::reportPartitionStatus which was driven by
        // the now-removed SBitSet-based partitions.
        PartitionStatistics* reportPartitionStatus(DirectedAcyclicGraph* dag);

        void saveToFile(const char* filename);
    };
}


#endif //RCP_RECONSTRUCTOR_H
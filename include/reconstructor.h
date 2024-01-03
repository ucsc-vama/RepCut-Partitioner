//
// Created by Haoyuan Wang on 12/15/22.
//

#ifndef RCP_RECONSTRUCTOR_H
#define RCP_RECONSTRUCTOR_H

#include "rcp_common.h"
#include "SBitSet.h"

#include "cluster_graph.h"

class Reconstructor {
public:
    // id: id in input DAG
    std::vector<SBitSet> partitions;

    std::string work_directory;

    void set_work_directory(const std::string& work_dir) {work_directory = work_dir;};

    void construct(const int nparts, ClusterGraph* cg);

    void saveToFile(const char* filename);
};
#endif //RCP_RECONSTRUCTOR_H

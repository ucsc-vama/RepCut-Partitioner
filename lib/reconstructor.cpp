//
// Created by Haoyuan Wang on 12/15/22.
//

#include "reconstructor.h"

#include <fstream>

void Reconstructor::construct(const int nparts, ClusterGraph* cg) {
    BOOST_LOG_TRIVIAL(info) << "Reconstruct: Start";
    auto start = std::chrono::system_clock::now();

    this -> partitions.clear();
    this -> partitions.assign(nparts, SBitSet());

    for (uint32_t pid = 0; pid < nparts; pid++) {
        auto partition_clusters = cg -> partitions[pid].get_elems();

        for (auto cid: *partition_clusters) {
            for (auto sg_id: cg -> clusters[cid]) {
                this -> partitions[pid].insert(sg_id);
            }
        }

        delete partition_clusters;
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(info) << "Reconstruct: Done in " << time_ms << "ms";
}

void Reconstructor::saveToFile(const char* filename) {
    BOOST_LOG_TRIVIAL(info) << "Write to file: Start";
    auto start = std::chrono::system_clock::now();

    auto ofs = std::ofstream(work_directory + "/" + filename);

    for (uint32_t pid = 0; pid < partitions.size(); pid++) {
        auto partition_nodes = this -> partitions[pid].get_elems();

        for (auto sg_id: *partition_nodes) {
            ofs << sg_id << ',';
        }

        delete partition_nodes;
        ofs << "\n";
    }

    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(info) << "Write to file: Done in " << time_ms << "ms";
}
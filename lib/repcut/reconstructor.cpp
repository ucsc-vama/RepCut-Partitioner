//
// Created by Haoyuan Wang on 12/15/22.
//

#include "reconstructor.h"
#include "dag.h"
#include "rcp_util.h"

#include <algorithm>
#include <fstream>
#include <unordered_set>

using namespace repcut;

// Reconstruct partitions by crawling the original DAG upstream from each
// sink's cone.  A non-sink cluster touches cone c iff its nodes are ancestors
// of sink(c); so {ancestors of sink(c) : coneIdToPartId[c] == pid} is exactly
// the set of nodes partition pid must simulate, including replicated nodes.
// This removes the dependence on ClusterGraph for reconstruction.
//
// vis is a per-partition unordered_set serving as both the BFS visited marker
// and the dedup container (the same ancestor may be reached through multiple
// downstream paths in the same partition).
void Reconstructor::construct(const int nparts,
                             DirectedAcyclicGraph* dag,
                             const std::vector<uint32_t>& coneIdToPartId) {
    BOOST_LOG_TRIVIAL(info) << "Reconstruct: Start";
    auto start = std::chrono::system_clock::now();

    assert(dag != nullptr);
    assert(coneIdToPartId.size() == dag->sinkNodes.size());

    this->partitions.clear();
    this->partitions.assign(nparts, std::vector<uint32_t>());

    std::unordered_set<uint32_t> vis;
    std::vector<uint32_t> fringe;
    std::vector<uint32_t> sinksForPart;
    // Heuristic reserves; resized below per partition.
    vis.reserve(1 << 14);
    fringe.reserve(1 << 14);

    for (uint32_t pid = 0; pid < static_cast<uint32_t>(nparts); ++pid) {
        auto& part = this->partitions[pid];

        // Seed: every sink assigned to this partition.
        sinksForPart.clear();
        for (uint32_t cone_id = 0; cone_id < coneIdToPartId.size(); ++cone_id) {
            if (coneIdToPartId[cone_id] == pid) {
                sinksForPart.push_back(dag->sinkNodes[cone_id]);
            }
        }

        vis.clear();
        fringe.clear();
        for (uint32_t s : sinksForPart) {
            if (vis.insert(s).second) {
                fringe.push_back(s);
            }
        }

        while (!fringe.empty()) {
            const uint32_t v = fringe.back();
            fringe.pop_back();

            // vis already contains v (we insert on push).  Skip invalid nodes
            // but still keep them in vis so we don't re-traverse their edges.
            if (!dag->graph[v].valid) continue;
            part.push_back(v);

            for (auto inEdges = boost::in_edges(v, dag->graph);
                 inEdges.first != inEdges.second; ++inEdges.first) {
                const auto u = static_cast<uint32_t>(boost::source(*inEdges.first, dag->graph));
                if (vis.insert(u).second) {
                    fringe.push_back(u);
                }
            }
        }

        // Match historical output order (ascending ids) so rcp_output.txt
        // remains diffable against prior runs.
        std::sort(part.begin(), part.end());
    }

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    BOOST_LOG_TRIVIAL(info) << "Reconstruct: Done in " << duration.count() << "ms";
}

PartitionStatistics* Reconstructor::reportPartitionStatus(DirectedAcyclicGraph* dag) {
    assert(!this->partitions.empty());

    auto ret = new PartitionStatistics();
    ret->nparts = this->partitions.size();

    // Whole-design totals (only valid nodes).
    for (auto vtxes = boost::vertices(dag->graph); vtxes.first != vtxes.second; ++vtxes.first) {
        const auto v = *vtxes.first;
        if (dag->graph[v].valid) {
            ret->sg_size++;
            ret->sg_weight += dag->graph[v].weight;
        }
    }

    uint32_t total_part_size = 0;
    float total_part_weight = 0;

    for (auto& part : this->partitions) {
        uint32_t part_size = 0;
        float part_weight = 0;

        for (auto& nid : part) {
            // `part` only contains valid ids (invalids are skipped during BFS),
            // but be defensive.
            if (dag->graph[nid].valid) {
                part_size += 1;
                part_weight += dag->graph[nid].weight;
            }
        }

        total_part_size += part_size;
        total_part_weight += part_weight;

        ret->partition_size.push_back(part_size);
        ret->partition_weights.push_back(part_weight);
    }

    ret->total_part_size = total_part_size;
    ret->replication_size = ret->total_part_size - ret->sg_size;
    ret->replication_rate_size = static_cast<float>(ret->replication_size) * 100.0f / ret->sg_size;
    ret->ib_factor_size = calculate_ib_factor(ret->partition_size);

    ret->total_part_weight = total_part_weight;
    ret->replication_weight = ret->total_part_weight - ret->sg_weight;
    ret->replication_rate_weight = static_cast<float>(ret->replication_weight) * 100.0f / ret->sg_weight;
    ret->ib_factor_weight = calculate_ib_factor(ret->partition_weights);

    return ret;
}

void Reconstructor::saveToFile(const char* filename) {
    BOOST_LOG_TRIVIAL(info) << "Write to file: Start";
    auto start = std::chrono::system_clock::now();

    auto ofs = std::ofstream(work_directory + "/" + filename);

    for (uint32_t pid = 0; pid < partitions.size(); pid++) {
        for (auto& sg_id : this->partitions[pid]) {
            ofs << sg_id << ',';
        }
        ofs << "\n";
    }

    ofs.close();

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    BOOST_LOG_TRIVIAL(info) << "Write to file: Done in " << duration.count() << "ms";
}
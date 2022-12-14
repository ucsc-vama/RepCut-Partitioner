//
// Created by Haoyuan Wang on 12/1/22.
//

#include "refiner.h"
#include "rcp_util.h"
#include <queue>
#include <set>
#include <chrono>

#include <iostream>
#include "SBitSet.h"

/*
 * Find all fringe nodes and save to (this -> fringe_nodes)
 * */
void FMRefiner::find_fringe_nodes() {
    BOOST_LOG_TRIVIAL(trace) << "Find fringe nodes: Start";
    auto start = std::chrono::system_clock::now();

    std::unordered_set<uint32_t> fringe_set;

    for (uint32_t node_id = 0; node_id < this -> hg -> nodes.size(); node_id++) {
        auto node_partition_id = this -> coneIdToPartId[node_id];
        auto& connecting_he = this -> hg -> nodes[node_id];

        for (auto he_id : connecting_he) {
            for (auto overlapping_node_id: this -> hg -> edges[he_id]) {
                auto overlapping_node_pid = this -> coneIdToPartId[overlapping_node_id];
                if (overlapping_node_pid != node_partition_id) {
                    // This is a fringe node
                    fringe_set.insert(node_id);
                    goto single_node_done;
                }
            }
        }
        single_node_done:
        assert(true);
    }

    this -> fringe_nodes.clear();
    this -> fringe_nodes.assign(fringe_set.begin(), fringe_set.end());

    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "Find fringe nodes: Done in " << time_ms << "ms";
}


/*
 * Update a single entry in (this -> he_conn_parts_cache)
 * This data structure holds partitions that each hyper edge (or non-root cluster) belongs to
 * */
void FMRefiner::update_hyper_edge_part_cache(uint32_t he_id) {
    std::vector<bool> conn_parts;
    conn_parts.assign(this -> nparts, false);

    for (auto overlapping_cone_id: this -> hg -> edges[he_id]) {
        auto cone_part_id = this -> coneIdToPartId[overlapping_cone_id];
        conn_parts[cone_part_id] = true;
    }

    this -> he_conn_parts_cache[he_id] = std::move(conn_parts);
}

/*
 * Calculate score if move given cone
 * */
MoveAction FMRefiner::calculate_action(uint32_t cone_id) {
    // Move current cone to the smallest partition
    MoveAction action{};
    uint32_t from_pid = this -> coneIdToPartId[cone_id];
    uint32_t to_pid = this -> partition_weight_ordered.back().second;
    assert(from_pid != to_pid);

    uint32_t largest_partition = this -> partition_weight_ordered[0].second;
    if (from_pid != largest_partition) {
        // This partition is not from the largest partition
        // Don't move
        action.valid = false;
        return action;
    }

    uint32_t from_overlapping = 0;
    uint32_t to_overlapping = 0;
    auto cone_weight = static_cast<uint32_t>(this -> cg -> weight[cone_id]);

    auto& connecting_he = this -> hg -> nodes[cone_id];
    for (auto he_id : connecting_he) {
        auto cluster_weight = this -> hg -> edgeWeight[he_id];
        // Update cone weight
        cone_weight += cluster_weight;

        if (this -> he_conn_parts_cache[he_id].empty()) {
            this -> update_hyper_edge_part_cache(he_id);
        }

        if (this -> he_conn_parts_cache[he_id][from_pid]) from_overlapping += cluster_weight;
        if (this -> he_conn_parts_cache[he_id][to_pid]) to_overlapping += cluster_weight;
    }

    assert(cone_weight > from_overlapping);
    assert(cone_weight > to_overlapping);

    uint32_t moveout_weight_reduction = cone_weight - from_overlapping;
    uint32_t movein_weight_inc = cone_weight - to_overlapping;
    int32_t move_extra_replication = static_cast<int32_t>(movein_weight_inc) - static_cast<int32_t>(moveout_weight_reduction);

    action.cone_id = cone_id;
    action.from_pid = from_pid;
    action.to_pid = to_pid;
    action.moveout_weight_reduction = moveout_weight_reduction;
    action.movein_weight_inc = movein_weight_inc;
    action.score = move_extra_replication;
    action.valid = true;

    return action;
}

/*
 * Incrementally update (this -> he_conn_parts_cache) based on iteration moves (this -> iteration_adopt_actions)
 * */
void FMRefiner::update_hyper_edge_part_cache_incremental() {
    std::unordered_set<uint32_t> he_to_update;

    for (auto& action: this -> iteration_adopt_actions) {
        auto& connecting_he = this -> hg -> nodes[action.cone_id];
        for (auto he_id : connecting_he) {
            he_to_update.insert(he_id);
        }
    }

    BOOST_LOG_TRIVIAL(info) << "This iteration requires to update " << he_to_update.size() << " hyper edges";

    for (auto& he_id: he_to_update) {
        this -> update_hyper_edge_part_cache(he_id);
    }
}

/*
 * Update (this -> partition_weight_ordered)
 * */
void FMRefiner::update_partition_weight_ordered() {
    this -> partition_weight_ordered.clear();
    for (uint32_t i = 0; i < this -> nparts; i++) {
        auto weight = this -> partition_weights[i];
        this -> partition_weight_ordered.emplace_back(weight, i);
    }
    std::sort(this -> partition_weight_ordered.begin(), this -> partition_weight_ordered.end(), std::greater<>());
}

/*
 * Incrementally update partition weights (this -> partition_weights)
 * based on iteration moves (this -> iteration_adopt_actions)
 * */
void FMRefiner::update_partition_weights_n_cache_incremental() {
    std::unordered_set<uint32_t> affected_he;

    // Collect all affected hyper edges
    for (auto& action: this -> iteration_adopt_actions) {
        auto& connecting_he = this -> hg -> nodes[action.cone_id];
        for (auto he_id: connecting_he) {
            affected_he.insert(he_id);
        }
    }

    // Remove affected hyper edge weights
    for (auto& he_id: affected_he) {
        auto cluster_weight = this -> hg -> edgeWeight[he_id];
        for (uint32_t pid = 0; pid < this -> nparts; pid++) {
            if (this -> he_conn_parts_cache[he_id][pid]) {
                this -> partition_weights[pid] -= cluster_weight;
            }
        }
    }

    // Remove affected (bare) cluster weights
    for (auto& action: this -> iteration_adopt_actions) {
        auto cluster_id = action.cone_id;
        auto cluster_pid_old = action.from_pid;
        auto cluster_weight = static_cast<uint32_t>(this -> cg -> weight[cluster_id]);
        this -> partition_weights[cluster_pid_old] -= cluster_weight;
    }

    // Update part cache
    this -> update_hyper_edge_part_cache_incremental();

    // Add new hyper edge weights
    for (auto& he_id: affected_he) {
        auto cluster_weight = this -> hg -> edgeWeight[he_id];

        for (uint32_t pid = 0; pid < this -> nparts; pid++) {
            if (this -> he_conn_parts_cache[he_id][pid]) {
                this -> partition_weights[pid] += cluster_weight;
            }
        }
    }

    // Add affected (bare) cluster weights
    for (auto& action: this -> iteration_adopt_actions) {
        auto cluster_id = action.cone_id;
        auto cluster_pid_new = action.to_pid;
        auto cluster_weight = static_cast<uint32_t>(this -> cg -> weight[cluster_id]);
        this -> partition_weights[cluster_pid_new] += cluster_weight;
    }

    this -> update_partition_weight_ordered();
}

/*
 * Calculate partition weight (this -> partition_weights)
 * This is not cheap
 * */
void FMRefiner::update_partition_weights() {
    this -> partition_weights.clear();
    this -> partition_weights.assign(this -> nparts, 0);

    for (uint32_t cluster_id = 0; cluster_id < this -> cg -> clusters.size(); cluster_id++) {
        // For every cluster:
        auto cluster_weight = static_cast<uint32_t>(this -> cg -> weight[cluster_id]);
        std::unordered_set<uint32_t> cluster_partitions;

        // Pick up 1 node from this cluster
        uint32_t cone_idx = cg -> clusters[cluster_id][0];
        for (auto& cone_id: this -> cg -> idToConeId[cone_idx]) {
            auto pid = this -> coneIdToPartId[cone_id];
            cluster_partitions.insert(pid);
        }

        for (auto& pid: cluster_partitions) {
            this -> partition_weights[pid] += cluster_weight;
        }
    }

    this -> update_partition_weight_ordered();
}

/*
 * Move given cone
 * */
void FMRefiner::move_cone(const MoveAction& action) {
    assert(this -> coneIdToPartId[action.cone_id] == action.from_pid);
    this -> coneIdToPartId[action.cone_id] = action.to_pid;
}

/*
 * Refiner entry point
 * */
void FMRefiner::refine() {
    float ib_factor = 0.0f;
    uint32_t iterations = 0;
    std::unordered_set<uint32_t> moved_cones;
    std::priority_queue<MoveAction> pq;
    std::vector<uint32_t> partition_weight_estimate;
    std::vector<uint32_t> partition_weight_estimate_bkp;

    // Init: 1. clear cache; 2. calculate partition weights
    this -> he_conn_parts_cache.assign(this -> hg -> numEdges, std::vector<bool>());
    this -> update_partition_weights();

    while (true) {
        BOOST_LOG_TRIVIAL(trace) << "Refiner iteration " << iterations;

        // Clear per-iteration data
        this -> iteration_adopt_actions.clear();
        // Clear pq
        while(!pq.empty()) pq.pop();
        // Init estimated weights
        partition_weight_estimate = this -> partition_weights;


        // 1. Find all fringe nodes
        this -> find_fringe_nodes();
        BOOST_LOG_TRIVIAL(trace) << "Found " << this -> fringe_nodes.size() << " fringe nodes";


        // 2. calculate action for fringe
        BOOST_LOG_TRIVIAL(trace) << "Calculate actions: Start";
        auto start = std::chrono::system_clock::now();

        for (auto& cid: this -> fringe_nodes) {
            auto action = this ->calculate_action(cid);
            if (action.valid) {
                pq.push(action);
            }
        }
        BOOST_LOG_TRIVIAL(trace) << "Found " << pq.size() << " move candidates";

        auto stop = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        uint64_t time_ms = duration.count();
        BOOST_LOG_TRIVIAL(trace) << "Calculate actions: Done in " << time_ms << "ms";


        if (pq.empty()) {
            BOOST_LOG_TRIVIAL(trace) << "Refiner break due to no possible move";
            break;
        }


        // 3. move
        BOOST_LOG_TRIVIAL(trace) << "Move cones: Start";
        auto move_start = std::chrono::system_clock::now();

        auto last_ib_factor = calculate_ib_factor(this -> partition_weights);
        while (!pq.empty()) {
            auto action = pq.top();
            pq.pop();

            // Ignore duplicate move
            if (moved_cones.contains(action.cone_id)) {
                continue;
            }

            // Estimate weights after move
            assert(partition_weight_estimate[action.from_pid] > action.moveout_weight_reduction);
            partition_weight_estimate_bkp = partition_weight_estimate;
            partition_weight_estimate[action.from_pid] -= action.moveout_weight_reduction;
            partition_weight_estimate[action.to_pid] += action.movein_weight_inc;

            auto ib_factor_estimate = calculate_ib_factor(partition_weight_estimate);

            if (ib_factor_estimate > last_ib_factor) {
                // TODO: break or continue?
                // Move would hurt balance, bypass

                // Recover
                partition_weight_estimate = partition_weight_estimate_bkp;
                continue;
            }

            if (ib_factor_estimate <= this -> target_ib_factor) {
                // Possibly reach the target.
                break;
            }

            // Move!
            last_ib_factor = ib_factor_estimate;
            this -> move_cone(action);
            this -> iteration_adopt_actions.push_back(action);
            moved_cones.insert(action.cone_id);
        }
        BOOST_LOG_TRIVIAL(trace) << "Move " << this -> iteration_adopt_actions.size() << " cones";

        auto move_stop = std::chrono::system_clock::now();
        auto move_duration = std::chrono::duration_cast<std::chrono::milliseconds>(move_stop - move_start);
        uint64_t move_time_ms = move_duration.count();
        BOOST_LOG_TRIVIAL(trace) << "Move cones: Done in " << move_time_ms << "ms";


        // 4. update
        BOOST_LOG_TRIVIAL(trace) << "Update: start";
        auto update_start = std::chrono::system_clock::now();

        this -> update_partition_weights_n_cache_incremental();

        auto update_stop = std::chrono::system_clock::now();
        auto update_duration = std::chrono::duration_cast<std::chrono::milliseconds>(update_stop - update_start);
        uint64_t update_time_ms = update_duration.count();
        BOOST_LOG_TRIVIAL(trace) << "Update: Done in " << update_time_ms << "ms";


        // Current iteration complete
        iterations ++;

        // Check balance status and decide what to do next
        ib_factor = calculate_ib_factor(this -> partition_weights);
        BOOST_LOG_TRIVIAL(trace) << "Current ib factor: " << ib_factor;

        if (ib_factor <= this -> target_ib_factor) {
            BOOST_LOG_TRIVIAL(trace) << "Refiner complete due to target meet";
            break;
        }
        if (iterations >= this -> max_iterations) {
            BOOST_LOG_TRIVIAL(trace) << "Refiner complete due to too much iterations";
            break;
        }
        if (this -> iteration_adopt_actions.empty()) {
            BOOST_LOG_TRIVIAL(trace) << "Refiner complete (no more feasible move)";
            break;
        }
    }

    BOOST_LOG_TRIVIAL(info) << "Refiner complete in " << iterations << " iterations";
    BOOST_LOG_TRIVIAL(info) << "Reach ib factor " << ib_factor;
}


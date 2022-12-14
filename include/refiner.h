//
// Created by Haoyuan Wang on 12/1/22.
//

#ifndef RCP_REFINER_H
#define RCP_REFINER_H

#include "rcp_common.h"
#include "dag.h"
#include "cluster_graph.h"
#include "hyper_graph.h"

class MoveAction {
public:
    uint32_t cone_id;

    uint32_t from_pid;
    uint32_t to_pid;

    uint32_t moveout_weight_reduction;
    uint32_t movein_weight_inc;

    int32_t score;

    bool valid;

    bool operator< (MoveAction const& other) const
    {
        return this -> score > other.score;
    }
};


class Refiner {
public:
    ClusterGraph* cg = nullptr;
    HyperGraph* hg = nullptr;

    uint32_t nparts = 0;
    float target_ib_factor = 0.1f;

    std::vector<uint32_t> coneIdToPartId;

    virtual void refine() = 0;
};


class FMRefiner: public Refiner {
private:
    std::vector<uint32_t> fringe_nodes;
    std::vector<uint32_t> partition_weights;
    std::vector<MoveAction> iteration_adopt_actions;
    std::vector<std::vector<bool>> he_conn_parts_cache;
    std::vector<std::pair<uint32_t, uint32_t>> partition_weight_ordered;


    void find_fringe_nodes();

    MoveAction calculate_action(uint32_t cone_id);

    void move_cone(const MoveAction& gain);

    void update_hyper_edge_part_cache(uint32_t he_id);
    void update_hyper_edge_part_cache_incremental();

    void update_partition_weight_ordered();
    void update_partition_weights();
    void update_partition_weights_n_cache_incremental();

public:
    uint32_t max_iterations = 10000;
    void refine() override;
};

#endif //RCP_REFINER_H

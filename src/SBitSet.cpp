//
// Created by Haoyuan Wang on 11/13/22.
//

#include "SBitSet.h"
#include <bit>

SBitSet::SBitSet(const std::vector<const SBitSet*>& srcs) {
    uint32_t max_blks = 0;
    for (auto src: srcs) {
        this -> max_elem = std::max(this -> max_elem, src -> max_elem);
        max_blks = std::max(max_blks, static_cast<uint32_t>(src -> dat.size()));
    }

    // grow data
    this -> dat.assign(max_blks, 0);

    // save data
    for (uint32_t blk_id = 0; blk_id < max_blks; blk_id++) {
        uint64_t blk = 0;
        for (auto src: srcs) {
            if (blk_id < src -> dat.size()) {
                blk |= src -> dat[blk_id];
            }
        }
        this -> dat[blk_id] = blk;
    }

    // update num_elem
    this -> num_elem = this -> calculate_size();
}

bool SBitSet::contains(uint32_t elem) {
    if (elem > max_elem) {
        return false;
    }
    uint32_t dat_pos = elem / 64;
    uint32_t bit_pos = elem % 64;
    uint64_t mask = 0x1l << bit_pos;

    return (dat[dat_pos] & mask) != 0;
}

void SBitSet::insert(uint32_t elem) {
    uint32_t dat_pos_new = elem / 64;
    uint32_t bit_pos_new = elem % 64;


    if (dat_pos_new >= dat.size()) {
        uint32_t space_to_grow = dat_pos_new + 1 - dat.size();
        this -> dat.insert(this -> dat.end(), space_to_grow, 0);
    }

    uint64_t old_data = this -> dat[dat_pos_new];
    uint64_t new_data = old_data | (0x1l << bit_pos_new);
    this -> dat[dat_pos_new] = new_data;

    this -> max_elem = std::max(elem, this -> max_elem);
    this -> num_elem ++;
}

uint32_t SBitSet::size() const {
    return this -> num_elem;
}

uint32_t SBitSet::calculate_size() {
    uint32_t num_elms = 0;

    for (auto& blk : this -> dat) {
        num_elms += std::popcount(blk);
    }

    return num_elms;
}

std::vector<uint32_t>* SBitSet::get_elems() {
    // Note: Users are responsible for free the memory!!
    auto ret = new std::vector<uint32_t>;

    uint32_t cursor = 0;

    while (cursor <= max_elem) {
        uint32_t dat_pos = cursor / 64;
        uint32_t bit_pos = cursor % 64;

        if (this -> dat[dat_pos] == 0) {
            cursor += 64;
        } else {
            uint64_t mask = 0x1l << bit_pos;
            if ((this -> dat[dat_pos] & mask) != 0) {
                // Hit
                ret->push_back(cursor);
            }
            cursor ++;
        }
    }

    return ret;
}
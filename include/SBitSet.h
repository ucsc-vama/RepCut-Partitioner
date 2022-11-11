//
// Created by Haoyuan Wang on 11/8/22.
// Scala style BitSet
//

#ifndef RCP_SBITSET_H
#define RCP_SBITSET_H

#include <bit>
#include <vector>
#include <stdint.h>

#define MAX_ELEMS UINT32_MAX

class SBitSet {
private:
    std::vector<uint64_t> dat;
    uint32_t max_elem = 0;

public:
    SBitSet() {};

    bool contains(uint32_t elem) {
        if (elem > max_elem) {
            return false;
        }
        uint32_t dat_pos = elem / 64;
        uint32_t bit_pos = elem % 64;
        uint64_t mask = 0x1l << bit_pos;

        return (dat[dat_pos] & mask) != 0;
    }

    void insert(uint32_t elem) {
        uint32_t dat_pos_new = elem / 64;
        uint32_t bit_pos_new = elem % 64;

        uint32_t dat_pos_old = this -> max_elem / 64;

        if (dat_pos_new > dat_pos_old) {
            this -> dat.insert(this -> dat.end(), (dat_pos_new - dat_pos_old), 0);
        }

        uint64_t old_data = this -> dat[dat_pos_new];
        uint64_t new_data = old_data & (0x1l << bit_pos_new);
        this -> dat[dat_pos_new] = new_data;

        this -> max_elem = std::max(elem, this -> max_elem);
    }

    uint32_t size() {
        uint32_t num_elms = 0;

        for (auto& blk : this -> dat) {
            num_elms += std::popcount(blk);
        }

        return num_elms;
    }

    std::vector<uint32_t>* get_elems() {
        // Note: Users are responsible for free the memory!!
        auto ret = new std::vector<uint32_t>;

        uint32_t cursor = 0;

        while (cursor < max_elem) {
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
};

#endif //RCP_SBITSET_H

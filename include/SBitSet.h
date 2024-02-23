//
// Created by Haoyuan Wang on 11/8/22.
// Scala style BitSet
//

#ifndef RCP_SBITSET_H
#define RCP_SBITSET_H

#include "rcp_common.h"

#define MAX_ELEMS UINT32_MAX

namespace repcut {
    class SBitSet {
    private:
        std::vector<uint64_t> dat;
        uint32_t max_elem = 0;
        uint32_t num_elem = 0;

    public:
        SBitSet() = default;
        // Build bitset from multiple source
        explicit SBitSet(const std::vector<const SBitSet*>& srcs);

        bool contains(uint32_t elem);

        void insert(uint32_t elem);

        [[nodiscard]] uint32_t size() const;
        [[nodiscard]] uint32_t calculate_size();

        [[nodiscard]] std::vector<uint32_t>* get_elems();

    };
}



#endif //RCP_SBITSET_H

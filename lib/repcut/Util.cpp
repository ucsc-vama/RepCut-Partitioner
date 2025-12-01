//
// Created by Haoyuan Wang on 11/14/22.
//

#include "Util.h"

#include <algorithm>
#include <numeric>

namespace repcut {
    float calculate_ib_factor(std::vector<uint32_t>& dat) {
        uint32_t total = std::accumulate(dat.begin(), dat.end(), static_cast<uint32_t>(0));
        uint32_t max = *std::max_element(dat.begin(), dat.end());
        uint32_t avg = total / dat.size();

        return static_cast<float>(max - avg) / static_cast<float>(avg);
    }


    float calculate_ib_factor(std::vector<float>& dat) {
        float total = std::accumulate(dat.begin(), dat.end(), static_cast<float>(0));
        float max = *std::max_element(dat.begin(), dat.end());
        float avg = total / static_cast<float>(dat.size());

        return static_cast<float>(max - avg) / static_cast<float>(avg);
    }
}
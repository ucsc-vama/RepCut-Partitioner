//
// Created by Haoyuan Wang on 11/14/22.
//

#include <numeric>

float calculate_ib_factor(std::vector<uint32_t>& dat) {
    uint32_t total = std::accumulate(dat.begin(), dat.end(), static_cast<uint32_t>(0));
    uint32_t max = *std::max_element(dat.begin(), dat.end());
    uint32_t avg = total / dat.size();

    return static_cast<float>(max - avg) / static_cast<float>(avg);
}
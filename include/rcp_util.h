//
// Created by Haoyuan Wang on 11/14/22.
//

#ifndef RCP_RCP_UTIL_H
#define RCP_RCP_UTIL_H

#include "rcp_common.h"

namespace repcut {
    float calculate_ib_factor(std::vector<uint32_t>& dat);
    float calculate_ib_factor(std::vector<float>& dat);
}


#endif //RCP_RCP_UTIL_H

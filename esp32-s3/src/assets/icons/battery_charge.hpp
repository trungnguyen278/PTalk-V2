#pragma once

/**
 * File:    battery_charge.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

#include <cstdint>

namespace asset::icon {
#ifndef ICON
#define ICON
struct Icon {
    int w;
    int h;
    const uint8_t* rle_data;
};
#endif

extern const Icon BATTERY_CHARGE;

} // namespace asset::icon

#pragma once

#include "aliases.hpp"

inline uint32_t ceilToNextMultiple(uint32_t value, uint32_t step)
{
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

inline uint32_t bitWidth(uint32_t value)
{
    if(value == 0)
        return 0;
    else
    {
        uint32_t w = 0;
        while (value >>= 1)
            ++w;
        return w;
    }
}

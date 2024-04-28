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

inline uint16_t float32ToFloat16(float value)
{
    uint32_t f32 = *reinterpret_cast<uint32_t*>(&value);
    uint16_t sign = (f32 >> 31) & 0x1;
    uint16_t exp = (f32 >> 23) & 0xFF;
    uint16_t mantissa = (f32 >> 13) & 0x3FF;
    if (exp == 0xFF)
    {
        // Special case: NaN or Infinity
        exp = 0x1F;
        mantissa = (f32 & 0x7FFFFF) ? 0x200 : 0;
    }
    else if (exp == 0)
    {
        // Denormalized number
        mantissa = 0;
    }
    else
    {
        // Normalized number: round to nearest even
        if ((f32 & 0x1FFF) > 0x1000)
            f32 += 0x8000; // Round up
        mantissa = (f32 >> 13) & 0x3FF;
        if (mantissa == 0x400)
        {
            mantissa = 0;
            exp++;
        }
    }
    return (sign << 15) | ((exp - 112) << 10) | mantissa;
}

inline std::vector<uint16_t> convertFloat32ToFloat16(const std::vector<float>& buffer)
{
    std::vector<uint16_t> result(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        result[i] = float32ToFloat16(buffer[i]);
    }
    return result;
}

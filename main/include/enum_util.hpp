#pragma once
#include <string_view>
#include <magic_enum.hpp>

template <typename To, typename From>
constexpr To conv_enum(From value)
{
    return *reinterpret_cast<To*>(&value);
}

template <typename To, typename From>
constexpr std::string_view conv_enum_str(From value)
{
    return magic_enum::enum_name(conv_enum<To>(value));
}

template <typename T>
constexpr std::string_view conv_enum_str(T value)
{
    return magic_enum::enum_name(value);
}

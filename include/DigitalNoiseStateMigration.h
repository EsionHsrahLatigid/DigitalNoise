#pragma once

#include <array>
#include <cstddef>

namespace digitalnoise
{

inline constexpr std::size_t legacyStateParameterCount = 10u;
inline constexpr std::size_t currentStateParameterCount = 12u;

inline std::array<float, currentStateParameterCount> migrateLegacyStateParameters (
    const std::array<float, legacyStateParameterCount>& legacy,
    float formatSmashDefault) noexcept
{
    std::array<float, currentStateParameterCount> values {};
    for (std::size_t i = 0; i < 8u; ++i)
        values[i] = legacy[i];

    values[8] = 0.0f;
    values[9] = formatSmashDefault;
    values[10] = legacy[8];
    values[11] = legacy[9];
    return values;
}

} // namespace digitalnoise

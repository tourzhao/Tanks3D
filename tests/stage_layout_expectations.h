#ifndef TANKS3D_STAGE_LAYOUT_EXPECTATIONS_H
#define TANKS3D_STAGE_LAYOUT_EXPECTATIONS_H

#include "game/stage_generator.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tanks3d_test
{
namespace detail
{
inline void mixStageLayoutByte(std::uint64_t &signature,
                               unsigned char value)
{
    signature ^= value;
    signature *= 1099511628211ULL;
}

inline std::uint64_t initialStageLayoutSignature()
{
    std::uint64_t signature = 14695981039346656037ULL;
    mixStageLayoutByte(signature, 1U);
    mixStageLayoutByte(
        signature,
        static_cast<unsigned char>(tanks3d::game::kMapSize));
    return signature;
}
} // namespace detail

inline std::uint64_t stageLayoutSignature(
    const tanks3d::game::StageTileGrid &tiles)
{
    // Versioned FNV-1a over explicit bytes is stable across standard-library
    // implementations and host endianness. The stage number is intentionally
    // excluded so uniqueness checks can detect duplicate layouts.
    std::uint64_t signature = detail::initialStageLayoutSignature();
    for (int row = 0; row < tanks3d::game::kMapSize; ++row)
    {
        for (int column = 0; column < tanks3d::game::kMapSize; ++column)
        {
            const char tile =
                tiles[static_cast<std::size_t>(row)]
                     [static_cast<std::size_t>(column)];
            detail::mixStageLayoutByte(
                signature, static_cast<unsigned char>(tile));
            detail::mixStageLayoutByte(
                signature,
                static_cast<unsigned char>(tile == '#' ? 0x0fU : 0U));
        }
    }
    return signature;
}

// Independently decoded original NES opening layouts, including the eight
// base-enclosure cells. Source verification precedes acceptance of these hashes.
inline constexpr std::array<std::uint64_t, tanks3d::game::kStageCount>
    kExpectedStageLayoutSignatures{{
        0x5c2ce11a49ccd52aULL, 0x0dcbe3883e982622ULL,
        0x01180e57eab577faULL, 0x2378c573fad5522aULL,
        0xc20c8100afd013d6ULL, 0x5be7f031849aa806ULL,
        0x659a20e3ba472ec2ULL, 0x7ebe201e58e5bceeULL,
        0xfcce2712ed06d1a2ULL, 0xff0d3262c9a30822ULL,
        0x36628f0b3f81dde6ULL, 0xa8ad2363fd49bf2eULL,
        0x82dd400a270ed8b2ULL, 0xb62a7e8d792fbdeaULL,
        0x6a24e40977e816d2ULL, 0xa7c00587eae9371eULL,
        0xb16fe5885a25c4c6ULL, 0x9ae3cd9bcf80378aULL,
        0xd2e9791d1a1f3a5aULL, 0xac897e98f50952faULL,
        0x67553e4bb9056f6aULL, 0x250f61a43f836c1aULL,
        0x3d6ae8c6d12056e2ULL, 0xefbc6e40d588c7f6ULL,
        0xf59e7afaa6ec416aULL, 0x17c48a7ff6ea378aULL,
        0x2d0afecb03d9c22aULL, 0x9d31ac8a62dea216ULL,
        0xaee07e491352e0a2ULL, 0x0dc6ee424385d54aULL,
        0x97a0c90325a5c17aULL, 0x3f81b94785a7d58eULL,
        0x45fca3d88892eda2ULL, 0xc18619816ee7edfeULL,
        0x46cf515593993312ULL}};
} // namespace tanks3d_test

#endif

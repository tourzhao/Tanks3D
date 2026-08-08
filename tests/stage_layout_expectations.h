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

inline constexpr std::array<std::uint64_t, tanks3d::game::kStageCount>
    kExpectedStageLayoutSignatures{{
        0x6393262a9b657ff5ULL, 0x5ac8724c44a81688ULL,
        0x0f4ec27c9fe51a90ULL, 0x6c64f0942c79f353ULL,
        0xef85299a443a8efeULL, 0x3d78b48b0aa0bedfULL,
        0x49da66e5663a53a6ULL, 0x9022e2e55550bbbcULL,
        0x48817da502745313ULL, 0x82644fe460c51cddULL,
        0x05f2f3b96dd7020aULL, 0x2085c69fbf2f4182ULL,
        0x0041f98bcf65b2a6ULL, 0x4d623bb6d029aafaULL,
        0x89a2016a73268e5aULL, 0xea1953cab857d92cULL,
        0x9e8018bfb3a8cbefULL, 0x089f8343074bee8eULL,
        0x6ae496ffe82e3574ULL, 0xa9324bca00049099ULL,
        0x3b470dcc7869b2d7ULL, 0xad31daace3d3006eULL,
        0xab30cbaa5878c6beULL, 0x523fcae0815878e4ULL,
        0x657ba266e04f2d0dULL, 0x30b84e4cf366ec64ULL,
        0xe9a0fc6940ec3bf3ULL, 0xbcfaa5456469a5f6ULL,
        0xb9e2331590b37a9eULL, 0xda029e74ad4de6bbULL,
        0x5156b3b132b3be3fULL, 0x75755499161ecd98ULL,
        0x06ef11717093c5a6ULL, 0xf86308fe1a1261bcULL,
        0x815b4db1307e9a84ULL}};
} // namespace tanks3d_test

#endif

#ifndef TANKS3D_STAGE_MAP_EXPECTATIONS_H
#define TANKS3D_STAGE_MAP_EXPECTATIONS_H

#include "game/stage_map.h"
#include "stage_layout_expectations.h"

#include <cstdint>

namespace tanks3d_test
{
inline std::uint64_t stageLayoutSignature(
    const tanks3d::game::StageMap &map)
{
    std::uint64_t signature = detail::initialStageLayoutSignature();
    for (int row = 0; row < tanks3d::game::kMapSize; ++row)
    {
        for (int column = 0; column < tanks3d::game::kMapSize; ++column)
        {
            detail::mixStageLayoutByte(
                signature,
                static_cast<unsigned char>(map.tile(row, column)));
            detail::mixStageLayoutByte(
                signature, map.brickMask(row, column));
        }
    }
    return signature;
}
} // namespace tanks3d_test

#endif

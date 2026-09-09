#include "game/stage_generator.h"

#include "game/classic_stage_layouts.h"

#include <algorithm>

namespace tanks3d::game
{
StageTileGrid StageGenerator::generate(int canonicalStage)
{
    const auto &rows = detail::kClassicStageLayouts[
        static_cast<std::size_t>(canonicalStage - 1)];
    StageTileGrid tiles{};
    for (std::size_t row = 0; row < rows.size(); ++row)
        std::copy(rows[row].begin(), rows[row].end(), tiles[row].begin());
    return tiles;
}
} // namespace tanks3d::game

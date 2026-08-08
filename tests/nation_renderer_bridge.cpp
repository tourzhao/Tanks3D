#include "core/nation.h"
#include "wwii_tank_model.h"
#include "tank_assets.h"

#include <type_traits>

static_assert(std::is_same_v<tanks3d::core::Nation,
                             wwii_tank_model::Nation>);

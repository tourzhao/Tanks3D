#include "bonus_assets.h"
#include "game/bonus_rules.h"

#include <type_traits>

static_assert(std::is_same_v<bonus_assets::Type,
                             tanks3d::game::BonusType>,
              "renderer must consume the shared bonus type");
static_assert(std::is_same_v<bonus_assets::Pickup,
                             tanks3d::game::Pickup>,
              "renderer must consume the game-owned pickup state");
static_assert(std::is_same_v<decltype(bonus_assets::Pickup{}.position),
                             tanks3d::core::XZ>,
              "pickup position must use the raylib-free core XZ type");
static_assert(bonus_assets::kPickupLifetime ==
                  tanks3d::game::kPickupLifetime,
              "renderer must consume the shared pickup lifetime");
static_assert(bonus_assets::kClassicPickupTypeCount ==
                  tanks3d::game::kClassicPickupTypeCount &&
                  bonus_assets::kBandageWeight ==
                      tanks3d::game::kBandageWeight,
              "renderer must consume the shared pickup weighting");

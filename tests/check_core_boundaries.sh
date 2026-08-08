#!/bin/sh
set -eu

test_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
core_directory="$test_directory/../src/core"
game_directory="$test_directory/../src/game"
app_directory="$test_directory/../src/app"
audio_directory="$test_directory/../src/audio"

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"](raylib|raymath|rlgl)(\.h)?[>"]' \
    "$core_directory" "$game_directory" "$app_directory" "$audio_directory"
then
    echo "layer dependency check failed: raylib-family include found" >&2
    exit 1
fi

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(wwii_tank_model|tank_assets|battle_fx|bonus_assets|environment_assets|post_process|test_support)(\.h)?[>"]|#[[:space:]]*include[[:space:]]*"\.\./' \
    "$core_directory" "$game_directory" "$app_directory" "$audio_directory"
then
    echo "layer dependency check failed: renderer/test include found" >&2
    exit 1
fi

if grep -R -n -E \
    '(wwii_tank_model|bonus_assets|environment_assets)::|\b(BattleFx|TankAssets)\b' \
    "$core_directory" "$game_directory" "$app_directory" "$audio_directory"
then
    echo "layer dependency check failed: renderer type found" >&2
    exit 1
fi

if grep -R -n -E \
    '(^|[^[:alnum:]_])(IsKey[A-Za-z0-9_]*|GetTime|Draw[A-Za-z0-9_]*|BeginMode3D|EndMode3D|BeginDrawing|EndDrawing|Load(Sound|MusicStream)|Unload(Sound|MusicStream)|Play(Sound|MusicStream)|StopMusicStream|UpdateMusicStream)[[:space:]]*\(' \
    "$core_directory" "$game_directory" "$app_directory" "$audio_directory"
then
    echo "layer dependency check failed: platform/presentation call found" >&2
    exit 1
fi

if grep -R -n -E \
    '(^|[^[:alnum:]_])spawn(MuzzleFlash|Impact|Explosion|BrickBurst)[[:space:]]*\(' \
    "$core_directory" "$game_directory" "$audio_directory"
then
    echo "rule dependency check failed: core/game/audio must not call presentation spawners" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"](raylib|raymath|rlgl)(\.h)?[>"]|#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(wwii_tank_model|tank_assets|battle_fx|bonus_assets|environment_assets|post_process)(\.h)?[>"]|(^|[^[:alnum:]_])(Vector3|Color|Model|Shader|BattleFx|TankAssets)([^[:alnum:]_]|$)|(wwii_tank_model|bonus_assets|environment_assets)::' \
    "$app_directory/command_side_effect_sink.h" \
    "$app_directory/command_side_effect_dispatch.h" \
    "$app_directory/command_side_effect_dispatch.cpp"
then
    echo "app dependency check failed: command side-effect boundary must remain raylib/renderer-free" >&2
    exit 1
fi

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(app|audio)/' \
    "$core_directory" "$game_directory"
then
    echo "rule dependency check failed: core/game must not depend on app/audio" >&2
    exit 1
fi

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?command_side_effect_(sink|dispatch)\.h[>"]' \
    "$core_directory" "$game_directory"
then
    echo "rule dependency check failed: core/game must not depend on the app side-effect boundary" >&2
    exit 1
fi

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(app|core|game)/' \
    "$audio_directory"
then
    echo "audio dependency check failed: audio values must remain foundational" >&2
    exit 1
fi

if grep -R -n -E \
    '#[[:space:]]*include[[:space:]]*[<"](game/|[^>"]*/game/)' \
    "$core_directory"
then
    echo "core dependency check failed: core must not depend on game" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?stage_map\.h[>"]' \
    "$game_directory/stage_generator.h" \
    "$game_directory/stage_generator.cpp"
then
    echo "game dependency check failed: StageGenerator must not depend on StageMap" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?combat_system\.h[>"]' \
    "$game_directory/bonus_rules.h" \
    "$game_directory/entities.h" \
    "$game_directory/game_event.h" \
    "$game_directory/settlement_system.h" \
    "$game_directory/settlement_system.cpp" \
    "$game_directory/stage_generator.h" \
    "$game_directory/stage_generator.cpp" \
    "$game_directory/stage_map.h" \
    "$game_directory/stage_map.cpp"
then
    echo "game dependency check failed: foundational game modules must not depend on CombatSystem" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(combat_system|stage_map)\.h[>"]' \
    "$game_directory/bonus_system.h" \
    "$game_directory/bonus_system.cpp" \
    "$game_directory/player_system.h" \
    "$game_directory/player_system.cpp"
then
    echo "game dependency check failed: BonusSystem/PlayerSystem must not depend on CombatSystem or StageMap" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?(bonus_rules|bonus_system|enemy_system|entities|game_event|settlement_system|stage_generator)\.h[>"]' \
    "$game_directory/player_system.h" \
    "$game_directory/player_system.cpp"
then
    echo "game dependency check failed: PlayerSystem must remain a scalar lifecycle/control/movement/fire module" >&2
    exit 1
fi

if grep -n -E \
    '#[[:space:]]*include[[:space:]]*[<"]([^>"]*/)?stage_map\.h[>"]' \
    "$game_directory/settlement_system.h" \
    "$game_directory/settlement_system.cpp"
then
    echo "game dependency check failed: SettlementSystem must not depend on StageMap" >&2
    exit 1
fi

if grep -n -E '(^|[^[:alnum:]_])Vector3([^[:alnum:]_]|$)' \
    "$game_directory/bonus_system.h" \
    "$game_directory/bonus_system.cpp" \
    "$game_directory/player_system.h" \
    "$game_directory/player_system.cpp"
then
    echo "game dependency check failed: BonusSystem/PlayerSystem must use core values, not Vector3" >&2
    exit 1
fi

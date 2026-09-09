from pathlib import Path
import hashlib
import subprocess

output = Path('build/enemy-performance-20260909')
source = Path('src/main.cpp').read_text()
(output / 'profile-source-sha256.txt').write_text('\n'.join(
    f'{hashlib.sha256(Path(name).read_bytes()).hexdigest()}  {name}'
    for name in ('src/main.cpp', 'src/environment_assets.h', 'src/post_process.h',
                 'src/wwii_tank_model.h', 'src/base_model.h')) + '\n')
source = source.replace('"../tests/', '"../../tests/')
source = '#include "profile_timing.h"\n' + source
start = source.index('bool renderGame(')
end = source.index('\nstruct MenuSettings', start)
part = source[start:end]
def wrap(old, field):
    global part
    assert part.count(old) == 1, (field, part.count(old))
    part = part.replace(old, '{ nativeProfile::Scope timing(nativeProfile::sample.' + field + ');\n' + old + '\n}')
wrap('lighting.updateShadowMap(\n        [&game, &tankAssets]() { drawShadowCasters(game, tankAssets); });', 'shadow')
wrap('drawWorld(game, tankAssets, environment, terrainView);', 'world')
wrap('drawForestForeground(game.map(), environment,\n                             game.cameraYawDegrees(), terrainView);', 'forest')
wrap('postProcess.draw(viewTargets.targets[0], destination,\n                     static_cast<float>(GetTime()));', 'post')
wrap('EndDrawing();', 'endDrawing')
source = source[:start] + part + source[end:]
source = source.replace('        if (!shouldUpdate)\n            return;', '        if (!shouldUpdate)\n            return;\n        nativeProfile::sample.shadowRefreshed = true;', 1)
for old, field in (
    ('drawBrickTile(map, environment, row, column);', 'bricks'),
    ('drawSteelTile(row, column, false);', 'steel'),
    ('drawWaterTile(map, row, column);', 'water'),
    ('environment.drawForestStructure(\n                    map.stage(), row, column,\n                    forestEdgeMask(map, row, column));', 'trunks'),
):
    assert source.count(old) == 1, (field, source.count(old))
    source = source.replace(old, '{ nativeProfile::Scope timing(nativeProfile::sample.' + field + ');\n' + old + '\n}')
(output / 'profile-main.cpp').write_text(source)
objects = sorted(p for p in Path('build/obj').rglob('*.o') if p.name != 'main.o')
subprocess.run(['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                '-Isrc', '-I/opt/homebrew/include', str(output / 'native_profile.cpp'),
                *[str(p) for p in objects], '-L/opt/homebrew/lib', '-lraylib',
                '-framework', 'Cocoa', '-framework', 'GameController',
                '-framework', 'IOKit', '-framework', 'OpenGL',
                '-o', str(output / 'native-profile')], check=True)

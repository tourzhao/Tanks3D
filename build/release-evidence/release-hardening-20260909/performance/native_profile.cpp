#define main tanks3dProductionMain
#include "profile-main.cpp"
#undef main
#include <fstream>

int main(int argc, char **argv)
{
    using nativeProfile::Clock;
    const fs::path output = argc > 1 ? argv[1] : "build/enemy-performance-20260909/results";
    fs::create_directories(output);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1280, 720, "Tanks3D native frame profile");
    if (!IsWindowReady()) return 2;
    SetExitKey(KEY_NULL);
    SceneLighting lighting;
    lighting.load();
    TankAssets tanks;
    tanks.load("resources", lighting.shader(), lighting.depthShader());
    EnvironmentAssets environment;
    environment.load("resources");
    bonus_assets::Assets bonuses;
    bonuses.load("resources");
    PostProcess post;
    post.load();
    post.setPixelStyleEnabled(false);
    ViewTargets views;
    Game3D game("resources", 2048U);
    std::ofstream frames(output / "frames.csv");
    std::ofstream scenes(output / "scenes.csv");
    frames << "stage,view,frame,frame_ms,render_ms,shadow_refreshed,shadow_ms,world_ms,forest_ms,post_ms,end_drawing_ms,brick_ms,steel_ms,water_ms,trunks_ms\n";
    scenes << "stage,view,screen_w,screen_h,render_w,render_h,view_w,view_h,span,target_x,target_z,visible_forest,visible_steel,visible_water,enemies,pixel_style\n";
    frames << std::fixed << std::setprecision(6);
    scenes << std::fixed << std::setprecision(6);
    for (int stage : {1, 26})
    {
        for (int view = 0; view < 3; ++view)
        {
            const std::array<Nation, 2> playerNations{{
                view == 0 ? Nation::UnitedStates : Nation::Germany,
                Nation::UnitedStates}};
            if (!game.start(1, 10, stage, playerNations)) return 3;
            // Exit stage intro and retain normal enemies, original terrain and
            // the normal player model. Freeze simulation for repeatable views;
            // native wall-clock animations, input polling and presentation run.
            for (int frame = 0; frame < 500; ++frame)
                game.update(1.0f / 120.0f, {});
            if (true)
            {
                const XZ desired{13.0f, 13.0f};
                XZ legal = game.players()[0].position;
                float closest = 10000.0f;
                // Original two-cell corridors are centered on integer
                // coordinates. Include both integer and half-cell candidates.
                for (int row = 2; row <= 50; ++row)
                    for (int column = 2; column <= 50; ++column)
                    {
                        const XZ point{column * 0.5f, row * 0.5f};
                        const float distance = distanceSquared(point, desired);
                        if (distance >= closest || game.map().isInsideBase(point) ||
                            game.map().collidesWithTank(point, 0.875f)) continue;
                        closest = distance;
                        legal = point;
                    }
                Game3DTestAccess::recenterPlayer(game, 0, legal);
                Game3DTestAccess::refreshCameras(game);
                Game3DTestAccess::advanceCameras(game, 1.0f);
            }
            const char *viewName = view == 0 ? "German-Tiger" : view == 1 ? "USA-T95" : "USSR-KV5";
            const Nation wantedNation = view == 0 ? Nation::Germany :
                                        view == 1 ? Nation::UnitedStates : Nation::SovietUnion;
            std::vector<XZ> positions;
            const XZ playerPosition = game.players()[0].position;
            for (int i = 0; i < 4; ++i)
            {
                const XZ wanted{playerPosition.x + (i % 2 == 0 ? -3.0f : 3.0f),
                                playerPosition.z - 2.0f - (i / 2) * 3.0f};
                float closest = 10000.0f;
                XZ legal{};
                for (int row = 2; row <= 50; ++row)
                    for (int column = 2; column <= 50; ++column)
                    {
                        const XZ point{column * 0.5f, row * 0.5f};
                        const float distance = distanceSquared(point, wanted);
                        if (distance >= closest || game.map().isInsideBase(point) ||
                            game.map().collidesWithTank(point, 0.875f) ||
                            distanceSquared(point, playerPosition) < 4.0f) continue;
                        bool occupied = false;
                        for (XZ existing : positions)
                            occupied = occupied || distanceSquared(point, existing) < 4.0f;
                        if (occupied) continue;
                        legal = point; closest = distance;
                    }
                if (closest == 10000.0f) return 4;
                positions.push_back(legal);
                Enemy enemy;
                enemy.id = i * 2 + (view == 1 ? 0 : 1);
                enemy.type = tanks3d::game::kArmorEnemyType;
                enemy.armor = 4;
                enemy.position = legal;
                enemy.moving = true;
                if (game.enemyNation(enemy.id) != wantedNation) return 5;
                if (i == 0) Game3DTestAccess::installEnemyUpdateScenario(game, enemy);
                else Game3DTestAccess::appendEnemyUpdateScenario(game, enemy);
            }
            const auto render = [&]() {
                tanks.setAnimationClock(GetTime());
                nativeProfile::Scope timing(nativeProfile::sample.render);
                if (!renderGame(game, views, lighting, tanks, environment, bonuses, post, true))
                    throw std::runtime_error("render failed");
            };
            for (int warmup = 0; warmup < 60; ++warmup)
            {
                LaptopFramePacer pacer(Clock::now());
                render();
            }
            const Camera3D camera = game.cameraForPlayer(0);
            TerrainView visible(camera, views.widths[0], views.height);
            int forest = 0, steel = 0, water = 0;
            for (int row = 0; row < 26; ++row)
                for (int column = 0; column < 26; ++column)
                {
                    if (!visible.containsCell(row, column)) continue;
                    const char tile = game.map().tile(row, column);
                    forest += tile == '%'; steel += tile == '@'; water += tile == '~';
                }
            scenes << stage << ',' << viewName << ',' << GetScreenWidth() << ',' << GetScreenHeight()
                   << ',' << GetRenderWidth() << ',' << GetRenderHeight()
                   << ',' << views.widths[0] << ',' << views.height << ',' << camera.fovy
                   << ',' << camera.target.x << ',' << camera.target.z << ',' << forest
                   << ',' << steel << ',' << water << ',' << game.enemies().size() << ",0\n";
            const auto start = Clock::now();
            int frame = 0;
            while (nativeProfile::milliseconds(Clock::now() - start) < 5000.0)
            {
                const auto frameStart = Clock::now();
                nativeProfile::sample = {};
                {
                    LaptopFramePacer pacer(frameStart);
                    render();
                }
                const double frameMs = nativeProfile::milliseconds(Clock::now() - frameStart);
                const auto &sample = nativeProfile::sample;
                frames << stage << ',' << viewName << ',' << frame++ << ',' << frameMs
                       << ',' << sample.render << ',' << sample.shadowRefreshed << ',' << sample.shadow
                       << ',' << sample.world << ',' << sample.forest << ',' << sample.post
                       << ',' << sample.endDrawing << ',' << sample.bricks << ',' << sample.steel
                       << ',' << sample.water << ',' << sample.trunks << '\n';
            }
            std::cout << "SAMPLED stage=" << stage << " view=" << viewName << " frames=" << frame
                      << " native=" << GetRenderWidth() << 'x' << GetRenderHeight() << std::endl;
        }
    }
    views.release(); post.unload(); bonuses.unload(); environment.unload();
    clearSteelTileGeometryCache();
    tanks.unload(); lighting.unload(); CloseWindow();
}

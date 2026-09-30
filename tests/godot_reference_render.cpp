// Optional visual comparison tool. The native core and original raylib renderer
// share one production Game3D; this is not a second gameplay implementation.
#define main tanks3dReferenceUnusedApplicationMain
#include "../src/main.cpp"
#undef main
#include "../src/godot/sample_core.cpp"

#include <fstream>
#include <iostream>

namespace
{
struct Options
{
    fs::path output;
    int seed = 20260916;
    int stage = 1;
    int frames = 360;
    int captureAt = 300;
    bool idle = false;
    bool solo = false;
};

Options parseOptions(int argc, char **argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument(argv[index]);
        if (argument.rfind("--output=", 0) == 0)
            options.output = argument.substr(9);
        else if (argument.rfind("--stage=", 0) == 0)
            options.stage = std::stoi(argument.substr(8));
        else if (argument.rfind("--seed=", 0) == 0)
            options.seed = std::stoi(argument.substr(7));
        else if (argument.rfind("--frames=", 0) == 0)
            options.frames = std::stoi(argument.substr(9));
        else if (argument.rfind("--capture-at=", 0) == 0)
            options.captureAt = std::stoi(argument.substr(13));
        else if (argument == "--idle")
            options.idle = true;
        else if (argument == "--solo")
            options.solo = true;
        else
            throw std::invalid_argument("Unknown argument: " + argument);
    }
    if (options.output.empty() || options.stage < 1 || options.stage > 35 ||
        options.captureAt < 1 || options.frames < options.captureAt || options.frames > 36000)
        throw std::invalid_argument("Use --output=build/... with valid stage/frame settings");
    const auto destination = fs::absolute(options.output).lexically_normal();
    const auto generatedRoot = fs::absolute("build").lexically_normal();
    const auto relative = destination.lexically_relative(generatedRoot);
    if (relative.empty() || *relative.begin() == ".." || relative == ".")
        throw std::invalid_argument("Reference output must be a subdirectory of build/");
    options.output = destination;
    return options;
}

struct ReferenceRenderer
{
    SceneLighting lighting;
    PostProcess post;
    EnvironmentAssets environment;
    bonus_assets::Assets bonuses;
    TankAssets tanks;
    ViewTargets targets;
    bool ownsWindow = false;

    void open(const fs::path &resources)
    {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
        InitWindow(1280, 720, "Tanks 3D - original renderer comparison");
        if (!IsWindowReady())
            throw std::runtime_error("Unable to open raylib comparison window");
        ownsWindow = true;
        SetExitKey(KEY_NULL);
        SetTargetFPS(60);
        lighting.load();
        if (!post.load())
            throw std::runtime_error("Unable to load original post-process shader");
        environment.load(resources);
        bonuses.load(resources);
        tanks.load(resources, lighting.shader(), lighting.depthShader());
    }

    void render(Game3D &game, int frame, bool pixel, bool aiP2)
    {
        tanks.setAnimationClock(frame / 60.0);
        post.setPixelStyleEnabled(pixel);
        if (!renderGame(game, targets, lighting, tanks, environment, bonuses, post, false, "", aiP2))
            throw std::runtime_error("Original render target allocation failed");
    }

    ~ReferenceRenderer()
    {
        if (!ownsWindow)
            return;
        targets.release();
        tanks.unload();
        bonuses.unload();
        environment.unload();
        post.unload();
        lighting.unload();
        CloseWindow();
    }
};

Rectangle tankEnvelope(const Player &player, const Camera3D &camera)
{
    float left = 1280.0f, top = 720.0f, right = 0.0f, bottom = 0.0f;
    // A documented projected envelope, not an assertion about occupied pixels.
    for (float x : {-1.05f, 1.05f})
        for (float z : {-1.25f, 1.25f})
            for (float y : {0.0f, 1.55f})
            {
                const auto point = GetWorldToScreenEx(
                    {player.position.x + x, y, player.position.z + z}, camera, 1280, 720);
                left = std::min(left, point.x);
                right = std::max(right, point.x);
                top = std::min(top, point.y);
                bottom = std::max(bottom, point.y);
            }
    left = std::clamp(std::floor(left), 0.0f, 1280.0f);
    right = std::clamp(std::ceil(right), 0.0f, 1280.0f);
    top = std::clamp(std::floor(top), 0.0f, 720.0f);
    bottom = std::clamp(std::ceil(bottom), 0.0f, 720.0f);
    return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
}

void exportFrame(const fs::path &output, const std::string &suffix, Rectangle crop)
{
    Image frame = LoadImageFromScreen();
    if (!frame.data || frame.width != 1280 || frame.height != 720)
    {
        if (frame.data)
            UnloadImage(frame);
        throw std::runtime_error("Reference framebuffer must be native 1280x720; never rescale evidence");
    }
    const bool saved = ExportImage(frame, (output / (suffix + ".png")).string().c_str());
    bool savedCrop = true;
    if (crop.width > 0 && crop.height > 0)
    {
        Image region = ImageFromImage(frame, crop);
        savedCrop = region.data && ExportImage(region,
            (output / ("tank-native-" + suffix + ".png")).string().c_str());
        UnloadImage(region);
    }
    UnloadImage(frame);
    if (!saved || !savedCrop)
        throw std::runtime_error("Unable to write original renderer screenshots");
}

void capturePair(ReferenceRenderer &renderer, void *handle, const Options &options, int frame)
{
    auto &sample = tanks3d::godot_sample::get(handle);
    auto &game = dynamic_cast<Game3D &>(*sample.game);
    const std::string before = tanks_sample_snapshot(handle);
    fs::create_directories(options.output);
    std::ostringstream captures;
    captures.imbue(std::locale::classic());
    captures << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (bool pixel : {false, true})
    {
        if (pixel)
            captures << ',';
        const std::string suffix = pixel ? "pixel" : "clean";
        // Do not step simulation between modes. Draw twice to let the original
        // shadow/resolve pipeline settle; wall-clock presentation remains live.
        renderer.render(game, frame, pixel, sample.aiP2);
        renderer.render(game, frame, pixel, sample.aiP2);
        const auto camera = game.cameraForPlayer(0);
        const Rectangle crop = tankEnvelope(game.players()[0], camera);
        exportFrame(options.output, suffix, crop);
        captures << "{\"mode\":\"" << suffix << "\",\"camera_position\":["
                 << camera.position.x << ',' << camera.position.y << ',' << camera.position.z
                 << "],\"camera_target\":[" << camera.target.x << ',' << camera.target.y
                 << ',' << camera.target.z << "],\"camera_span\":" << camera.fovy
                 << ",\"tank_envelope_rect\":[" << crop.x << ',' << crop.y << ','
                 << crop.width << ',' << crop.height << "]}";
    }
    const std::string after = tanks_sample_snapshot(handle);
    if (before != after)
        throw std::runtime_error("Rendering mutated production state or snapshot");
    std::ofstream metadata(options.output / "capture.json");
    metadata << "{\"renderer\":\"production raylib\",\"window\":[1280,720],\"frame\":"
             << frame << ",\"seed\":" << options.seed << ",\"stage\":" << options.stage
             << ",\"idle\":" << (options.idle ? "true" : "false")
             << ",\"note\":\"Original renderGame, same simulation and camera rig. "
                "Wall-clock camera shake, shader time and presentation effects differ between engines. "
                "Crops show a fixed projected tank envelope at native scale, not occupied-pixel bounds.\","
                "\"captures\":[" << captures.str() << "],\"snapshot\":" << before << "}\n";
    if (!metadata)
        throw std::runtime_error("Unable to write reference capture metadata");
    std::cout << "REFERENCE_CAPTURE frame=" << frame
              << " digest=" << tanks3d::godot_sample::digest(game)
              << " output=" << options.output << '\n';
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        ReferenceRenderer renderer;
        renderer.open("resources");
        std::unique_ptr<void, decltype(&tanks_sample_destroy)> handle(
            tanks_sample_create("resources"), tanks_sample_destroy);
        if (!handle || tanks_sample_reset(handle.get(), options.seed, options.stage,
                                          options.solo ? 1 : 2, options.solo ? 0 : 1) != 0)
            throw std::runtime_error(tanks_sample_error(handle.get()));
        auto &sample = tanks3d::godot_sample::get(handle.get());
        sample.game = std::make_unique<Game3D>(std::move(*sample.game));
        auto &game = dynamic_cast<Game3D &>(*sample.game);
        constexpr unsigned tape[]{17U, 24U, 17U, 20U, 18U, 16U};
        for (int frame = 0; frame < options.frames; ++frame)
        {
            if (WindowShouldClose())
                throw std::runtime_error("Comparison window closed before requested frames finished");
            const unsigned input = options.idle ? 0U : tape[(frame / 180) % 6];
            if (tanks_sample_step(handle.get(), 1.0 / 60.0, input, 0) != 0)
                throw std::runtime_error(tanks_sample_error(handle.get()));
            renderer.render(game, frame + 1, false, !options.solo);
            if (frame + 1 == options.captureAt)
                capturePair(renderer, handle.get(), options, frame + 1);
        }
        std::cout << "REFERENCE_COMPLETE frames=" << options.frames
                  << " digest=" << tanks3d::godot_sample::digest(game) << '\n';
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Reference render failed: " << exception.what() << '\n';
        return 1;
    }
}

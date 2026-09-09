#ifndef TANKS3D_ENVIRONMENT_ASSETS_H
#define TANKS3D_ENVIRONMENT_ASSETS_H

#include <raylib.h>
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <raymath.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>

// Incremental environment art layer.  It owns only the authored ground
// texture; the modular skyline is assembled from reusable architectural
// pieces so it remains lightweight in the local tilted top-down combat view.
class EnvironmentAssets
{
public:
    EnvironmentAssets() = default;
    EnvironmentAssets(const EnvironmentAssets &) = delete;
    EnvironmentAssets &operator=(const EnvironmentAssets &) = delete;

    enum class UrbanBuildingKind : unsigned char
    {
        Office,
        Residence,
        Mall
    };

    struct UrbanBuildingProfile
    {
        UrbanBuildingKind kind = UrbanBuildingKind::Residence;
        unsigned char heightTier = 0U;
        unsigned char palette = 0U;
        unsigned char roofVariant = 0U;
        int floors = 2;
        std::uint32_t seed = 0U;
        float coreHeight = 0.88f;
        float totalHeight = 1.0f;
    };

    struct UrbanMass
    {
        Vector3 center{};
        Vector3 size{};
    };

    struct UrbanMassPlan
    {
        std::array<UrbanMass, 5> masses{};
        int count = 0;
        float topHeight = 0.0f;
    };

    // A forest cell always owns two mature trees. Exposed cells may add one
    // smaller edge sapling, producing a scalloped forest boundary without
    // consuming gameplay RNG or changing from frame to frame.
    struct ForestTree
    {
        Vector3 trunkBase{};
        Vector3 trunkTop{};
        float trunkBaseRadius = 0.055f;
        float trunkTopRadius = 0.032f;
        float crownBaseHeight = 0.58f;
        float crownTopHeight = 1.20f;
        float crownRadius = 0.30f;
        float crownScaleX = 1.0f;
        float crownScaleZ = 1.0f;
        float crownTwist = 0.0f;
        std::uint32_t seed = 0U;
        unsigned char palette = 0U;
        unsigned char branchCount = 1U;
        bool sapling = false;
    };

    struct ForestPlan
    {
        std::array<ForestTree, 3> trees{};
        int treeCount = 2;
        float topHeight = 0.0f;
        std::uint32_t seed = 0U;
        unsigned char edgeMask = 0U;
    };

    // Set bits denote an exposed forest edge rather than a neighboring tree.
    static constexpr unsigned char kForestEdgeNorth = 1U << 0U;
    static constexpr unsigned char kForestEdgeEast = 1U << 1U;
    static constexpr unsigned char kForestEdgeSouth = 1U << 2U;
    static constexpr unsigned char kForestEdgeWest = 1U << 3U;
    static constexpr unsigned char kForestAllEdges =
        kForestEdgeNorth | kForestEdgeEast |
        kForestEdgeSouth | kForestEdgeWest;
    static constexpr float kForestCanopyHeightCap = 1.44f;
    static constexpr float kForestCanopyRadiusCap = 0.38f;
    static constexpr float kForestTileOverhangCap = 0.10f;
    // A single crown now masks vehicle color and fine detail; overlapping
    // crowns read as real cover while their faceted gaps still leave the tank
    // silhouette faintly visible, matching the original foreground bushes.
    static constexpr unsigned char kForestCanopyAlphaMinimum = 104U;
    static constexpr unsigned char kForestCanopyAlphaMaximum = 132U;

    static constexpr float kUrbanTotalHeightCap = 1.38f;

    // Destructible architecture is deterministic per stage and 2x2 lot.
    // It never consumes gameplay RNG, and brick damage cannot reroll a model.
    static UrbanBuildingProfile urbanProfile(int stage, int row, int column)
    {
        const std::uint32_t lotRow = static_cast<std::uint32_t>(row / 2);
        const std::uint32_t lotColumn = static_cast<std::uint32_t>(column / 2);
        const std::uint32_t seed = mixUrbanSeed(
            static_cast<std::uint32_t>(stage) * 0x9e3779b9U ^
            (lotRow + 1U) * 0x85ebca6bU ^
            (lotColumn + 1U) * 0xc2b2ae35U);
        const unsigned int typeRoll = seed % 20U;
        const unsigned char tier = static_cast<unsigned char>((seed >> 8U) % 3U);

        UrbanBuildingProfile profile;
        profile.seed = seed;
        profile.heightTier = tier;
        profile.palette = static_cast<unsigned char>((seed >> 13U) % 3U);
        profile.roofVariant = static_cast<unsigned char>((seed >> 18U) % 3U);
        if (typeRoll < 9U)
        {
            static constexpr std::array<float, 3> heights{{0.88f, 0.97f, 1.06f}};
            profile.kind = UrbanBuildingKind::Residence;
            profile.coreHeight = heights[tier];
            profile.totalHeight = profile.coreHeight + 0.12f;
            profile.floors = tier == 2U ? 3 : 2;
        }
        else if (typeRoll < 15U)
        {
            static constexpr std::array<float, 3> heights{{0.74f, 0.81f, 0.88f}};
            profile.kind = UrbanBuildingKind::Mall;
            profile.coreHeight = heights[tier];
            profile.totalHeight = profile.coreHeight + 0.10f;
            profile.floors = 1;
        }
        else
        {
            static constexpr std::array<float, 3> heights{{1.03f, 1.13f, 1.23f}};
            profile.kind = UrbanBuildingKind::Office;
            profile.coreHeight = heights[tier];
            profile.totalHeight = profile.coreHeight + 0.15f;
            profile.floors = 3 + static_cast<int>(tier);
        }
        profile.totalHeight = std::min(profile.totalHeight,
                                       kUrbanTotalHeightCap);
        return profile;
    }

    // The original profile height is divided between walls and a readable
    // roof silhouette. Neighboring cells meet at their shared lot boundary;
    // only exposed faces receive the projecting architectural trim.
    static UrbanMassPlan urbanMassPlan(const UrbanBuildingProfile &profile,
                                       int row, int column,
                                       unsigned char brickMask)
    {
        UrbanMassPlan plan;
        if (brickMask == 0U)
            return plan;

        const float x = column + 0.5f;
        const float z = row + 0.5f;
        constexpr float baseY = 0.05f;
        if (brickMask == 0x0fU)
        {
            const float wallHeight = urbanWallHeight(profile);
            plan.masses[0] = {{x, baseY + wallHeight * 0.5f, z},
                              {1.0f, wallHeight, 1.0f}};
            const float bodyTop = baseY + wallHeight;
            constexpr float capHeight = 0.024f;
            constexpr float accessoryClearance = 0.008f;
            plan.masses[1] = {{x, bodyTop + capHeight * 0.5f, z},
                              {1.0f, capHeight, 1.0f}};
            plan.count = 2;

            const float roofBase = bodyTop + capHeight + accessoryClearance;
            const float roofHeight = std::max(
                0.012f, profile.totalHeight - roofBase);
            const float offsetX =
                (static_cast<float>((profile.seed >> 21U) & 3U) - 1.5f) * 0.065f;
            const float offsetZ =
                (static_cast<float>((profile.seed >> 23U) & 3U) - 1.5f) * 0.065f;
            Vector3 roofSize{1.0f, roofHeight, 1.0f};
            const bool pitchedRoof = profile.kind != UrbanBuildingKind::Mall;
            if (!pitchedRoof)
                roofSize = {0.38f, roofHeight, 0.34f};
            plan.masses[2] = {
                {x + (pitchedRoof ? 0.0f : offsetX),
                 roofBase + roofHeight * 0.5f,
                 z + (pitchedRoof ? 0.0f : offsetZ)},
                roofSize};
            plan.count = 3;
            plan.topHeight = std::min(profile.totalHeight,
                                      kUrbanTotalHeightCap);
            return plan;
        }

        for (int quadrant = 0; quadrant < 4; ++quadrant)
        {
            if ((brickMask & (1U << quadrant)) == 0U)
                continue;
            const float jitter =
                (static_cast<float>((profile.seed >> (quadrant * 2U)) & 3U) -
                 1.5f) * 0.025f;
            const float ruinHeight = std::clamp(
                profile.coreHeight * 0.66f + jitter, 0.48f, 0.82f);
            const float fragmentX =
                column + ((quadrant & 1) != 0 ? 0.75f : 0.25f);
            const float fragmentZ =
                row + ((quadrant & 2) != 0 ? 0.75f : 0.25f);
            plan.masses[static_cast<std::size_t>(plan.count++)] = {
                {fragmentX, baseY + ruinHeight * 0.5f, fragmentZ},
                {0.44f, ruinHeight, 0.44f}};
            plan.topHeight = std::max(plan.topHeight, baseY + ruinHeight);
        }
        return plan;
    }

    void drawUrbanBuildingCell(int stage, int row, int column,
                               unsigned char brickMask,
                               const std::array<bool, 4> &exposedFaces) const
    {
        const UrbanBuildingProfile profile = urbanProfile(stage, row, column);
        const UrbanMassPlan plan = urbanMassPlan(profile, row, column, brickMask);
        if (plan.count == 0)
            return;

        const float x = column + 0.5f;
        const float z = row + 0.5f;
        const Color accent = material(urbanAccentColor(profile), 7);
        const Color glass = material(urbanGlassColor(profile), 7);
        const Color dark = material(shade(urbanBodyColor(profile), 0.54f), 7);
        const Color light = material(Color{219, 194, 146, 255}, 7);

        DrawCube({x + 0.10f, 0.006f, z + 0.09f}, 0.98f, 0.012f, 0.98f,
                 Color{18, 24, 21, 82});
        if (brickMask != 0x0fU)
        {
            for (int index = 0; index < plan.count; ++index)
            {
                const UrbanMass &mass = plan.masses[static_cast<std::size_t>(index)];
                drawUrbanRuin(profile, mass, row, column, false);
            }
            return;
        }

        const UrbanMass &bodyMass = plan.masses[0];
        drawUrbanBody(profile, bodyMass, row, column, exposedFaces);
        DrawCube({x, 0.055f, z}, 1.0f, 0.10f, 1.0f, dark);
        const UrbanMass &roofCap = plan.masses[1];
        DrawCube(roofCap.center, roofCap.size.x, roofCap.size.y,
                 roofCap.size.z, light);

        for (int face = 0; face < 4; ++face)
        {
            if (!exposedFaces[static_cast<std::size_t>(face)])
                continue;
            if (profile.kind == UrbanBuildingKind::Office)
                drawOfficeFacade(profile, face, x, z, glass, accent);
            else if (profile.kind == UrbanBuildingKind::Residence)
                drawResidenceFacade(profile, face, x, z, glass, accent, dark);
            else
                drawMallFacade(profile, face, x, z, glass, accent, light);
            drawWeatheredFacade(profile, face, x, z, light);
        }

        drawUrbanRoof(profile, plan.masses[2], row, column, false);
    }

    static void drawUrbanBuildingShadow(int stage, int row, int column,
                                        unsigned char brickMask)
    {
        const UrbanBuildingProfile profile = urbanProfile(stage, row, column);
        const UrbanMassPlan plan = urbanMassPlan(profile, row, column, brickMask);
        // Roofs are substantial geometry now. Share their exact silhouette
        // with the lit pass, but omit the small seams and facade trim.
        const int shadowMassCount = brickMask == 0x0fU
                                        ? std::min(plan.count, 2)
                                        : plan.count;
        for (int index = 0; index < shadowMassCount; ++index)
        {
            const UrbanMass &mass = plan.masses[static_cast<std::size_t>(index)];
            if (brickMask == 0x0fU)
                DrawCube(mass.center, mass.size.x, mass.size.y, mass.size.z, WHITE);
            else
                drawUrbanRuin(profile, mass, row, column, true);
        }
        if (brickMask == 0x0fU)
            drawUrbanRoof(profile, plan.masses[2], row, column, true);
    }

    void load(const std::filesystem::path &resourceRoot)
    {
        forestCacheStorage().reset();
        std::filesystem::path textureRoot = resourceRoot / "textures";
        if (!std::filesystem::is_regular_file(textureRoot / "battlefield_grass.png"))
            textureRoot = resourceRoot.parent_path() / "3d" / "assets" / "textures";
        const std::filesystem::path path = textureRoot / "battlefield_grass.png";
        grass_ = LoadTexture(path.string().c_str());
        if (IsTextureValid(grass_))
        {
            GenTextureMipmaps(&grass_);
            SetTextureFilter(grass_, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(grass_, TEXTURE_WRAP_REPEAT);
        }
        else
        {
            TraceLog(LOG_WARNING, "TANKS3D: environment texture unavailable: %s",
                     path.string().c_str());
        }

        const std::filesystem::path masonryPath = textureRoot / "urban_masonry.png";
        masonry_ = LoadTexture(masonryPath.string().c_str());
        if (IsTextureValid(masonry_))
        {
            GenTextureMipmaps(&masonry_);
            SetTextureFilter(masonry_, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(masonry_, TEXTURE_WRAP_REPEAT);
        }
        else
        {
            TraceLog(LOG_WARNING, "TANKS3D: masonry texture unavailable: %s",
                     masonryPath.string().c_str());
        }
    }

    void unload()
    {
        forestCacheStorage().reset();
        if (IsTextureValid(grass_))
            UnloadTexture(grass_);
        if (IsTextureValid(masonry_))
            UnloadTexture(masonry_);
        grass_ = {};
        masonry_ = {};
    }

    void drawArenaGround() const
    {
        if (!IsTextureValid(grass_))
        {
            DrawPlane({13.0f, -0.055f, 13.0f}, {26.0f, 26.0f}, Color{59, 68, 59, 8});
            return;
        }

        // rlgl supplies explicit UV repetition and an upward normal, letting
        // the ground participate in the same directional/rim lighting shader
        // as the rest of the scene without baking light into the albedo map.
        rlSetTexture(grass_.id);
        rlBegin(RL_QUADS);
        rlColor4ub(190, 201, 180, 8);
        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(0.0f, 0.0f);   rlVertex3f(0.0f, -0.052f, 0.0f);
        rlTexCoord2f(0.0f, 13.0f);  rlVertex3f(0.0f, -0.052f, 26.0f);
        rlTexCoord2f(13.0f, 13.0f); rlVertex3f(26.0f, -0.052f, 26.0f);
        rlTexCoord2f(13.0f, 0.0f);  rlVertex3f(26.0f, -0.052f, 0.0f);
        rlEnd();
        rlSetTexture(0);
    }

    void drawArenaApron() const
    {
        constexpr float minimum = -51.0f;
        constexpr float maximum = 77.0f;
        constexpr float height = -0.105f;
        if (!IsTextureValid(grass_))
        {
            DrawPlane({13.0f, height, 13.0f}, {128.0f, 128.0f},
                      Color{45, 55, 46, 8});
            return;
        }

        // Continue the arena's two-world-unit texture cadence beyond its
        // collision boundary. The darker tint keeps the 0..26 playfield
        // readable while a player-centered camera remains visually grounded
        // near map edges.
        rlSetTexture(grass_.id);
        rlBegin(RL_QUADS);
        rlColor4ub(118, 132, 112, 8);
        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(minimum * 0.5f, minimum * 0.5f);
        rlVertex3f(minimum, height, minimum);
        rlTexCoord2f(minimum * 0.5f, maximum * 0.5f);
        rlVertex3f(minimum, height, maximum);
        rlTexCoord2f(maximum * 0.5f, maximum * 0.5f);
        rlVertex3f(maximum, height, maximum);
        rlTexCoord2f(maximum * 0.5f, minimum * 0.5f);
        rlVertex3f(maximum, height, minimum);
        rlEnd();
        rlSetTexture(0);
    }

    void drawBackdropCity() const
    {
        // Buildings sit beyond the playfield, so they establish scale and a
        // war-era urban setting without altering the original collision map.
        static constexpr std::array<Building, 9> buildings{{
            {-5.0f, -4.8f, 5.2f, 4.1f, 5.8f, Color{149, 145, 112, 255}, Color{111, 79, 53, 255}, 1},
            { 0.3f, -5.4f, 4.1f, 3.3f, 4.2f, Color{176, 151, 110, 255}, Color{66, 106, 91, 255}, 0},
            { 5.0f, -5.6f, 4.7f, 3.8f, 6.8f, Color{139, 154, 136, 255}, Color{139, 89, 53, 255}, 2},
            {10.2f, -6.1f, 4.8f, 4.4f, 5.1f, Color{178, 152, 116, 255}, Color{66, 108, 93, 255}, 1},
            {15.6f, -5.8f, 5.0f, 3.9f, 7.4f, Color{132, 149, 134, 255}, Color{131, 86, 57, 255}, 2},
            {21.2f, -5.7f, 4.6f, 3.5f, 5.7f, Color{173, 143, 108, 255}, Color{66, 97, 83, 255}, 0},
            {26.4f, -5.3f, 4.9f, 4.1f, 6.5f, Color{149, 153, 126, 255}, Color{134, 84, 54, 255}, 1},
            {31.7f, -5.0f, 5.5f, 4.0f, 4.8f, Color{180, 153, 115, 255}, Color{65, 103, 87, 255}, 0},
            {37.3f, -5.2f, 5.2f, 4.2f, 7.0f, Color{139, 151, 130, 255}, Color{137, 89, 58, 255}, 2}}};

        for (const Building &building : buildings)
            drawBuilding(building);
    }

    void drawMasonryCore(float x, float z, float width = 0.88f,
                         float height = 0.72f, float depth = 0.88f) const
    {
        if (!IsTextureValid(masonry_))
        {
            DrawCube({x, height * 0.5f + 0.05f, z}, width, height, depth,
                     Color{161, 67, 47, 6});
            return;
        }

        const float x0 = x - width * 0.5f;
        const float x1 = x + width * 0.5f;
        const float z0 = z - depth * 0.5f;
        const float z1 = z + depth * 0.5f;
        const float y0 = 0.05f;
        const float y1 = y0 + height;
        const auto vertex = [](float u, float v, float px, float py, float pz) {
            rlTexCoord2f(u, v);
            rlVertex3f(px, py, pz);
        };

        rlSetTexture(masonry_.id);
        rlBegin(RL_QUADS);
        rlColor4ub(232, 220, 205, 6);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        vertex(0, 1, x0, y0, z1); vertex(1, 1, x1, y0, z1);
        vertex(1, 0, x1, y1, z1); vertex(0, 0, x0, y1, z1);

        rlNormal3f(0.0f, 0.0f, -1.0f);
        vertex(0, 1, x1, y0, z0); vertex(1, 1, x0, y0, z0);
        vertex(1, 0, x0, y1, z0); vertex(0, 0, x1, y1, z0);

        rlNormal3f(1.0f, 0.0f, 0.0f);
        vertex(0, 1, x1, y0, z1); vertex(1, 1, x1, y0, z0);
        vertex(1, 0, x1, y1, z0); vertex(0, 0, x1, y1, z1);

        rlNormal3f(-1.0f, 0.0f, 0.0f);
        vertex(0, 1, x0, y0, z0); vertex(1, 1, x0, y0, z1);
        vertex(1, 0, x0, y1, z1); vertex(0, 0, x0, y1, z0);

        rlEnd();
        rlSetTexture(0);
    }

    // Pure forest layout shared by the lit structure, translucent canopy, and
    // shadow pass. Core trees never depend on edgeMask, so destroying a nearby
    // forest tile cannot reroll a surviving tree. The edge mask only reveals a
    // precomputed sapling along one exposed side.
    static ForestPlan forestPlan(int stage, int row, int column,
                                 unsigned char requestedEdgeMask)
    {
        ForestPlan plan;
        plan.edgeMask = requestedEdgeMask & kForestAllEdges;
        plan.seed = mixUrbanSeed(
            static_cast<std::uint32_t>(stage) * 0x9e3779b9U ^
            (static_cast<std::uint32_t>(row) + 1U) * 0x85ebca6bU ^
            (static_cast<std::uint32_t>(column) + 1U) * 0xc2b2ae35U ^
            0x6d2b79f5U);

        static constexpr std::array<std::array<float, 2>, 2> anchors{{
            {{-0.15f, 0.14f}}, {{0.16f, -0.16f}}}};
        const int rotation = static_cast<int>((plan.seed >> 28U) & 3U);
        for (int index = 0; index < 2; ++index)
        {
            ForestTree &tree = plan.trees[static_cast<std::size_t>(index)];
            tree.seed = mixUrbanSeed(
                plan.seed ^ (0x27d4eb2dU * static_cast<std::uint32_t>(index + 1)));
            float localX = anchors[static_cast<std::size_t>(index)][0];
            float localZ = anchors[static_cast<std::size_t>(index)][1];
            for (int turn = 0; turn < rotation; ++turn)
            {
                const float previousX = localX;
                localX = -localZ;
                localZ = previousX;
            }
            localX += forestSigned(tree.seed ^ 0x7f4a7c15U) * 0.035f;
            localZ += forestSigned(tree.seed ^ 0x94d049bbU) * 0.035f;

            const float trunkHeight = index == 0
                                          ? 0.80f + forestSigned(tree.seed ^ 0x165667b1U) * 0.070f
                                          : 0.69f + forestSigned(tree.seed ^ 0xd3a2646cU) * 0.060f;
            const float leanX = forestSigned(tree.seed ^ 0xfd7046c5U) * 0.018f;
            const float leanZ = forestSigned(tree.seed ^ 0xb55a4f09U) * 0.018f;
            tree.trunkBase = {column + 0.5f + localX, 0.02f,
                              row + 0.5f + localZ};
            tree.trunkTop = {tree.trunkBase.x + leanX, trunkHeight,
                             tree.trunkBase.z + leanZ};
            tree.trunkBaseRadius = index == 0
                                       ? 0.065f + forestSigned(tree.seed ^ 0x1b873593U) * 0.009f
                                       : 0.049f + forestSigned(tree.seed ^ 0x85ebca77U) * 0.007f;
            tree.trunkTopRadius = tree.trunkBaseRadius *
                                  (0.55f + forestUnit(tree.seed ^ 0xc2b2ae3dU) * 0.08f);
            tree.crownBaseHeight = index == 0
                                       ? 0.46f + forestSigned(tree.seed ^ 0x9e3779b1U) * 0.035f
                                       : 0.44f + forestSigned(tree.seed ^ 0x4cf5ad43U) * 0.030f;
            tree.crownTopHeight = index == 0
                                      ? 1.34f + forestSigned(tree.seed ^ 0x632be5abU) * 0.080f
                                      : 1.20f + forestSigned(tree.seed ^ 0x85157af5U) * 0.065f;
            tree.crownTopHeight = std::min(tree.crownTopHeight,
                                           kForestCanopyHeightCap);
            tree.crownRadius = index == 0
                                   ? 0.350f + forestSigned(tree.seed ^ 0x58f38dedU) * 0.014f
                                   : 0.315f + forestSigned(tree.seed ^ 0xa24baed5U) * 0.012f;
            tree.crownRadius = std::min(tree.crownRadius,
                                        kForestCanopyRadiusCap);
            tree.crownScaleX = 0.96f + forestUnit(tree.seed ^ 0x9fb21c65U) * 0.08f;
            tree.crownScaleZ = 0.96f + forestUnit(tree.seed ^ 0x3c6ef372U) * 0.08f;
            tree.crownTwist = forestUnit(tree.seed ^ 0xbb67ae85U) *
                              (2.0f * kForestPi);
            tree.palette = static_cast<unsigned char>((tree.seed >> 19U) % 3U);
            tree.branchCount = static_cast<unsigned char>(index == 0 ? 2U : 1U);
            tree.sapling = false;
            plan.topHeight = std::max(plan.topHeight, tree.crownTopHeight);
        }

        std::array<unsigned char, 4> exposedEdges{};
        int exposedCount = 0;
        for (unsigned char edge : {kForestEdgeNorth, kForestEdgeEast,
                                   kForestEdgeSouth, kForestEdgeWest})
        {
            if ((plan.edgeMask & edge) != 0U)
                exposedEdges[static_cast<std::size_t>(exposedCount++)] = edge;
        }
        if (exposedCount > 0 && ((plan.seed >> 17U) & 3U) != 0U)
        {
            ForestTree &sapling = plan.trees[2];
            sapling.seed = mixUrbanSeed(plan.seed ^ 0x243f6a88U);
            const unsigned char edge = exposedEdges[static_cast<std::size_t>(
                (sapling.seed >> 8U) % static_cast<std::uint32_t>(exposedCount))];
            const float along = forestSigned(sapling.seed ^ 0x13198a2eU) * 0.13f;
            float localX = along;
            float localZ = -0.33f;
            if (edge == kForestEdgeEast)
            {
                localX = 0.33f;
                localZ = along;
            }
            else if (edge == kForestEdgeSouth)
            {
                localX = along;
                localZ = 0.33f;
            }
            else if (edge == kForestEdgeWest)
            {
                localX = -0.33f;
                localZ = along;
            }

            const float trunkHeight =
                0.53f + forestSigned(sapling.seed ^ 0xa4093822U) * 0.050f;
            sapling.trunkBase = {column + 0.5f + localX, 0.02f,
                                 row + 0.5f + localZ};
            sapling.trunkTop = {
                sapling.trunkBase.x + forestSigned(sapling.seed ^ 0x299f31d0U) * 0.012f,
                trunkHeight,
                sapling.trunkBase.z + forestSigned(sapling.seed ^ 0x082efa98U) * 0.012f};
            sapling.trunkBaseRadius =
                0.039f + forestSigned(sapling.seed ^ 0xec4e6c89U) * 0.005f;
            sapling.trunkTopRadius = sapling.trunkBaseRadius * 0.56f;
            sapling.crownBaseHeight =
                0.40f + forestSigned(sapling.seed ^ 0x452821e6U) * 0.020f;
            sapling.crownTopHeight =
                0.91f + forestSigned(sapling.seed ^ 0x38d01377U) * 0.070f;
            sapling.crownRadius =
                0.190f + forestSigned(sapling.seed ^ 0xbe5466cfU) * 0.009f;
            sapling.crownScaleX =
                0.97f + forestUnit(sapling.seed ^ 0x34e90c6cU) * 0.06f;
            sapling.crownScaleZ =
                0.97f + forestUnit(sapling.seed ^ 0xc0ac29b7U) * 0.06f;
            sapling.crownTwist = forestUnit(sapling.seed ^ 0xc97c50ddU) *
                                 (2.0f * kForestPi);
            sapling.palette = static_cast<unsigned char>((sapling.seed >> 20U) % 3U);
            sapling.branchCount = 0U;
            sapling.sapling = true;
            plan.treeCount = 3;
            plan.topHeight = std::max(plan.topHeight, sapling.crownTopHeight);
        }
        return plan;
    }

    // Call inside the lit world pass. Material tag 8 keeps the narrow trunks
    // opaque and rough while the later canopy pass preserves classic cover.
    void drawForestStructure(int stage, int row, int column,
                             unsigned char edgeMask) const
    {
        const ForestCellCache *cached = forestCellCache(stage, row, column, edgeMask);
        const ForestPlan plan = cached ? cached->plan
                                      : forestPlan(stage, row, column, edgeMask);
        for (int index = 0; index < plan.treeCount; ++index)
            drawForestTrunk(plan.trees[static_cast<std::size_t>(index)]);
    }

    // Call after vehicles in BLEND_ALPHA mode. Each tree is one connected,
    // faceted crown rather than a stack of intersecting transparent spheres.
    void drawForestCanopy(int stage, int row, int column,
                          unsigned char edgeMask) const
    {
        ForestCellCache *cached = forestCellCache(stage, row, column, edgeMask);
        const ForestPlan plan = cached ? cached->plan
                                      : forestPlan(stage, row, column, edgeMask);
        for (int index = 0; index < plan.treeCount; ++index)
        {
            const ForestTree &tree = plan.trees[static_cast<std::size_t>(index)];
            drawTieredForestCrown(tree, forestPalette(tree.palette), 1.0f,
                cached ? &cached->lit[static_cast<std::size_t>(index)] : nullptr);
        }
    }

    // Only the two mature canopy cores cast into the shadow map. Their smaller
    // footprint yields readable dappled shade instead of an opaque dark tile.
    static void drawForestShadow(int stage, int row, int column,
                                 unsigned char edgeMask)
    {
        ForestCellCache *cached = forestCellCache(stage, row, column, edgeMask);
        const ForestPlan plan = cached ? cached->plan
                                      : forestPlan(stage, row, column, edgeMask);
        static constexpr std::array<Color, 4> shadowColors{{
            WHITE, WHITE, WHITE, WHITE}};
        for (int index = 0; index < 2; ++index)
        {
            drawTieredForestCrown(
                plan.trees[static_cast<std::size_t>(index)], shadowColors, 0.62f,
                cached ? &cached->shadow[static_cast<std::size_t>(index)] : nullptr);
        }
    }

    // Transitional wrapper for callers that have not yet supplied stage and
    // edge context. It still uses the new connected canopy geometry.
    void drawForestTile(int row, int column) const
    {
        drawForestCanopy(1, row, column, 0U);
    }

private:
    static constexpr int kForestCrownSegments = 16;
    static constexpr int kForestCrownRings = 7;
    static constexpr int kForestCrownTriangles =
        (kForestCrownRings - 1) * kForestCrownSegments * 2 + kForestCrownSegments;

    struct ForestCrownGeometry
    {
        std::array<std::array<Vector3, kForestCrownSegments>, kForestCrownRings> rings{};
        Vector3 apex{};
        std::array<Vector3, kForestCrownTriangles> normals{};
        std::array<Color, kForestCrownTriangles> colors{};
    };

    struct ForestCellCache
    {
        ForestPlan plan{};
        std::array<std::unique_ptr<ForestCrownGeometry>, 3> lit{};
        std::array<std::unique_ptr<ForestCrownGeometry>, 2> shadow{};
    };

    struct ForestRenderCache
    {
        int stage = 0;
        std::array<std::unique_ptr<ForestCellCache>, 26 * 26> cells{};
    };

    // One shared CPU cache also serves the static shadow entry point. Cells
    // and the two radius variants allocate only when drawn; changing stages
    // releases the previous arena. The 676-cell limit bounds retained memory.
    static std::unique_ptr<ForestRenderCache> &forestCacheStorage()
    {
        static std::unique_ptr<ForestRenderCache> cache;
        return cache;
    }

    static ForestCellCache *forestCellCache(int stage, int row, int column,
                                            unsigned char requestedEdgeMask)
    {
        if (row < 0 || row >= 26 || column < 0 || column >= 26)
            return nullptr;
        auto &cache = forestCacheStorage();
        if (!cache || cache->stage != stage)
        {
            cache = std::make_unique<ForestRenderCache>();
            cache->stage = stage;
        }
        auto &cell = cache->cells[static_cast<std::size_t>(row * 26 + column)];
        const unsigned char edgeMask = requestedEdgeMask & kForestAllEdges;
        if (!cell || cell->plan.edgeMask != edgeMask)
        {
            cell = std::make_unique<ForestCellCache>();
            cell->plan = forestPlan(stage, row, column, edgeMask);
        }
        return cell.get();
    }

    static constexpr float kForestPi = 3.14159265358979323846f;

    static float urbanWallHeight(const UrbanBuildingProfile &profile)
    {
        const float roofAllowance = profile.kind == UrbanBuildingKind::Mall
                                        ? 0.11f : 0.18f;
        return profile.coreHeight - roofAllowance;
    }

    static float forestUnit(std::uint32_t value)
    {
        const std::uint32_t mixed = mixUrbanSeed(value);
        return static_cast<float>(mixed & 0xffffU) / 65535.0f;
    }

    static float forestSigned(std::uint32_t value)
    {
        return forestUnit(value) * 2.0f - 1.0f;
    }

    static std::array<Color, 4> forestPalette(unsigned char requestedPalette)
    {
        static constexpr std::array<std::array<Color, 4>, 3> palettes{{
            {{Color{24, 61, 47, 132}, Color{43, 99, 57, 126},
              Color{88, 142, 70, 116}, Color{153, 179, 92, 104}}},
            {{Color{22, 65, 57, 132}, Color{38, 104, 72, 126},
              Color{77, 151, 92, 116}, Color{135, 184, 113, 104}}},
            {{Color{39, 65, 36, 132}, Color{74, 108, 43, 126},
              Color{118, 148, 61, 116}, Color{177, 186, 95, 104}}}}};
        return palettes[requestedPalette % palettes.size()];
    }

    static Vector3 forestLerp(Vector3 first, Vector3 second, float amount)
    {
        return {first.x + (second.x - first.x) * amount,
                first.y + (second.y - first.y) * amount,
                first.z + (second.z - first.z) * amount};
    }

    static void drawForestCylinder(Vector3 start, Vector3 end,
                                    float startRadius, float endRadius,
                                    int sides, Color color)
    {
        const Vector3 axis = Vector3Subtract(end, start);
        if (axis.x == 0.0f && axis.y == 0.0f && axis.z == 0.0f)
            return;

        // Keep the existing raylib cylinder's ring orientation and vertices,
        // but emit an explicit outward normal for every facet and cap. The
        // stock DrawCylinderEx inherits whichever normal was emitted last.
        const Vector3 tangent = Vector3Normalize(Vector3Perpendicular(axis));
        const Vector3 bitangent = Vector3Normalize(Vector3CrossProduct(tangent, axis));
        const float step = (2.0f * kForestPi) / static_cast<float>(sides);
        const auto point = [&](Vector3 center, float radius, int index)
        {
            const float sine = std::sin(step * index) * radius;
            const float cosine = std::cos(step * index) * radius;
            return Vector3{center.x + sine * tangent.x + cosine * bitangent.x,
                           center.y + sine * tangent.y + cosine * bitangent.y,
                           center.z + sine * tangent.z + cosine * bitangent.z};
        };

        rlBegin(RL_TRIANGLES);
        for (int side = 0; side < sides; ++side)
        {
            const Vector3 lower = point(start, startRadius, side);
            const Vector3 lowerNext = point(start, startRadius, side + 1);
            const Vector3 upper = point(end, endRadius, side);
            const Vector3 upperNext = point(end, endRadius, side + 1);
            emitSurfaceTriangle(lower, lowerNext, upper, color);
            emitSurfaceTriangle(lowerNext, upperNext, upper, color);
            if (startRadius > 0.0f)
                emitSurfaceTriangle(start, lowerNext, lower, color);
            if (endRadius > 0.0f)
                emitSurfaceTriangle(end, upper, upperNext, color);
        }
        rlEnd();
    }

    static void drawForestTrunk(const ForestTree &tree)
    {
        const Color bark = material(
            tree.sapling ? Color{83, 65, 42, 255}
                         : Color{74, 56, 37, 255},
            8);
        const Color branchColor = material(
            tree.sapling ? Color{105, 79, 46, 255}
                         : Color{101, 74, 43, 255},
            8);
        drawForestCylinder(tree.trunkBase, tree.trunkTop,
                           tree.trunkBaseRadius, tree.trunkTopRadius,
                           tree.sapling ? 7 : 8, bark);

        if (!tree.sapling)
        {
            // Broad buttress roots ground the arcade jungle silhouettes.
            for (int root = 0; root < 3; ++root)
            {
                const float angle = tree.crownTwist + root * 2.094395f;
                const Vector3 toe{
                    tree.trunkBase.x + std::cos(angle) * 0.125f, 0.018f,
                    tree.trunkBase.z + std::sin(angle) * 0.125f};
                drawForestCylinder(toe, forestLerp(tree.trunkBase, tree.trunkTop, 0.18f),
                                   0.020f, tree.trunkBaseRadius * 0.42f, 5, bark);
            }
        }

        for (unsigned char branch = 0U; branch < tree.branchCount; ++branch)
        {
            const float branchHeight = 0.57f + static_cast<float>(branch) * 0.10f;
            const Vector3 start = forestLerp(tree.trunkBase, tree.trunkTop,
                                             branchHeight);
            const float angle = forestUnit(
                                    tree.seed ^
                                    (0x9e3779b9U * static_cast<std::uint32_t>(branch + 1U))) *
                                (2.0f * kForestPi);
            const float length = 0.18f + forestUnit(
                tree.seed ^ (0x85ebca6bU + static_cast<std::uint32_t>(branch))) * 0.07f;
            const Vector3 end{
                start.x + std::cos(angle) * length,
                start.y + 0.15f + forestUnit(
                    tree.seed ^ (0xc2b2ae35U + static_cast<std::uint32_t>(branch))) * 0.08f,
                start.z + std::sin(angle) * length};
            drawForestCylinder(start, end, tree.trunkTopRadius * 0.72f,
                               tree.trunkTopRadius * 0.24f, 6, branchColor);
        }
    }

    static void emitSurfaceTriangle(Vector3 first, Vector3 second, Vector3 third,
                                   Color color)
    {
        const Vector3 firstEdge{second.x - first.x, second.y - first.y,
                                second.z - first.z};
        const Vector3 secondEdge{third.x - first.x, third.y - first.y,
                                 third.z - first.z};
        Vector3 normal{
            firstEdge.y * secondEdge.z - firstEdge.z * secondEdge.y,
            firstEdge.z * secondEdge.x - firstEdge.x * secondEdge.z,
            firstEdge.x * secondEdge.y - firstEdge.y * secondEdge.x};
        const float normalLength = std::sqrt(normal.x * normal.x +
                                             normal.y * normal.y +
                                             normal.z * normal.z);
        if (normalLength > 0.000001f)
        {
            normal.x /= normalLength;
            normal.y /= normalLength;
            normal.z /= normalLength;
        }
        else
        {
            normal = {0.0f, 1.0f, 0.0f};
        }
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(normal.x, normal.y, normal.z);
        rlVertex3f(first.x, first.y, first.z);
        rlVertex3f(second.x, second.y, second.z);
        rlVertex3f(third.x, third.y, third.z);
    }

    static std::unique_ptr<ForestCrownGeometry> makeForestCrownGeometry(
        const ForestTree &tree, const std::array<Color, 4> &colors,
        float radiusScale)
    {
        static constexpr int segmentCount = kForestCrownSegments;
        static constexpr int ringCount = kForestCrownRings;
        static constexpr std::array<float, ringCount> ringHeight{{
            0.0f, 0.13f, 0.35f, 0.57f, 0.77f, 0.92f, 0.98f}};
        static constexpr std::array<float, ringCount> ringRadius{{
            0.30f, 0.69f, 0.93f, 1.00f, 0.89f, 0.59f, 0.27f}};
        auto geometry = std::make_unique<ForestCrownGeometry>();
        auto &rings = geometry->rings;
        const float crownHeight = tree.crownTopHeight - tree.crownBaseHeight;
        const float lobePhase = forestUnit(tree.seed ^ 0x632be5abU) *
                                (2.0f * kForestPi);

        for (int ring = 0; ring < ringCount; ++ring)
        {
            const float heightFraction = ringHeight[static_cast<std::size_t>(ring)];
            const float y = tree.crownBaseHeight + crownHeight * heightFraction;
            const float centerX = tree.trunkTop.x +
                forestSigned(tree.seed ^ 0x7f4a7c15U) * heightFraction * 0.008f;
            const float centerZ = tree.trunkTop.z +
                forestSigned(tree.seed ^ 0x94d049bbU) * heightFraction * 0.008f;
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                // Broad, low-frequency bulges form rounded leaf groups in
                // one transparent skin. Closely spaced shoulder rings close
                // the dome smoothly; the top ring varies by only 1.6% of H.
                const float angle = tree.crownTwist +
                                    static_cast<float>(segment) *
                                        (2.0f * kForestPi /
                                         static_cast<float>(segmentCount));
                const float irregularity =
                    0.91f + 0.06f * std::cos(3.0f * angle + lobePhase +
                                             heightFraction * 0.16f) +
                    0.024f * std::cos(2.0f * angle - lobePhase);
                const float radius = tree.crownRadius *
                                     ringRadius[static_cast<std::size_t>(ring)] *
                                     radiusScale * irregularity;
                const float heightVariation = ring == ringCount - 1
                    ? crownHeight * 0.008f * std::cos(2.0f * angle + lobePhase)
                    : ring > 0
                        ? crownHeight * 0.017f * std::cos(2.0f * angle + lobePhase)
                        : 0.0f;
                rings[static_cast<std::size_t>(ring)]
                     [static_cast<std::size_t>(segment)] = {
                    centerX + std::cos(angle) * radius * tree.crownScaleX,
                    y + heightVariation,
                    centerZ + std::sin(angle) * radius * tree.crownScaleZ};
            }
        }

        const Vector3 apex{
            tree.trunkTop.x + forestSigned(tree.seed ^ 0x7f4a7c15U) * 0.008f,
            tree.crownTopHeight,
            tree.trunkTop.z + forestSigned(tree.seed ^ 0x94d049bbU) * 0.008f};
        geometry->apex = apex;
        std::size_t triangleIndex = 0;
        const auto remember = [&](Vector3 first, Vector3 second, Vector3 third,
                                  Color color) {
            const Vector3 firstEdge{second.x - first.x, second.y - first.y,
                                    second.z - first.z};
            const Vector3 secondEdge{third.x - first.x, third.y - first.y,
                                     third.z - first.z};
            Vector3 normal{
                firstEdge.y * secondEdge.z - firstEdge.z * secondEdge.y,
                firstEdge.z * secondEdge.x - firstEdge.x * secondEdge.z,
                firstEdge.x * secondEdge.y - firstEdge.y * secondEdge.x};
            const float normalLength = std::sqrt(normal.x * normal.x +
                                                 normal.y * normal.y +
                                                 normal.z * normal.z);
            if (normalLength > 0.000001f)
            {
                normal.x /= normalLength;
                normal.y /= normalLength;
                normal.z /= normalLength;
            }
            else
            {
                normal = {0.0f, 1.0f, 0.0f};
            }
            geometry->normals[triangleIndex] = normal;
            geometry->colors[triangleIndex++] = color;
        };
        for (int ring = 0; ring < ringCount - 1; ++ring)
        {
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const int next = (segment + 1) % segmentCount;
                const Vector3 lower =
                    rings[static_cast<std::size_t>(ring)]
                         [static_cast<std::size_t>(segment)];
                const Vector3 lowerNext =
                    rings[static_cast<std::size_t>(ring)]
                         [static_cast<std::size_t>(next)];
                const Vector3 upper =
                    rings[static_cast<std::size_t>(ring + 1)]
                         [static_cast<std::size_t>(segment)];
                const Vector3 upperNext =
                    rings[static_cast<std::size_t>(ring + 1)]
                         [static_cast<std::size_t>(next)];
                static constexpr std::array<std::size_t, ringCount - 1> tones{{
                    0U, 1U, 1U, 2U, 2U, 3U}};
                const Color color = colors[tones[static_cast<std::size_t>(ring)]];
                const float facetAngle = static_cast<float>(segment) *
                    (2.0f * kForestPi / static_cast<float>(segmentCount));
                const float facetLight = 1.0f +
                    0.05f * std::cos(3.0f * facetAngle + lobePhase);
                remember(lower, upperNext, lowerNext, color);
                remember(lower, upper, upperNext, shade(color, facetLight));
            }
        }
        for (int segment = 0; segment < segmentCount; ++segment)
        {
            const int next = (segment + 1) % segmentCount;
            const Vector3 lower =
                rings[ringCount - 1][static_cast<std::size_t>(segment)];
            const Vector3 lowerNext =
                rings[ringCount - 1][static_cast<std::size_t>(next)];
            remember(lower, apex, lowerNext, colors[3]);
        }
        return geometry;
    }

    static void drawTieredForestCrown(
        const ForestTree &tree, const std::array<Color, 4> &colors,
        float radiusScale, std::unique_ptr<ForestCrownGeometry> *cached)
    {
        std::unique_ptr<ForestCrownGeometry> transient;
        if (cached == nullptr)
            cached = &transient;
        if (!*cached)
            *cached = makeForestCrownGeometry(tree, colors, radiusScale);
        const ForestCrownGeometry &geometry = **cached;
        std::size_t triangleIndex = 0;
        const auto emit = [&](Vector3 first, Vector3 second, Vector3 third) {
            const Color color = geometry.colors[triangleIndex];
            const Vector3 normal = geometry.normals[triangleIndex++];
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlNormal3f(normal.x, normal.y, normal.z);
            rlVertex3f(first.x, first.y, first.z);
            rlVertex3f(second.x, second.y, second.z);
            rlVertex3f(third.x, third.y, third.z);
        };
        // Replay the original world-space vertex and normal stream. No matrix,
        // material, primitive or alpha-order change accompanies the cache.
        rlBegin(RL_TRIANGLES);
        for (int ring = 0; ring < kForestCrownRings - 1; ++ring)
        {
            for (int segment = 0; segment < kForestCrownSegments; ++segment)
            {
                const int next = (segment + 1) % kForestCrownSegments;
                const Vector3 lower = geometry.rings[static_cast<std::size_t>(ring)]
                                                   [static_cast<std::size_t>(segment)];
                const Vector3 lowerNext = geometry.rings[static_cast<std::size_t>(ring)]
                                                       [static_cast<std::size_t>(next)];
                const Vector3 upper = geometry.rings[static_cast<std::size_t>(ring + 1)]
                                                   [static_cast<std::size_t>(segment)];
                const Vector3 upperNext = geometry.rings[static_cast<std::size_t>(ring + 1)]
                                                       [static_cast<std::size_t>(next)];
                emit(lower, upperNext, lowerNext);
                emit(lower, upper, upperNext);
            }
        }
        for (int segment = 0; segment < kForestCrownSegments; ++segment)
        {
            const int next = (segment + 1) % kForestCrownSegments;
            emit(geometry.rings[kForestCrownRings - 1][static_cast<std::size_t>(segment)],
                 geometry.apex,
                 geometry.rings[kForestCrownRings - 1][static_cast<std::size_t>(next)]);
        }
        rlEnd();
    }

    static std::uint32_t mixUrbanSeed(std::uint32_t value)
    {
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        value ^= value >> 16U;
        return value;
    }

    static Color urbanBodyColor(const UrbanBuildingProfile &profile)
    {
        static constexpr std::array<Color, 3> office{{
            Color{148, 149, 121, 255}, Color{179, 151, 110, 255},
            Color{139, 159, 145, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{208, 177, 125, 255}, Color{191, 140, 108, 255},
            Color{218, 197, 152, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{197, 175, 134, 255}, Color{189, 145, 109, 255},
            Color{167, 181, 156, 255}}};
        const std::size_t palette = profile.palette % 3U;
        if (profile.kind == UrbanBuildingKind::Office)
            return office[palette];
        if (profile.kind == UrbanBuildingKind::Mall)
            return mall[palette];
        return residence[palette];
    }

    static Color urbanAccentColor(const UrbanBuildingProfile &profile)
    {
        static constexpr std::array<Color, 3> office{{
            Color{65, 104, 95, 255}, Color{109, 71, 55, 255},
            Color{66, 91, 84, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{65, 115, 104, 255}, Color{89, 111, 87, 255},
            Color{129, 70, 56, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{143, 65, 54, 255}, Color{63, 117, 107, 255},
            Color{153, 107, 54, 255}}};
        const std::size_t palette = profile.palette % 3U;
        if (profile.kind == UrbanBuildingKind::Office)
            return office[palette];
        if (profile.kind == UrbanBuildingKind::Mall)
            return mall[palette];
        return residence[palette];
    }

    static Color urbanGlassColor(const UrbanBuildingProfile &profile)
    {
        static constexpr std::array<Color, 3> office{{
            Color{38, 66, 66, 255}, Color{50, 66, 63, 255},
            Color{45, 76, 73, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{42, 58, 55, 255}, Color{48, 62, 58, 255},
            Color{41, 62, 60, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{37, 64, 63, 255}, Color{37, 72, 65, 255},
            Color{43, 61, 59, 255}}};
        const std::size_t palette = profile.palette % 3U;
        if (profile.kind == UrbanBuildingKind::Office)
            return office[palette];
        if (profile.kind == UrbanBuildingKind::Mall)
            return mall[palette];
        return residence[palette];
    }

    static void drawFacadeElement(int face, float x, float z, float y,
                                  float lateral, float width, float height,
                                  float thickness, Color color)
    {
        constexpr float halfBody = 0.50f;
        const float outward = halfBody + thickness * 0.5f - 0.008f;
        if (face == 0)
            DrawCube({x + lateral, y, z - outward}, width, height, thickness,
                     color);
        else if (face == 1)
            DrawCube({x + lateral, y, z + outward}, width, height, thickness,
                     color);
        else if (face == 2)
            DrawCube({x - outward, y, z + lateral}, thickness, height, width,
                     color);
        else
            DrawCube({x + outward, y, z + lateral}, thickness, height, width,
                     color);
    }

    static Vector3 facadePoint(int face, float x, float z, float lateral,
                                float y, float outward)
    {
        if (face == 0)
            return {x + lateral, y, z - outward};
        if (face == 1)
            return {x + lateral, y, z + outward};
        if (face == 2)
            return {x - outward, y, z + lateral};
        return {x + outward, y, z + lateral};
    }

    static void emitSurfaceQuad(Vector3 first, Vector3 second, Vector3 third,
                                Vector3 fourth, Color color)
    {
        emitSurfaceTriangle(first, second, third, color);
        emitSurfaceTriangle(first, third, fourth, color);
    }

    static void drawFramedWindow(int face, float x, float z, float y,
                                 float lateral, float width, float height,
                                 Color inset, Color frame, bool crossbar)
    {
        drawFacadeElement(face, x, z, y, lateral,
                          width + 0.065f, height + 0.055f, 0.034f, frame);
        drawFacadeElement(face, x, z, y, lateral,
                          width, height, 0.048f, inset);
        drawFacadeElement(face, x, z, y - height * 0.5f - 0.024f, lateral,
                          width + 0.11f, 0.043f, 0.085f, frame);
        if (crossbar)
        {
            drawFacadeElement(face, x, z, y, lateral,
                              0.026f, height, 0.063f, frame);
            drawFacadeElement(face, x, z, y - height * 0.11f, lateral,
                              width, 0.020f, 0.063f, frame);
        }
    }

    static void drawUrbanBody(const UrbanBuildingProfile &profile,
                               const UrbanMass &body, int row, int column,
                               const std::array<bool, 4> &exposedFaces)
    {
        const float bottom = body.center.y - body.size.y * 0.5f;
        const float top = body.center.y + body.size.y * 0.5f;
        const Color plaster = material(urbanBodyColor(profile), 7);
        const Color interior = material(shade(urbanBodyColor(profile), 0.72f), 7);
        const Color timber = material(Color{87, 75, 55, 255}, 7);
        const Color brick = material(Color{141, 90, 63, 255}, 6);
        rlBegin(RL_TRIANGLES);
        for (int face = 0; face < 4; ++face)
        {
            const auto panel = [&](float left, float right, float low,
                                   float high, Color color)
            {
                const Vector3 a = facadePoint(face, body.center.x, body.center.z,
                                               left, low, 0.5f);
                const Vector3 b = facadePoint(face, body.center.x, body.center.z,
                                               right, low, 0.5f);
                const Vector3 c = facadePoint(face, body.center.x, body.center.z,
                                               right, high, 0.5f);
                const Vector3 d = facadePoint(face, body.center.x, body.center.z,
                                               left, high, 0.5f);
                if (face == 0 || face == 3)
                    emitSurfaceQuad(d, c, b, a, color);
                else
                    emitSurfaceQuad(a, b, c, d, color);
            };
            const bool insideLot = face == 0 ? (row & 1) != 0
                                 : face == 1 ? (row & 1) == 0
                                 : face == 2 ? (column & 1) != 0
                                             : (column & 1) == 0;
            if (!insideLot || exposedFaces[static_cast<std::size_t>(face)])
            {
                panel(-0.5f, 0.5f, bottom, top, plaster);
                continue;
            }

            // A damaged neighboring cell reveals a room section, not a new
            // featureless exterior. These coplanar panels replace the body
            // face and stay exactly inside its original mass envelope.
            const float floor = bottom + body.size.y * 0.47f;
            const float band = bottom + body.size.y * 0.20f;
            panel(-0.5f, -0.44f, bottom, top, brick);
            panel(0.44f, 0.5f, bottom, top, brick);
            panel(-0.44f, 0.44f, bottom, band, timber);
            panel(-0.44f, 0.44f, band, floor - 0.025f, interior);
            panel(-0.44f, 0.44f, floor - 0.025f, floor + 0.025f, timber);
            const float partition = ((profile.seed >> (face + 2U)) & 1U) != 0U
                                        ? 0.17f : -0.13f;
            panel(-0.44f, partition - 0.025f, floor + 0.025f, top,
                  material(Color{153, 145, 109, 255}, 7));
            panel(partition - 0.025f, partition + 0.025f,
                  floor + 0.025f, top, timber);
            panel(partition + 0.025f, 0.44f, floor + 0.025f, top,
                  material(Color{116, 131, 113, 255}, 7));
        }
        rlEnd();
    }

    static void drawStripedAwning(int face, float x, float z, float y,
                                  Color accent, Color canvas)
    {
        // The awning is a sloping canvas prism, not a horizontal shelf.
        // Four broad stripes per cell align across the adjoining shopfront.
        rlBegin(RL_TRIANGLES);
        for (int stripe = 0; stripe < 4; ++stripe)
        {
            const float left = -0.48f + stripe * 0.24f;
            const float right = left + 0.24f;
            const Color color = (stripe & 1) == 0 ? accent : canvas;
            const Vector3 a = facadePoint(face, x, z, left, y + 0.07f, 0.505f);
            const Vector3 b = facadePoint(face, x, z, left, y, 0.67f);
            const Vector3 c = facadePoint(face, x, z, right, y, 0.67f);
            const Vector3 d = facadePoint(face, x, z, right, y + 0.07f, 0.505f);
            const Vector3 e = facadePoint(face, x, z, left, y - 0.04f, 0.67f);
            const Vector3 f = facadePoint(face, x, z, right, y - 0.04f, 0.67f);
            if (face == 0 || face == 3)
            {
                emitSurfaceQuad(d, c, b, a, color);
                emitSurfaceQuad(c, f, e, b, shade(color, 0.82f));
            }
            else
            {
                emitSurfaceQuad(a, b, c, d, color);
                emitSurfaceQuad(b, e, f, c, shade(color, 0.82f));
            }
        }
        rlEnd();
    }

    static void drawWeatheredFacade(const UrbanBuildingProfile &profile,
                                    int face, float x, float z, Color stone)
    {
        const float wallTop = 0.05f + urbanWallHeight(profile);
        const bool secondCell = (static_cast<int>(face < 2 ? x : z) & 1) != 0;
        const float corner = secondCell ? 0.46f : -0.46f;
        drawFacadeElement(face, x, z, wallTop - 0.005f, 0.0f,
                          1.0f, 0.045f, 0.068f, stone);
        drawFacadeElement(face, x, z, 0.115f, 0.0f,
                          1.0f, 0.12f, 0.030f,
                          material(shade(urbanBodyColor(profile), 0.71f), 7));
        for (int block = 0; block < 3; ++block)
            drawFacadeElement(face, x, z, 0.21f + block * (wallTop - 0.28f) / 3.0f,
                              corner, (block & 1) == 0 ? 0.09f : 0.065f,
                              0.09f, 0.048f, stone);

        const std::uint32_t wearSeed = mixUrbanSeed(profile.seed ^
            static_cast<std::uint32_t>(face * 17 + (secondCell ? 3 : 0)));
        const float patchSide = secondCell ? -0.34f : 0.34f;
        if ((wearSeed & 3U) != 3U)
        {
            const float patchY = wallTop * 0.47f;
            const Color wornPlaster = material(shade(urbanBodyColor(profile), 0.80f), 7);
            static constexpr std::array<std::array<float, 2>, 7> edge{{
                {{-0.10f, -0.13f}}, {{0.08f, -0.11f}}, {{0.11f, -0.02f}},
                {{0.075f, 0.02f}}, {{0.09f, 0.12f}}, {{-0.04f, 0.14f}},
                {{-0.11f, 0.05f}}}};
            const Vector3 center = facadePoint(face, x, z, patchSide, patchY, 0.505f);
            rlBegin(RL_TRIANGLES);
            for (std::size_t point = 0; point < edge.size(); ++point)
            {
                const auto &first = edge[point];
                const auto &second = edge[(point + 1U) % edge.size()];
                const Vector3 a = facadePoint(face, x, z, patchSide + first[0],
                                               patchY + first[1], 0.505f);
                const Vector3 b = facadePoint(face, x, z, patchSide + second[0],
                                               patchY + second[1], 0.505f);
                if (face == 0 || face == 3)
                    emitSurfaceTriangle(center, b, a, wornPlaster);
                else
                    emitSurfaceTriangle(center, a, b, wornPlaster);
            }
            rlEnd();
        }
        const Color brick = material(Color{149, 94, 65, 255}, 6);
        drawFacadeElement(face, x, z, 0.155f, patchSide,
                          0.18f, 0.11f, 0.040f, brick);
        drawFacadeElement(face, x, z, 0.215f, patchSide + 0.025f,
                          0.12f, 0.06f, 0.041f, brick);
        drawFacadeElement(face, x, z, 0.152f, patchSide,
                          0.16f, 0.012f, 0.054f,
                          material(Color{191, 154, 106, 255}, 7));
        if ((wearSeed & 3U) == 0U)
        {
            const Color pipe = material(Color{93, 110, 91, 255}, 7);
            const Vector3 bottom = facadePoint(face, x, z, corner * 0.80f,
                                                0.15f, 0.56f);
            const Vector3 top = facadePoint(face, x, z, corner * 0.80f,
                                             wallTop - 0.075f, 0.56f);
            DrawCylinderEx(bottom, top, 0.018f, 0.018f, 6, pipe);
            drawFacadeElement(face, x, z, wallTop * 0.46f, corner * 0.80f,
                              0.05f, 0.035f, 0.095f, stone);
        }
    }

    static void drawOfficeFacade(const UrbanBuildingProfile &profile, int face,
                                 float x, float z, Color glass, Color accent)
    {
        const float wallTop = 0.05f + urbanWallHeight(profile);
        const Color stone = material(Color{204, 183, 138, 255}, 7);
        const Color iron = material(Color{62, 74, 67, 255}, 7);
        const bool gateCell = (static_cast<int>(face < 2 ? x : z) & 1) == 0;
        const float gateHeight = wallTop * 0.47f;
        drawFramedWindow(face, x, z, 0.12f + gateHeight * 0.5f,
                         0.0f, 0.62f, gateHeight, glass, stone, false);
        if (gateCell)
        {
            drawFacadeElement(face, x, z, 0.12f + gateHeight * 0.5f,
                              0.0f, 0.53f, gateHeight - 0.04f, 0.055f, accent);
            for (int rib = 0; rib < 4; ++rib)
                drawFacadeElement(face, x, z,
                                  0.155f + rib * (gateHeight - 0.07f) / 3.0f,
                                  0.0f, 0.54f, 0.013f, 0.068f, iron);
            drawFacadeElement(face, x, z, 0.22f, 0.18f,
                              0.045f, 0.018f, 0.083f, stone);
        }
        else
        {
            for (float divider : {-0.17f, 0.0f, 0.17f})
                drawFacadeElement(face, x, z, 0.12f + gateHeight * 0.5f,
                                  divider, 0.025f, gateHeight - 0.025f,
                                  0.064f, accent);
        }
        const float windowY = wallTop - 0.17f;
        drawFramedWindow(face, x, z, windowY, 0.0f,
                         0.60f, 0.20f, glass, stone, false);
        drawFacadeElement(face, x, z, windowY, 0.0f,
                          0.025f, 0.17f, 0.062f, accent);
        drawFacadeElement(face, x, z, windowY, 0.0f,
                          0.53f, 0.018f, 0.062f, accent);
        drawFacadeElement(face, x, z, 0.14f + gateHeight, 0.0f,
                          0.74f, 0.045f, 0.080f, accent);
    }

    static void drawResidenceFacade(const UrbanBuildingProfile &profile,
                                    int face, float x, float z, Color glass,
                                    Color accent, Color dark)
    {
        const float wallTop = 0.05f + urbanWallHeight(profile);
        const Color stone = material(Color{227, 205, 160, 255}, 7);
        const bool doorCell = (static_cast<int>(face < 2 ? x : z) & 1) == 0;
        if (doorCell)
        {
            drawFramedWindow(face, x, z, 0.255f, -0.06f,
                             0.27f, 0.34f, dark, stone, false);
            drawFacadeElement(face, x, z, 0.25f, -0.06f,
                              0.20f, 0.29f, 0.055f, accent);
            drawFacadeElement(face, x, z, 0.28f, 0.005f,
                              0.022f, 0.025f, 0.072f,
                              material(Color{213, 172, 90, 255}, 7));
            drawFacadeElement(face, x, z, 0.073f, -0.06f,
                              0.38f, 0.05f, 0.12f, stone);
        }
        else
        {
            drawFramedWindow(face, x, z, 0.285f, 0.03f,
                             0.31f, 0.25f, glass, stone, true);
        }
        const float windowY = wallTop - 0.155f;
        drawFramedWindow(face, x, z, windowY, 0.0f,
                         0.29f, 0.23f, glass, stone, true);
        for (float side : {-1.0f, 1.0f})
        {
            const float shutter = side * 0.225f;
            drawFacadeElement(face, x, z, windowY, shutter,
                              0.105f, 0.24f, 0.055f, accent);
            for (int slat = 0; slat < 3; ++slat)
                drawFacadeElement(face, x, z, windowY - 0.068f + slat * 0.067f,
                                  shutter, 0.075f, 0.012f, 0.070f,
                                  material(shade(urbanAccentColor(profile), 1.28f), 7));
        }
        drawFacadeElement(face, x, z, 0.47f, 0.0f,
                          1.0f, 0.026f, 0.042f, stone);
    }

    static void drawMallFacade(const UrbanBuildingProfile &profile, int face,
                               float x, float z, Color glass, Color accent,
                               Color light)
    {
        const float wallTop = 0.05f + urbanWallHeight(profile);
        const Color frame = material(Color{77, 87, 71, 255}, 7);
        drawFramedWindow(face, x, z, 0.27f, 0.0f,
                         0.72f, 0.34f, glass, light, false);
        const bool doorCell = (static_cast<int>(face < 2 ? x : z) & 1) == 0;
        if (doorCell)
        {
            drawFacadeElement(face, x, z, 0.27f, -0.15f,
                              0.032f, 0.30f, 0.064f, frame);
            drawFacadeElement(face, x, z, 0.19f, 0.17f,
                              0.30f, 0.12f, 0.061f, accent);
        }
        else
        {
            drawFacadeElement(face, x, z, 0.27f, 0.0f,
                              0.65f, 0.28f, 0.057f,
                              material(Color{139, 147, 121, 255}, 7));
            for (int rib = 0; rib < 4; ++rib)
                drawFacadeElement(face, x, z, 0.17f + rib * 0.064f,
                                  0.0f, 0.65f, 0.014f, 0.072f, frame);
        }
        const float signY = wallTop - 0.09f;
        drawFacadeElement(face, x, z, signY, 0.0f,
                          0.77f, 0.16f, 0.043f, light);
        drawFacadeElement(face, x, z, signY, 0.0f,
                          0.70f, 0.115f, 0.058f, accent);
        // Broad worn sign strokes remain legible from the combat camera.
        for (int stroke = 0; stroke < 3; ++stroke)
            drawFacadeElement(face, x, z, signY, -0.22f + stroke * 0.22f,
                              0.115f, 0.032f, 0.073f, light);
        drawStripedAwning(face, x, z, 0.455f, accent, light);
    }

    static void drawBrokenWall(float width, float thickness, float height,
                                 std::uint32_t seed, bool plasterFront,
                                 Color brick, Color plaster, Color stone)
    {
        // One continuous fractured wall replaces the repeated stair-step
        // columns. Unequal break positions and a missing upper corner are
        // stable per surviving quadrant and shared by both render passes.
        const bool reverse = ((seed >> 6U) & 1U) != 0U;
        static constexpr std::array<float, 7> positions{{
            -0.5f, -0.28f, -0.20f, -0.05f, 0.04f, 0.26f, 0.5f}};
        static constexpr std::array<std::array<float, 7>, 4> fractures{{
            {{1.0f, 0.94f, 0.66f, 0.72f, 0.49f, 0.45f, 0.24f}},
            {{0.28f, 0.34f, 0.76f, 1.0f, 0.91f, 0.56f, 0.61f}},
            {{0.75f, 0.65f, 0.70f, 0.30f, 0.38f, 0.20f, 0.23f}},
            {{0.18f, 0.48f, 0.42f, 0.85f, 1.0f, 0.64f, 0.69f}}}};
        const auto &heights = fractures[(seed >> 2U) & 3U];
        const float half = thickness * 0.5f;
        rlBegin(RL_TRIANGLES);
        for (int section = 0; section < 6; ++section)
        {
            const float left = positions[static_cast<std::size_t>(section)] * width;
            const float right = positions[static_cast<std::size_t>(section + 1)] * width;
            const int first = reverse ? 6 - section : section;
            const int second = reverse ? 5 - section : section + 1;
            const float leftY = heights[static_cast<std::size_t>(first)] * height;
            const float rightY = heights[static_cast<std::size_t>(second)] * height;
            const float leftSkin = std::max(0.005f, leftY - 0.095f);
            const float rightSkin = std::max(0.005f, rightY - 0.065f);
            const Color outer = plasterFront ? plaster : brick;
            emitSurfaceQuad({left, 0.0f, half}, {right, 0.0f, half},
                            {right, rightSkin, half}, {left, leftSkin, half}, outer);
            emitSurfaceQuad({left, leftSkin, half}, {right, rightSkin, half},
                            {right, rightY, half}, {left, leftY, half}, brick);
            emitSurfaceQuad({right, 0.0f, -half}, {left, 0.0f, -half},
                            {left, leftY, -half}, {right, rightY, -half}, brick);
            emitSurfaceQuad({left, leftY, -half}, {left, leftY, half},
                            {right, rightY, half}, {right, rightY, -half}, stone);
            if (section == 0)
                emitSurfaceQuad({left, 0.0f, -half}, {left, 0.0f, half},
                                {left, leftY, half}, {left, leftY, -half}, brick);
            if (section == 5)
                emitSurfaceQuad({right, 0.0f, half}, {right, 0.0f, -half},
                                {right, rightY, -half}, {right, rightY, half}, brick);
        }
        rlEnd();
    }

    static void drawUrbanRuin(const UrbanBuildingProfile &profile,
                               const UrbanMass &mass, int row, int column,
                               bool shadow)
    {
        const float x = mass.center.x;
        const float z = mass.center.z;
        const float sideX = x < column + 0.5f ? -1.0f : 1.0f;
        const float sideZ = z < row + 0.5f ? -1.0f : 1.0f;
        const unsigned int quadrant = (sideX > 0.0f ? 1U : 0U) |
                                      (sideZ > 0.0f ? 2U : 0U);
        const std::uint32_t fracture = mixUrbanSeed(profile.seed ^
            static_cast<std::uint32_t>((row & 1) * 47 + (column & 1) * 131) ^
            ((quadrant + 1U) * 0x7f4a7c15U));
        const Color brick = shadow ? WHITE : material(Color{148, 94, 66, 255}, 6);
        const Color plaster = shadow ? WHITE : material(urbanBodyColor(profile), 7);
        const Color stone = shadow ? WHITE : material(Color{174, 155, 113, 255}, 7);
        const Color inside = shadow ? WHITE : material(Color{91, 91, 70, 255}, 7);
        constexpr float base = 0.05f;
        constexpr float thickness = 0.088f;
        const float halfWidth = mass.size.x * 0.5f;
        const float halfDepth = mass.size.z * 0.5f;
        const bool shortReturn = ((fracture >> 9U) & 1U) != 0U;
        DrawCube({x, base + 0.025f, z}, mass.size.x, 0.05f, mass.size.z, inside);

        rlPushMatrix();
        rlTranslatef(x, base, z + sideZ * (halfDepth - thickness * 0.5f));
        if (sideZ < 0.0f)
            rlRotatef(180.0f, 0.0f, 1.0f, 0.0f);
        drawBrokenWall(mass.size.x, thickness, mass.size.y, fracture,
                       (fracture & 3U) != 0U, brick, plaster, stone);
        rlPopMatrix();

        rlPushMatrix();
        rlTranslatef(x + sideX * (halfWidth - thickness * 0.5f), base, z);
        rlRotatef(sideX > 0.0f ? 90.0f : -90.0f, 0.0f, 1.0f, 0.0f);
        drawBrokenWall(mass.size.z, thickness,
                       mass.size.y * (shortReturn ? 0.43f : 0.82f),
                       mixUrbanSeed(fracture ^ 0x85ebca6bU),
                       (fracture & 4U) != 0U, brick, plaster, stone);
        rlPopMatrix();

        // The broken floor and fallen beam stay inside the same living
        // quadrant; no debris bridges a cleared path or creates new cover.
        DrawCube({x - sideX * 0.028f, base + 0.065f,
                  z - sideZ * 0.02f}, 0.25f, 0.048f, 0.24f, stone);
        const float lean = ((fracture >> 12U) & 1U) != 0U ? 0.14f : 0.075f;
        DrawCylinderEx({x - sideX * 0.135f, base + 0.09f, z + sideZ * 0.075f},
                       {x + sideX * 0.045f, base + lean, z - sideZ * 0.10f},
                       0.027f, 0.022f, 4,
                       shadow ? WHITE : material(Color{90, 69, 48, 255}, 7));
        DrawCube({x - sideX * 0.10f, base + 0.065f, z - sideZ * 0.12f},
                 0.105f, 0.055f, 0.074f, brick);
    }

    static void drawRoofBand(float firstX, float secondX, float firstY,
                               float secondY, float bottom,
                               Color surface, Color gable)
    {
        if (secondX < firstX)
        {
            std::swap(firstX, secondX);
            std::swap(firstY, secondY);
        }
        emitSurfaceQuad({firstX, firstY, -0.5f}, {firstX, firstY, 0.5f},
                        {secondX, secondY, 0.5f}, {secondX, secondY, -0.5f}, surface);
        emitSurfaceQuad({firstX, bottom, 0.5f}, {secondX, bottom, 0.5f},
                        {secondX, secondY, 0.5f}, {firstX, firstY, 0.5f}, gable);
        emitSurfaceQuad({secondX, bottom, -0.5f}, {firstX, bottom, -0.5f},
                        {firstX, firstY, -0.5f}, {secondX, secondY, -0.5f}, gable);
        if (firstX == -0.5f)
            emitSurfaceQuad({firstX, bottom, -0.5f}, {firstX, bottom, 0.5f},
                            {firstX, firstY, 0.5f}, {firstX, firstY, -0.5f}, gable);
        if (secondX == 0.5f)
            emitSurfaceQuad({secondX, bottom, 0.5f}, {secondX, bottom, -0.5f},
                            {secondX, secondY, -0.5f}, {secondX, secondY, 0.5f}, gable);
    }

    static void drawUrbanRoof(const UrbanBuildingProfile &profile,
                               const UrbanMass &roof, int row, int column,
                               bool shadow)
    {
        const float bottom = roof.center.y - roof.size.y * 0.5f;
        const float top = roof.center.y + roof.size.y * 0.5f;
        if (profile.kind != UrbanBuildingKind::Mall)
        {
            const bool alongX = profile.roofVariant == 1U;
            const bool rising = alongX ? (row & 1) != 0 : (column & 1) == 0;
            const bool farEnd = alongX ? (column & 1) != 0 : (row & 1) != 0;
            const float low = bottom + 0.022f;
            const float roofTop = top - 0.032f;
            static constexpr std::array<Color, 3> roofColors{{
                Color{153, 79, 52, 255}, Color{158, 93, 59, 255},
                Color{172, 110, 65, 255}}};
            Color roofColor = material(roofColors[profile.roofVariant % 3U], 7);
            if (profile.kind == UrbanBuildingKind::Office)
                roofColor = material(Color{74, 112, 97, 255}, 7);
            const Color topColor = shadow ? WHITE : roofColor;
            const Color edgeColor = shadow ? WHITE : shade(roofColor, 0.73f);
            const auto fromEdge = [rising](float amount)
            {
                return rising ? -0.5f + amount : 0.5f - amount;
            };

            rlPushMatrix();
            rlTranslatef(roof.center.x, 0.0f, roof.center.z);
            if (alongX)
                rlRotatef(90.0f, 0.0f, 1.0f, 0.0f);
            if (profile.kind == UrbanBuildingKind::Office)
            {
                // A raised glazed monitor gives the workshop a factory
                // silhouette. Its two halves meet at the lot ridge, and
                // every panel remains below the original profile height.
                const float shoulder = low + (roofTop - low) * 0.28f;
                const float clerestory = roofTop - 0.045f;
                const Color glass = shadow ? WHITE
                    : material(Color{46, 75, 70, 255}, 7);
                rlBegin(RL_TRIANGLES);
                drawRoofBand(fromEdge(0.0f), fromEdge(0.60f), low, shoulder,
                             bottom, topColor, edgeColor);
                drawRoofBand(fromEdge(0.60f), fromEdge(0.60f),
                             rising ? shoulder : clerestory,
                             rising ? clerestory : shoulder,
                             bottom, glass, edgeColor);
                drawRoofBand(fromEdge(0.60f), fromEdge(1.0f), clerestory, roofTop,
                             bottom, topColor, edgeColor);
                rlEnd();
                if (!shadow)
                {
                    const Color seam = material(shade(roofColor, 1.17f), 7);
                    DrawCube({fromEdge(0.605f), clerestory - 0.008f, 0.0f},
                             0.028f, 0.024f, 0.99f, seam);
                    DrawCube({fromEdge(0.30f), low + (shoulder - low) * 0.5f,
                              0.0f}, 0.014f, 0.012f, 0.99f, seam);
                }
            }
            else
            {
                // Terracotta hips turn down toward the ends of the lot.
                // The center bend creates a heavy, slightly bellied roof
                // rather than four identical flat wedge tiles.
                const bool hipped = profile.roofVariant != 1U;
                const auto heightAt = [&](float px, float pz)
                {
                    const float across = rising ? px + 0.5f : 0.5f - px;
                    const float along = farEnd ? 0.5f - pz : pz + 0.5f;
                    const float pitch = across < 0.5f ? across * 1.12f
                        : 0.56f + (across - 0.5f) * 0.88f;
                    return low + (roofTop - low) *
                        (hipped ? std::min(pitch, along * 2.0f) : pitch);
                };
                rlBegin(RL_TRIANGLES);
                for (int strip = 0; strip < 2; ++strip)
                {
                    for (int end = 0; end < 2; ++end)
                    {
                        const float x0 = -0.5f + strip * 0.5f;
                        const float x1 = x0 + 0.5f;
                        const float z0 = -0.5f + end * 0.5f;
                        const float z1 = z0 + 0.5f;
                        emitSurfaceQuad({x0, heightAt(x0, z0), z0},
                                        {x0, heightAt(x0, z1), z1},
                                        {x1, heightAt(x1, z1), z1},
                                        {x1, heightAt(x1, z0), z0}, topColor);
                        if (end == 0)
                            emitSurfaceQuad({x1, bottom, z0}, {x0, bottom, z0},
                                            {x0, heightAt(x0, z0), z0},
                                            {x1, heightAt(x1, z0), z0}, edgeColor);
                        if (end == 1)
                            emitSurfaceQuad({x0, bottom, z1}, {x1, bottom, z1},
                                            {x1, heightAt(x1, z1), z1},
                                            {x0, heightAt(x0, z1), z1}, edgeColor);
                        if (strip == 0)
                            emitSurfaceQuad({x0, bottom, z0}, {x0, bottom, z1},
                                            {x0, heightAt(x0, z1), z1},
                                            {x0, heightAt(x0, z0), z0}, edgeColor);
                        if (strip == 1)
                            emitSurfaceQuad({x1, bottom, z1}, {x1, bottom, z0},
                                            {x1, heightAt(x1, z0), z0},
                                            {x1, heightAt(x1, z1), z1}, edgeColor);
                    }
                }
                rlEnd();
                const int lotPart = (row & 1) * 2 + (column & 1);
                if (lotPart == static_cast<int>((profile.seed >> 24U) & 3U))
                {
                    const float chimneyX = fromEdge(0.24f);
                    const float chimneyZ = farEnd ? -0.14f : 0.14f;
                    const float chimneyBase = heightAt(chimneyX, chimneyZ) - 0.020f;
                    const Color brick = shadow ? WHITE
                        : material(Color{129, 87, 63, 255}, 6);
                    const Color cap = shadow ? WHITE
                        : material(Color{196, 166, 116, 255}, 7);
                    DrawCube({chimneyX, (top + chimneyBase) * 0.5f, chimneyZ},
                             0.13f, top - chimneyBase, 0.16f, brick);
                    DrawCube({chimneyX, top - 0.017f, chimneyZ},
                             0.18f, 0.034f, 0.20f, cap);
                    if (!shadow)
                        DrawCube({chimneyX, top - 0.001f, chimneyZ},
                                 0.084f, 0.002f, 0.105f,
                                 material(Color{51, 57, 48, 255}, 7));
                }
                if (!shadow)
                {
                    const Color course = material(shade(roofColor, 1.12f), 7);
                    // Two broad courses follow the facets; no tiled micro-
                    // geometry is needed at the actual gameplay distance.
                    for (int strip = 0; strip < 2; ++strip)
                    {
                        const float px = fromEdge(0.22f + strip * 0.43f);
                        const float z0 = hipped ? (farEnd ? -0.495f : -0.02f) : -0.495f;
                        const float z1 = hipped ? (farEnd ? 0.02f : 0.495f) : 0.495f;
                        rlBegin(RL_TRIANGLES);
                        emitSurfaceQuad({px - 0.009f, heightAt(px - 0.009f, z0) + 0.002f, z0},
                                        {px - 0.009f, heightAt(px - 0.009f, z1) + 0.002f, z1},
                                        {px + 0.009f, heightAt(px + 0.009f, z1) + 0.002f, z1},
                                        {px + 0.009f, heightAt(px + 0.009f, z0) + 0.002f, z0}, course);
                        rlEnd();
                    }
                }
            }
            rlPopMatrix();
            return;
        }

        const int lotPart = (row & 1) * 2 + (column & 1);
        const Color iron = shadow ? WHITE : material(Color{72, 96, 87, 255}, 7);
        const Color rim = shadow ? WHITE : material(Color{150, 157, 126, 255}, 7);
        const float x = roof.center.x;
        const float z = roof.center.z;
        if (lotPart == 0)
        {
            // One water drum per lot, accompanied by three different utility
            // forms rather than repeating the same tank on every roof cell.
            const float radius = roof.size.z * 0.45f;
            DrawCylinder({x, bottom, z}, radius, radius,
                         roof.size.y, 10, iron);
            DrawCylinder({x, top - 0.022f, z}, radius + 0.012f, radius + 0.012f,
                         0.022f, 10, rim);
            DrawCylinder({x, bottom + 0.020f, z}, radius + 0.010f, radius + 0.010f,
                         0.018f, 10, rim);
        }
        else if (lotPart == 1)
        {
            DrawCube({x, bottom + roof.size.y * 0.38f, z},
                     0.26f, roof.size.y * 0.76f, 0.24f, iron);
            DrawCube({x, top - roof.size.y * 0.12f, z},
                     0.34f, roof.size.y * 0.24f, 0.30f, rim);
        }
        else if (lotPart == 2)
        {
            DrawCube({x, bottom + 0.025f, z}, 0.30f, 0.05f, 0.28f, rim);
            DrawCylinder({x, bottom + 0.05f, z}, 0.073f, 0.073f,
                         roof.size.y - 0.068f, 8, iron);
            DrawCylinder({x, top - 0.018f, z}, 0.113f, 0.083f,
                         0.018f, 8, rim);
        }
        else
        {
            DrawCube(roof.center, roof.size.x, roof.size.y,
                     roof.size.z, iron);
            if (!shadow)
            {
                DrawCube({x, top - 0.018f, z}, 0.31f, 0.024f, 0.27f,
                         material(Color{128, 159, 134, 255}, 7));
                DrawCube({x, top - 0.002f, z}, 0.025f, 0.004f, 0.27f, rim);
            }
        }
    }

    struct Building
    {
        float x;
        float z;
        float width;
        float depth;
        float height;
        Color wall;
        Color accent;
        int style;
    };

    static Color shade(Color color, float factor)
    {
        const auto channel = [factor](unsigned char value) {
            return static_cast<unsigned char>(std::clamp(value * factor, 0.0f, 255.0f));
        };
        return {channel(color.r), channel(color.g), channel(color.b), color.a};
    }

    static void drawBuilding(const Building &building)
    {
        const float baseY = building.height * 0.5f;
        const Color wall = material(building.wall, 7);
        const Color accent = material(building.accent, 7);
        const Color dark = shade(wall, 0.48f);
        const Color light = shade(wall, 1.18f);
        DrawCube({building.x + 0.34f, 0.012f, building.z + 0.28f},
                 building.width + 0.24f, 0.024f, building.depth + 0.24f,
                 Color{13, 18, 18, 105});
        DrawCube({building.x, baseY, building.z}, building.width, building.height,
                 building.depth, wall);
        DrawCube({building.x, 0.16f, building.z}, building.width + 0.28f, 0.32f,
                 building.depth + 0.28f, dark);
        DrawCube({building.x, building.height + 0.10f, building.z},
                 building.width + 0.22f, 0.20f, building.depth + 0.22f, light);

        // The distant town uses the same broad plaster frames, shutters and
        // workshop doors as the destructible streets, at a quieter density.
        const int columns = std::max(2, static_cast<int>(building.width / 1.8f));
        const int floors = std::max(2, static_cast<int>(building.height / 1.8f));
        const float frontZ = building.z + building.depth * 0.5f + 0.016f;
        for (int column = 0; column < columns; ++column)
        {
            const float x = building.x - building.width * 0.5f +
                            (column + 0.5f) * building.width / columns;
            for (int floor = 0; floor < floors; ++floor)
            {
                const float y = 0.76f + floor * (building.height - 0.60f) / floors;
                const float width = building.width / columns * 0.49f;
                const float height = floor == 0 ? 1.05f : 0.69f;
                DrawCube({x, y, frontZ}, width + 0.19f, height + 0.17f, 0.10f, light);
                DrawCube({x, y, frontZ + 0.065f}, width, height, 0.065f,
                         material(Color{36, 61, 55, 255}, 7));
                DrawCube({x, y - height * 0.5f - 0.075f, frontZ + 0.08f},
                         width + 0.30f, 0.12f, 0.21f, light);
                if (floor == 0 && building.style != 0)
                {
                    DrawCube({x, y, frontZ + 0.11f}, width - 0.08f,
                             height - 0.06f, 0.06f, accent);
                    for (int rib = 0; rib < 4; ++rib)
                        DrawCube({x, y - 0.36f + rib * 0.24f, frontZ + 0.16f},
                                 width - 0.09f, 0.025f, 0.04f, dark);
                }
                else
                {
                    DrawCube({x, y, frontZ + 0.13f}, 0.065f, height, 0.035f, light);
                    DrawCube({x, y, frontZ + 0.13f}, width, 0.055f, 0.035f, light);
                    if (building.style == 0)
                        for (float side : {-1.0f, 1.0f})
                            DrawCube({x + side * (width * 0.5f + 0.22f), y,
                                      frontZ + 0.08f}, 0.24f, height + 0.08f, 0.12f, accent);
                }
            }
        }
        for (float side : {-1.0f, 1.0f})
        {
            const float x = building.x + side * (building.width * 0.5f - 0.10f);
            DrawCube({x, baseY, frontZ + 0.015f}, 0.17f, building.height,
                     0.12f, light);
        }
        DrawCube({building.x, 1.47f, frontZ + 0.05f},
                 building.width, 0.12f, 0.18f, accent);
        const float sideX = building.x + building.width * 0.5f + 0.035f;
        for (int floor = 1; floor < floors; ++floor)
            for (float side : {-1.0f, 1.0f})
            {
                const float y = 0.76f + floor * (building.height - 0.60f) / floors;
                const float z = building.z + side * building.depth * 0.25f;
                DrawCube({sideX, y, z}, 0.10f, 0.86f, 0.88f, light);
                DrawCube({sideX + 0.068f, y, z}, 0.06f, 0.69f, 0.69f,
                         material(Color{36, 61, 55, 255}, 7));
                DrawCube({sideX + 0.11f, y, z}, 0.045f, 0.69f, 0.055f, accent);
            }

        // Low pitched roofs and factory water towers replace the office-block
        // skyline. These structures remain entirely outside the playfield.
        if (building.style == 2)
        {
            DrawCube({building.x - building.width * 0.18f, building.height + 0.42f, building.z},
                     building.width * 0.28f, 0.65f, building.depth * 0.35f, dark);
            const float tankX = building.x + building.width * 0.20f;
            DrawCylinder({tankX, building.height + 0.20f, building.z},
                         0.48f, 0.48f, 0.88f, 10, accent);
            DrawCylinder({tankX, building.height + 1.02f, building.z},
                         0.53f, 0.53f, 0.09f, 10, light);
            DrawCube({building.x - building.width * 0.31f,
                      building.height + 0.96f, building.z - 0.48f},
                     0.36f, 1.50f, 0.42f, accent);
        }
        else
        {
            const float x0 = building.x - building.width * 0.525f;
            const float x1 = building.x + building.width * 0.525f;
            const float z0 = building.z - building.depth * 0.525f;
            const float z1 = building.z + building.depth * 0.525f;
            const float edgeY = building.height + 0.21f;
            const float ridgeY = building.height + 0.87f;
            const Color roofColor = building.style == 0
                                       ? material(Color{140, 81, 54, 255}, 7)
                                       : material(Color{69, 106, 89, 255}, 7);
            rlBegin(RL_TRIANGLES);
            emitSurfaceQuad({x0, edgeY, z0}, {x0, edgeY, z1},
                            {building.x, ridgeY, z1}, {building.x, ridgeY, z0}, roofColor);
            emitSurfaceQuad({building.x, ridgeY, z0}, {building.x, ridgeY, z1},
                            {x1, edgeY, z1}, {x1, edgeY, z0}, roofColor);
            emitSurfaceTriangle({x0, edgeY, z1}, {x1, edgeY, z1},
                                {building.x, ridgeY, z1}, shade(roofColor, 0.72f));
            emitSurfaceTriangle({x1, edgeY, z0}, {x0, edgeY, z0},
                                {building.x, ridgeY, z0}, shade(roofColor, 0.72f));
            rlEnd();
        }
    }

    static Color material(Color color, unsigned char tag)
    {
        color.a = tag;
        return color;
    }

    Texture2D grass_{};
    Texture2D masonry_{};
};

#endif // TANKS3D_ENVIRONMENT_ASSETS_H

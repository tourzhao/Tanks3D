#ifndef TANKS3D_ENVIRONMENT_ASSETS_H
#define TANKS3D_ENVIRONMENT_ASSETS_H

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>

// Incremental environment art layer.  It owns only the authored ground
// texture; the modular skyline is assembled from reusable architectural
// pieces so it remains lightweight in the local isometric combat view.
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

    // Visible geometry and the shadow pass consume this same mass plan. Full
    // buildings use a body, a thin roof cap, and one separated low roof
    // feature; damaged buildings use only their surviving classic brick
    // quadrants as shortened ruins.
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
            plan.masses[0] = {{x, baseY + profile.coreHeight * 0.5f, z},
                              {0.92f, profile.coreHeight, 0.92f}};
            const float bodyTop = baseY + profile.coreHeight;
            constexpr float capHeight = 0.024f;
            constexpr float accessoryClearance = 0.008f;
            plan.masses[1] = {{x, bodyTop + capHeight * 0.5f, z},
                              {0.96f, capHeight, 0.96f}};
            plan.count = 2;

            const float roofBase = bodyTop + capHeight + accessoryClearance;
            const float roofHeight = std::max(
                0.012f, profile.totalHeight - roofBase);
            const float offsetX =
                (static_cast<float>((profile.seed >> 21U) & 3U) - 1.5f) * 0.065f;
            const float offsetZ =
                (static_cast<float>((profile.seed >> 23U) & 3U) - 1.5f) * 0.065f;
            Vector3 roofSize{0.34f, roofHeight, 0.30f};
            if (profile.kind == UrbanBuildingKind::Residence)
                roofSize = {0.15f, roofHeight, 0.15f};
            else if (profile.kind == UrbanBuildingKind::Mall)
                roofSize = {0.38f, roofHeight, 0.32f};
            plan.masses[2] = {
                {x + offsetX,
                 roofBase + roofHeight * 0.5f,
                 z + offsetZ},
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
        const Color body = material(urbanBodyColor(profile), 7);
        const Color accent = material(urbanAccentColor(profile), 7);
        const Color glass = material(urbanGlassColor(profile), 4);
        const Color dark = material(shade(urbanBodyColor(profile), 0.48f), 7);
        const Color light = material(shade(urbanBodyColor(profile), 1.16f), 7);

        DrawCube({x + 0.10f, 0.006f, z + 0.09f}, 0.98f, 0.012f, 0.98f,
                 Color{18, 24, 21, 82});
        if (brickMask != 0x0fU)
        {
            for (int index = 0; index < plan.count; ++index)
            {
                const UrbanMass &mass = plan.masses[static_cast<std::size_t>(index)];
                drawMasonryCore(mass.center.x, mass.center.z,
                                mass.size.x, mass.size.y, mass.size.z);
                DrawCube({mass.center.x, 0.047f, mass.center.z},
                         mass.size.x + 0.025f, 0.085f,
                         mass.size.z + 0.025f, dark);
                const float top = mass.center.y + mass.size.y * 0.5f;
                DrawCube({mass.center.x, top + 0.018f, mass.center.z},
                         mass.size.x + 0.018f, 0.036f,
                         mass.size.z + 0.018f,
                         ((profile.seed >> index) & 1U) != 0U ? accent : light);
                DrawCube({mass.center.x + 0.09f, top + 0.043f,
                          mass.center.z - 0.07f},
                         0.028f, 0.07f, 0.028f,
                         Color{74, 70, 63, 2});
            }
            return;
        }

        const UrbanMass &bodyMass = plan.masses[0];
        DrawCube(bodyMass.center, bodyMass.size.x, bodyMass.size.y,
                 bodyMass.size.z, body);
        DrawCube({x, 0.055f, z}, 0.98f, 0.10f, 0.98f, dark);
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
        }

        const UrbanMass &roof = plan.masses[2];
        const Color roughSkylight = material(urbanGlassColor(profile), 7);
        const Color roofColor = profile.kind == UrbanBuildingKind::Mall
                                    ? roughSkylight
                                    : profile.kind == UrbanBuildingKind::Office
                                          ? dark
                                          : accent;
        DrawCube(roof.center, roof.size.x, roof.size.y, roof.size.z,
                 roofColor);
        if (profile.kind == UrbanBuildingKind::Office)
        {
            constexpr float panelDepth = 0.018f;
            const float side = profile.roofVariant == 0U ? -1.0f : 1.0f;
            const float panelZ = roof.center.z + side *
                (roof.size.z * 0.5f + panelDepth * 0.5f + 0.002f);
            DrawCube({roof.center.x, roof.center.y, panelZ},
                     roof.size.x * 0.72f, roof.size.y * 0.46f, panelDepth,
                     material(Color{119, 139, 143, 255}, 7));
        }
    }

    static void drawUrbanBuildingShadow(int stage, int row, int column,
                                        unsigned char brickMask)
    {
        const UrbanBuildingProfile profile = urbanProfile(stage, row, column);
        const UrbanMassPlan plan = urbanMassPlan(profile, row, column, brickMask);
        // Tiny roof accessories do not cast into the shared shadow map. Their
        // sub-pixel self-shadow used to crawl during camera shake; the body and
        // cap retain the complete readable building silhouette.
        const int shadowMassCount = brickMask == 0x0fU
                                        ? std::min(plan.count, 2)
                                        : plan.count;
        for (int index = 0; index < shadowMassCount; ++index)
        {
            const UrbanMass &mass = plan.masses[static_cast<std::size_t>(index)];
            DrawCube(mass.center, mass.size.x, mass.size.y, mass.size.z, WHITE);
        }
    }

    void load(const std::filesystem::path &resourceRoot)
    {
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

    void drawBackdropCity() const
    {
        // Buildings sit beyond the playfield, so they establish scale and a
        // war-era urban setting without altering the original collision map.
        static constexpr std::array<Building, 9> buildings{{
            {-5.0f, -4.8f, 5.2f, 4.1f, 5.8f, Color{76, 86, 91, 255}, Color{132, 89, 63, 255}, 1},
            { 0.3f, -5.4f, 4.1f, 3.3f, 4.2f, Color{109, 99, 84, 255}, Color{66, 80, 82, 255}, 0},
            { 5.0f, -5.6f, 4.7f, 3.8f, 6.8f, Color{81, 94, 101, 255}, Color{151, 102, 65, 255}, 2},
            {10.2f, -6.1f, 4.8f, 4.4f, 5.1f, Color{116, 105, 91, 255}, Color{64, 78, 83, 255}, 1},
            {15.6f, -5.8f, 5.0f, 3.9f, 7.4f, Color{74, 87, 94, 255}, Color{145, 94, 61, 255}, 2},
            {21.2f, -5.7f, 4.6f, 3.5f, 5.7f, Color{104, 96, 84, 255}, Color{57, 72, 78, 255}, 0},
            {26.4f, -5.3f, 4.9f, 4.1f, 6.5f, Color{74, 86, 91, 255}, Color{139, 88, 57, 255}, 1},
            {31.7f, -5.0f, 5.5f, 4.0f, 4.8f, Color{105, 96, 82, 255}, Color{60, 76, 80, 255}, 0},
            {37.3f, -5.2f, 5.2f, 4.2f, 7.0f, Color{71, 84, 91, 255}, Color{144, 92, 58, 255}, 2}}};

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
        const ForestPlan plan = forestPlan(stage, row, column, edgeMask);
        for (int index = 0; index < plan.treeCount; ++index)
            drawForestTrunk(plan.trees[static_cast<std::size_t>(index)]);
    }

    // Call after vehicles in BLEND_ALPHA mode. Each tree is one connected,
    // faceted crown rather than a stack of intersecting transparent spheres.
    void drawForestCanopy(int stage, int row, int column,
                          unsigned char edgeMask) const
    {
        const ForestPlan plan = forestPlan(stage, row, column, edgeMask);
        for (int index = 0; index < plan.treeCount; ++index)
        {
            const ForestTree &tree = plan.trees[static_cast<std::size_t>(index)];
            drawTieredForestCrown(tree, forestPalette(tree.palette), 1.0f);
        }
    }

    // Only the two mature canopy cores cast into the shadow map. Their smaller
    // footprint yields readable dappled shade instead of an opaque dark tile.
    static void drawForestShadow(int stage, int row, int column,
                                 unsigned char edgeMask)
    {
        const ForestPlan plan = forestPlan(stage, row, column, edgeMask);
        static constexpr std::array<Color, 4> shadowColors{{
            WHITE, WHITE, WHITE, WHITE}};
        for (int index = 0; index < 2; ++index)
        {
            drawTieredForestCrown(
                plan.trees[static_cast<std::size_t>(index)], shadowColors, 0.62f);
        }
    }

    // Transitional wrapper for callers that have not yet supplied stage and
    // edge context. It still uses the new connected canopy geometry.
    void drawForestTile(int row, int column) const
    {
        drawForestCanopy(1, row, column, 0U);
    }

private:
    static constexpr float kForestPi = 3.14159265358979323846f;

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
            {{Color{24, 50, 29, 132}, Color{39, 76, 35, 126},
              Color{61, 97, 42, 116}, Color{83, 119, 53, 104}}},
            {{Color{22, 47, 32, 132}, Color{34, 70, 39, 126},
              Color{53, 91, 46, 116}, Color{73, 111, 56, 104}}},
            {{Color{32, 53, 27, 132}, Color{51, 78, 33, 126},
              Color{72, 98, 40, 116}, Color{94, 118, 50, 104}}}}};
        return palettes[requestedPalette % palettes.size()];
    }

    static Vector3 forestLerp(Vector3 first, Vector3 second, float amount)
    {
        return {first.x + (second.x - first.x) * amount,
                first.y + (second.y - first.y) * amount,
                first.z + (second.z - first.z) * amount};
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
        DrawCylinderEx(tree.trunkBase, tree.trunkTop,
                       tree.trunkBaseRadius, tree.trunkTopRadius,
                       tree.sapling ? 7 : 8, bark);

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
            DrawCylinderEx(start, end, tree.trunkTopRadius * 0.72f,
                           tree.trunkTopRadius * 0.24f, 6, branchColor);
        }
    }

    static void emitForestTriangle(Vector3 first, Vector3 second, Vector3 third,
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

    static void drawTieredForestCrown(
        const ForestTree &tree, const std::array<Color, 4> &colors,
        float radiusScale)
    {
        static constexpr int segmentCount = 8;
        static constexpr int ringCount = 4;
        static constexpr std::array<float, ringCount> ringHeight{{
            0.0f, 0.27f, 0.57f, 0.81f}};
        static constexpr std::array<float, ringCount> ringRadius{{
            0.46f, 0.98f, 1.0f, 0.72f}};
        std::array<std::array<Vector3, segmentCount>, ringCount> rings{};
        const float crownHeight = tree.crownTopHeight - tree.crownBaseHeight;

        for (int ring = 0; ring < ringCount; ++ring)
        {
            const float heightFraction = ringHeight[static_cast<std::size_t>(ring)];
            const float y = tree.crownBaseHeight + crownHeight * heightFraction;
            const float centerX = tree.trunkTop.x +
                forestSigned(tree.seed ^
                             (0x7f4a7c15U + static_cast<std::uint32_t>(ring))) *
                    0.008f;
            const float centerZ = tree.trunkTop.z +
                forestSigned(tree.seed ^
                             (0x94d049bbU + static_cast<std::uint32_t>(ring))) *
                    0.008f;
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const std::uint32_t detailSeed =
                    tree.seed ^
                    (0x165667b1U * static_cast<std::uint32_t>(ring + 1)) ^
                    (0xd3a2646cU * static_cast<std::uint32_t>(segment + 1));
                const float irregularity = 0.96f + forestUnit(detailSeed) * 0.08f;
                const float angle = tree.crownTwist +
                                    static_cast<float>(segment) *
                                        (2.0f * kForestPi /
                                         static_cast<float>(segmentCount)) +
                                    static_cast<float>(ring) * 0.055f;
                const float radius = tree.crownRadius *
                                     ringRadius[static_cast<std::size_t>(ring)] *
                                     radiusScale * irregularity;
                rings[static_cast<std::size_t>(ring)]
                     [static_cast<std::size_t>(segment)] = {
                    centerX + std::cos(angle) * radius * tree.crownScaleX,
                    y,
                    centerZ + std::sin(angle) * radius * tree.crownScaleZ};
            }
        }

        const Vector3 apex{
            tree.trunkTop.x + forestSigned(tree.seed ^ 0xfd7046c5U) * 0.008f,
            tree.crownTopHeight,
            tree.trunkTop.z + forestSigned(tree.seed ^ 0xb55a4f09U) * 0.008f};
        rlBegin(RL_TRIANGLES);
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
                const Color color = colors[static_cast<std::size_t>(ring)];
                emitForestTriangle(lower, upperNext, lowerNext, color);
                emitForestTriangle(lower, upper, upperNext, color);
            }
        }
        for (int segment = 0; segment < segmentCount; ++segment)
        {
            const int next = (segment + 1) % segmentCount;
            const Vector3 lower =
                rings[ringCount - 1][static_cast<std::size_t>(segment)];
            const Vector3 lowerNext =
                rings[ringCount - 1][static_cast<std::size_t>(next)];
            emitForestTriangle(lower, apex, lowerNext, colors[3]);
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
            Color{72, 91, 103, 255}, Color{91, 101, 109, 255},
            Color{68, 85, 92, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{166, 132, 99, 255}, Color{151, 111, 91, 255},
            Color{180, 158, 119, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{145, 147, 139, 255}, Color{167, 151, 126, 255},
            Color{132, 153, 151, 255}}};
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
            Color{38, 53, 62, 255}, Color{55, 68, 75, 255},
            Color{42, 73, 76, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{103, 66, 49, 255}, Color{78, 88, 81, 255},
            Color{119, 76, 57, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{207, 105, 49, 255}, Color{45, 139, 145, 255},
            Color{197, 150, 49, 255}}};
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
            Color{38, 75, 91, 255}, Color{47, 83, 106, 255},
            Color{41, 96, 104, 255}}};
        static constexpr std::array<Color, 3> residence{{
            Color{35, 50, 57, 255}, Color{48, 64, 68, 255},
            Color{38, 60, 70, 255}}};
        static constexpr std::array<Color, 3> mall{{
            Color{27, 71, 84, 255}, Color{33, 85, 91, 255},
            Color{44, 70, 93, 255}}};
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
        constexpr float halfBody = 0.46f;
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

    static void drawOfficeFacade(const UrbanBuildingProfile &profile, int face,
                                 float x, float z, Color glass, Color accent)
    {
        const float usableHeight = std::max(0.54f, profile.coreHeight - 0.16f);
        const float floorStep = usableHeight / static_cast<float>(profile.floors);
        for (int floor = 0; floor < profile.floors; ++floor)
        {
            const float y = 0.14f + (floor + 0.5f) * floorStep;
            const float windowHeight = std::min(0.15f, floorStep * 0.52f);
            drawFacadeElement(face, x, z, y, -0.20f, 0.29f,
                              windowHeight, 0.032f, glass);
            drawFacadeElement(face, x, z, y, 0.20f, 0.29f,
                              windowHeight, 0.032f,
                              ((profile.seed >> floor) & 7U) == 0U
                                  ? material(Color{225, 174, 93, 255}, 4)
                                  : glass);
        }
        drawFacadeElement(face, x, z, 0.14f + usableHeight * 0.5f,
                          0.0f, 0.045f, usableHeight, 0.040f, accent);
    }

    static void drawResidenceFacade(const UrbanBuildingProfile &profile,
                                    int face, float x, float z, Color glass,
                                    Color accent, Color dark)
    {
        const float usableHeight = std::max(0.48f, profile.coreHeight - 0.18f);
        const float floorStep = usableHeight / static_cast<float>(profile.floors);
        for (int floor = 0; floor < profile.floors; ++floor)
        {
            const float y = 0.15f + (floor + 0.5f) * floorStep;
            const float windowHeight = std::min(0.19f, floorStep * 0.52f);
            drawFacadeElement(face, x, z, y, -0.20f, 0.20f,
                              windowHeight, 0.035f, glass);
            drawFacadeElement(face, x, z, y, 0.20f, 0.20f,
                              windowHeight, 0.035f, glass);
            if (floor > 0 && ((profile.seed >> (face + floor)) & 1U) != 0U)
            {
                drawFacadeElement(face, x, z, y - windowHeight * 0.55f,
                                  0.0f, 0.72f, 0.040f, 0.12f, accent);
                drawFacadeElement(face, x, z, y + 0.02f,
                                  0.0f, 0.055f, windowHeight * 0.95f,
                                  0.135f, dark);
            }
        }
        if (face == static_cast<int>((profile.seed >> 25U) & 3U))
            drawFacadeElement(face, x, z, 0.245f, 0.0f,
                              0.25f, 0.36f, 0.045f, dark);
    }

    static void drawMallFacade(const UrbanBuildingProfile &profile, int face,
                               float x, float z, Color glass, Color accent,
                               Color light)
    {
        const float coreTop = 0.05f + profile.coreHeight;
        drawFacadeElement(face, x, z, 0.29f, 0.0f,
                          0.72f, 0.32f, 0.038f, glass);
        drawFacadeElement(face, x, z, coreTop - 0.14f, 0.0f,
                          0.77f, 0.13f, 0.045f, accent);
        drawFacadeElement(face, x, z, coreTop - 0.30f, 0.0f,
                          0.82f, 0.050f, 0.14f, light);
        const float signOffset =
            ((profile.seed >> (face + 4U)) & 1U) != 0U ? -0.22f : 0.22f;
        drawFacadeElement(face, x, z, coreTop - 0.14f, signOffset,
                          0.18f, 0.055f, 0.055f,
                          material(Color{244, 207, 107, 255}, 4));
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

        // Vertical pilasters and recessed south-facing window bays give the
        // skyline readable architecture instead of featureless cuboids.
        const int columns = std::max(2, static_cast<int>(building.width / 0.9f));
        const int floors = std::max(2, static_cast<int>(building.height / 1.15f));
        const float frontZ = building.z + building.depth * 0.5f + 0.016f;
        for (int column = 0; column < columns; ++column)
        {
            const float x = building.x - building.width * 0.5f +
                            (column + 0.5f) * building.width / columns;
            for (int floor = 0; floor < floors; ++floor)
            {
                const float y = 0.55f + floor * (building.height - 0.75f) / floors;
                const bool warm = ((column * 7 + floor * 3 + building.style) % 11) == 0;
                DrawCube({x, y, frontZ}, building.width / columns * 0.54f,
                         std::min(0.52f, building.height / floors * 0.48f), 0.035f,
                         warm ? Color{237, 184, 103, 4} : Color{29, 43, 50, 4});
                DrawCube({x, y + 0.30f, frontZ + 0.006f},
                         building.width / columns * 0.68f, 0.055f, 0.045f,
                         accent);
            }
        }
        for (int column = 0; column <= columns; ++column)
        {
            const float x = building.x - building.width * 0.5f +
                            column * building.width / columns;
            DrawCube({x, baseY, frontZ + 0.025f}, 0.095f, building.height,
                     0.08f, accent);
        }

        // Rooftop utility silhouette: lift housing, vents and an antenna.
        if (building.style == 2)
        {
            DrawCube({building.x - building.width * 0.18f, building.height + 0.42f, building.z},
                     building.width * 0.28f, 0.65f, building.depth * 0.35f, dark);
            DrawCylinder({building.x + building.width * 0.18f, building.height + 0.36f, building.z},
                         0.24f, 0.19f, 0.52f, 12, accent);
        }
        else
        {
            DrawCube({building.x, building.height + 0.32f, building.z},
                     building.width * 0.32f, 0.45f, building.depth * 0.36f, dark);
        }
        DrawCylinderEx({building.x, building.height + 0.18f, building.z},
                       {building.x, building.height + 1.35f, building.z},
                       0.025f, 0.012f, 8, Color{62, 70, 72, 2});
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

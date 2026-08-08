#ifndef TANKS3D_TANK_ASSETS_H
#define TANKS3D_TANK_ASSETS_H

#include <raylib.h>
#include <rlgl.h>

#include "core/nation.h"
#include "wwii_tank_model.h"

#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// Stable renderer-facing facade. Normal play retains the original code-native
// WWII roster. An explicit QA flag can route draw requests through one GLB so
// the importer, animation, lighting and shadow pipeline can be proved without
// changing collision, muzzle positions, vehicle progression or release art.
class TankAssets
{
public:
    TankAssets() = default;
    TankAssets(const TankAssets &) = delete;
    TankAssets &operator=(const TankAssets &) = delete;

    struct DrawRequest
    {
        float x = 0.0f;
        float z = 0.0f;
        float yaw = 0.0f;
        Color bodyColor = WHITE;
        float shield = 0.0f;
        int identity = 0;
        bool moving = false;
    };

    void configureGltfProbe(std::filesystem::path requestedPath = {})
    {
        probeRequested_ = true;
        requestedProbePath_ = std::move(requestedPath);
    }

    static std::filesystem::path defaultProbeModelPath(
        const std::filesystem::path &resourceRoot)
    {
        const std::vector<std::filesystem::path> candidates{
            resourceRoot / "models" / "tank_basic.glb",
            resourceRoot.parent_path() / "3d" / "assets" / "models" /
                "tank_basic.glb",
            resourceRoot.parent_path() / "assets" / "models" /
                "tank_basic.glb"};
        std::error_code error;
        for (const std::filesystem::path &candidate : candidates)
        {
            if (std::filesystem::is_regular_file(candidate, error))
                return candidate;
            error.clear();
        }
        return {};
    }

    void load(const std::filesystem::path &resourceRoot, Shader visibleShader,
              Shader depthShader)
    {
        visibleShader_ = visibleShader;
        depthShader_ = depthShader;
        TraceLog(LOG_INFO,
                 "TANKS3D: WWII set ready: USA, USSR and Germany player progressions; four classic enemy types");
        if (!probeRequested_)
            return;

        probePath_ = requestedProbePath_.empty()
                         ? defaultProbeModelPath(resourceRoot)
                         : requestedProbePath_;
        std::error_code error;
        if (probePath_.empty() ||
            !std::filesystem::is_regular_file(probePath_, error))
        {
            TraceLog(LOG_WARNING,
                     "TANKS3D: GLB QA model is missing; using code-native tanks");
            return;
        }

        probeModel_ = LoadModel(probePath_.string().c_str());
        // IsModelValid() currently assumes GPU skinning buffers at fixed VBO
        // slots. The macOS raylib build uses CPU skinning, so an otherwise
        // usable animated GLB is reported invalid after all meshes upload.
        // Validate only the data this isolated renderer actually consumes.
        if (!isProbeModelUsable(probeModel_))
        {
            TraceLog(LOG_WARNING,
                     "TANKS3D: GLB QA model failed to load; using code-native tanks");
            if (hasAllocatedProbeModel(probeModel_))
                UnloadModel(probeModel_);
            probeModel_ = {};
            return;
        }

        probeAnimations_ = LoadModelAnimations(probePath_.string().c_str(),
                                                &probeAnimationCount_);
        forwardAnimation_ = -1;
        for (int index = 0; index < probeAnimationCount_; ++index)
        {
            const std::string name = probeAnimations_[index].name;
            if (name.find("Forward") != std::string::npos)
            {
                forwardAnimation_ = index;
                break;
            }
        }
        if (forwardAnimation_ < 0 && probeAnimationCount_ > 0)
            forwardAnimation_ = 0;
        probeAnimationCompatible_ =
            forwardAnimation_ >= 0 && probeAnimations_ != nullptr &&
            IsModelAnimationValid(probeModel_,
                                  probeAnimations_[forwardAnimation_]);

        for (int index = 0; index < probeModel_.materialCount; ++index)
            probeModel_.materials[index].shader = visibleShader_;

        modelSpaceLocation_ = GetShaderLocation(visibleShader_,
                                                "modelSpaceInput");
        materialTagLocation_ = GetShaderLocation(visibleShader_,
                                                 "materialTagOverride");
        probeBounds_ = GetModelBoundingBox(probeModel_);
        probeTriangleCount_ = 0;
        for (int index = 0; index < probeModel_.meshCount; ++index)
            probeTriangleCount_ += probeModel_.meshes[index].triangleCount;
        probeLoaded_ = true;

        TraceLog(LOG_INFO,
                 "TANKS3D: isolated GLB QA ready: %s (%i meshes, %i materials, %i triangles, %i animations; forward animation %s)",
                 probePath_.filename().string().c_str(), probeModel_.meshCount,
                 probeModel_.materialCount, probeTriangleCount_,
                 probeAnimationCount_,
                 probeAnimationCompatible_ ? "compatible" : "import-only");
    }

    void unload()
    {
        visibleQueue_.clear();
        shadowQueue_.clear();
        if (probeAnimations_ != nullptr)
            UnloadModelAnimations(probeAnimations_, probeAnimationCount_);
        probeAnimations_ = nullptr;
        probeAnimationCount_ = 0;
        forwardAnimation_ = -1;
        probeAnimationCompatible_ = false;
        if (hasAllocatedProbeModel(probeModel_))
            UnloadModel(probeModel_);
        probeModel_ = {};
        probeLoaded_ = false;
    }

    void setAnimationClock(double seconds)
    {
        animationClock_ = seconds;
    }

    bool gltfProbeRequested() const { return probeRequested_; }
    bool gltfProbeEnabled() const { return probeRequested_ && probeLoaded_; }

    std::string gltfProbeStatus() const
    {
        if (!probeRequested_)
            return {};
        if (!probeLoaded_)
            return "GLB QA UNAVAILABLE - CODE-NATIVE FALLBACK ACTIVE";
        std::ostringstream status;
        status << "GLB QA  " << probePath_.filename().string() << "   "
               << probeModel_.meshCount << " MESHES   "
               << probeModel_.materialCount << " MATERIALS   "
               << probeTriangleCount_ << " TRIANGLES   "
               << probeAnimationCount_ << " ANIMATIONS   "
               << (probeAnimationCompatible_ ? "FORWARD READY"
                                             : "SKIN IMPORT-ONLY");
        return status.str();
    }

    void draw(float x, float z, float yaw, Color bodyColor, bool enemy,
              int armor, float shield, int identity, bool moving,
              tanks3d::core::Nation nation =
                  tanks3d::core::Nation::UnitedStates,
              bool shadowPass = false)
    {
        if (gltfProbeEnabled())
        {
            DrawRequest request{x, z, yaw, bodyColor, shield, identity,
                                moving};
            (shadowPass ? shadowQueue_ : visibleQueue_).push_back(request);
            return;
        }
        wwii_tank_model::DrawTank(x, z, yaw, bodyColor, enemy,
                                  armor, shield, identity, moving, nation);
    }

    void flushQueued(bool shadowPass)
    {
        std::vector<DrawRequest> &queue = shadowPass ? shadowQueue_
                                                     : visibleQueue_;
        if (!gltfProbeEnabled() || queue.empty())
        {
            queue.clear();
            return;
        }

        rlDrawRenderBatchActive();
        const Shader passShader = shadowPass ? depthShader_ : visibleShader_;
        for (int index = 0; index < probeModel_.materialCount; ++index)
            probeModel_.materials[index].shader = passShader;

        if (!shadowPass)
        {
            const int modelSpace = 1;
            const int paintedArmorTag = 9;
            SetShaderValue(visibleShader_, modelSpaceLocation_, &modelSpace,
                           SHADER_UNIFORM_INT);
            SetShaderValue(visibleShader_, materialTagLocation_,
                           &paintedArmorTag, SHADER_UNIFORM_INT);
        }

        for (const DrawRequest &request : queue)
        {
            updateProbePose(request.moving, request.identity);
            DrawModelEx(probeModel_, {request.x, kProbeGroundOffset, request.z},
                        {0.0f, 1.0f, 0.0f},
                        -request.yaw * RAD2DEG + kProbeForwardCorrectionDegrees,
                        kProbeScale, WHITE);
        }

        if (!shadowPass)
        {
            const int immediateSpace = 0;
            const int vertexMaterialTag = -1;
            SetShaderValue(visibleShader_, modelSpaceLocation_, &immediateSpace,
                           SHADER_UNIFORM_INT);
            SetShaderValue(visibleShader_, materialTagLocation_,
                           &vertexMaterialTag, SHADER_UNIFORM_INT);
        }
        rlSetShader(passShader.id, passShader.locs);
        queue.clear();
    }

private:
    static bool hasAllocatedProbeModel(const Model &model)
    {
        return model.meshes != nullptr || model.materials != nullptr ||
               model.meshMaterial != nullptr;
    }

    static bool isProbeModelUsable(const Model &model)
    {
        if (model.meshCount <= 0 || model.meshes == nullptr ||
            model.materialCount <= 0 || model.materials == nullptr ||
            model.meshMaterial == nullptr)
            return false;

        for (int index = 0; index < model.meshCount; ++index)
        {
            const Mesh &mesh = model.meshes[index];
            const bool uploaded = mesh.vaoId != 0 ||
                                  (mesh.vboId != nullptr && mesh.vboId[0] != 0);
            if (mesh.vertexCount <= 0 || mesh.vertices == nullptr || !uploaded)
                return false;
        }
        return true;
    }

    void updateProbePose(bool moving, int identity)
    {
        if (!probeAnimationCompatible_ || forwardAnimation_ < 0 ||
            probeAnimations_ == nullptr)
            return;
        const ModelAnimation &animation = probeAnimations_[forwardAnimation_];
        if (animation.keyframeCount <= 0)
            return;
        float frame = 0.0f;
        if (moving)
        {
            const double phase = animationClock_ * 24.0 +
                                 static_cast<double>(identity) * 2.75;
            frame = static_cast<float>(std::fmod(
                phase, static_cast<double>(animation.keyframeCount)));
            if (frame < 0.0f)
                frame += static_cast<float>(animation.keyframeCount);
        }
        UpdateModelAnimation(probeModel_, animation, frame);
    }

    static constexpr Vector3 kProbeScale{0.09f, 0.18f, 0.14f};
    static constexpr float kProbeGroundOffset = 0.003f;
    static constexpr float kProbeForwardCorrectionDegrees = -90.0f;

    Shader visibleShader_{};
    Shader depthShader_{};
    Model probeModel_{};
    ModelAnimation *probeAnimations_ = nullptr;
    BoundingBox probeBounds_{};
    std::filesystem::path requestedProbePath_;
    std::filesystem::path probePath_;
    std::vector<DrawRequest> visibleQueue_;
    std::vector<DrawRequest> shadowQueue_;
    double animationClock_ = 0.0;
    int probeAnimationCount_ = 0;
    int forwardAnimation_ = -1;
    int probeTriangleCount_ = 0;
    int modelSpaceLocation_ = -1;
    int materialTagLocation_ = -1;
    bool probeRequested_ = false;
    bool probeLoaded_ = false;
    bool probeAnimationCompatible_ = false;
};

#endif // TANKS3D_TANK_ASSETS_H

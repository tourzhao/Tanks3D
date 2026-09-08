#ifndef TANKS3D_POST_PROCESS_H
#define TANKS3D_POST_PROCESS_H

#include <raylib.h>

// Lightweight filmic post-processing for an HDR player view.
// Draw the HUD after draw() so text and minimap lines remain crisp.
class PostProcess
{
public:
    PostProcess() = default;
    PostProcess(const PostProcess &) = delete;
    PostProcess &operator=(const PostProcess &) = delete;

    bool load()
    {
        unload();
        shader_ = LoadShaderFromMemory(nullptr, fragmentShaderSource());
        if (!IsShaderValid(shader_))
            return false;

        texelSizeLocation_ = GetShaderLocation(shader_, "texelSize");
        timeLocation_ = GetShaderLocation(shader_, "time");
        valid_ = texelSizeLocation_ >= 0 && timeLocation_ >= 0;
        if (!valid_)
            unload();
        return valid_;
    }

    void unload()
    {
        unloadResolvedTarget();
        if (IsShaderValid(shader_))
            UnloadShader(shader_);
        shader_ = {};
        texelSizeLocation_ = -1;
        timeLocation_ = -1;
        valid_ = false;
    }

    bool valid() const
    {
        return valid_ && IsShaderValid(shader_);
    }

    // Must be called inside BeginDrawing()/EndDrawing(). The source height is
    // flipped here because OpenGL render textures use a bottom-left origin.
    void draw(const RenderTexture2D &source, Rectangle destination, float time = 0.0f)
    {
        if (!IsTextureValid(source.texture))
            return;

        const Rectangle sourceRectangle{
            0.0f,
            0.0f,
            static_cast<float>(source.texture.width),
            -static_cast<float>(source.texture.height)};

        if (!valid() ||
            !ensureResolvedTarget(source.texture.width,
                                  source.texture.height))
        {
            DrawTexturePro(source.texture, sourceRectangle, destination,
                           {0.0f, 0.0f}, 0.0f, WHITE);
            return;
        }

        const Vector2 texelSize{
            1.0f / static_cast<float>(source.texture.width),
            1.0f / static_cast<float>(source.texture.height)};

        // The source view is logical-window resolution while a Retina back
        // buffer has four times as many fragments. Run the 13-tap bloom and
        // tone map once per source pixel, then perform one inexpensive
        // bilinear upscale. HUD rendering still follows at native Retina
        // resolution in the caller.
        BeginTextureMode(resolvedTarget_);
        ClearBackground(BLACK);
        BeginShaderMode(shader_);
        SetShaderValue(shader_, texelSizeLocation_, &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_, timeLocation_, &time, SHADER_UNIFORM_FLOAT);
        const Rectangle resolvedDestination{
            0.0f, 0.0f,
            static_cast<float>(resolvedTarget_.texture.width),
            static_cast<float>(resolvedTarget_.texture.height)};
        DrawTexturePro(source.texture, sourceRectangle, resolvedDestination,
                       {0.0f, 0.0f}, 0.0f, WHITE);
        EndShaderMode();
        EndTextureMode();

        const Rectangle resolvedSource{
            0.0f, 0.0f,
            static_cast<float>(resolvedTarget_.texture.width),
            -static_cast<float>(resolvedTarget_.texture.height)};
        DrawTexturePro(resolvedTarget_.texture, resolvedSource, destination,
                       {0.0f, 0.0f}, 0.0f, WHITE);
    }

private:
    bool ensureResolvedTarget(int width, int height)
    {
        if (IsRenderTextureValid(resolvedTarget_) &&
            resolvedTarget_.texture.width == width &&
            resolvedTarget_.texture.height == height)
        {
            return true;
        }

        unloadResolvedTarget();
        resolvedTarget_ = LoadRenderTexture(width, height);
        if (!IsRenderTextureValid(resolvedTarget_))
        {
            resolvedTarget_ = {};
            return false;
        }
        SetTextureFilter(resolvedTarget_.texture, TEXTURE_FILTER_BILINEAR);
        return true;
    }

    void unloadResolvedTarget()
    {
        if (IsRenderTextureValid(resolvedTarget_))
            UnloadRenderTexture(resolvedTarget_);
        resolvedTarget_ = {};
    }

    static const char *fragmentShaderSource()
    {
        return R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 texelSize;
uniform float time;

out vec4 finalColor;

vec3 approximateLinear(vec3 color)
{
    return color*color;
}

vec3 softThreshold(vec3 color)
{
    // Only genuinely bright specular and emissive values enter bloom. Keeping
    // diffuse surfaces below the knee avoids the flat, hazy "mobile game" look.
    const float threshold = 0.78;
    const float knee = 0.22;
    float brightness = max(max(color.r, color.g), color.b);
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0*knee);
    soft = soft*soft/(4.0*knee + 0.00001);
    float contribution = max(brightness - threshold, soft)/max(brightness, 0.00001);
    return color*contribution;
}

vec3 glowAt(vec2 offset)
{
    vec3 sampleColor = texture(texture0, fragTexCoord + offset*texelSize).rgb;
    return softThreshold(approximateLinear(sampleColor));
}

vec3 acesApprox(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color*(a*color + b))/(color*(c*color + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 source = texture(texture0, fragTexCoord);
    vec3 sourceLinear = approximateLinear(source.rgb);

    // Thirteen taps: center, a compact 8-sample ring and a wider 4-sample
    // ring. Constant weights keep the loop-free shader predictable on macOS.
    vec3 glow = glowAt(vec2(0.0))*0.18;
    glow += glowAt(vec2( 1.45,  0.00))*0.11;
    glow += glowAt(vec2(-1.45,  0.00))*0.11;
    glow += glowAt(vec2( 0.00,  1.45))*0.11;
    glow += glowAt(vec2( 0.00, -1.45))*0.11;
    glow += glowAt(vec2( 1.35,  1.35))*0.065;
    glow += glowAt(vec2(-1.35,  1.35))*0.065;
    glow += glowAt(vec2( 1.35, -1.35))*0.065;
    glow += glowAt(vec2(-1.35, -1.35))*0.065;
    glow += glowAt(vec2( 3.80,  0.00))*0.03;
    glow += glowAt(vec2(-3.80,  0.00))*0.03;
    glow += glowAt(vec2( 0.00,  3.80))*0.03;
    glow += glowAt(vec2( 0.00, -3.80))*0.03;

    float glowPulse = 1.0 + 0.012*sin(time*0.70);
    vec3 exposed = (sourceLinear + glow*0.24*glowPulse)*1.06;
    vec3 mapped = sqrt(max(acesApprox(exposed), vec3(0.0)));
    vec3 color = mapped;

    // Cool shadows and warm highlights, both intentionally subtle.
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 cool = vec3(0.985, 1.002, 1.025);
    vec3 warm = vec3(1.020, 1.004, 0.982);
    color *= mix(cool, warm, smoothstep(0.24, 0.80, luminance));

    vec2 centered = fragTexCoord*2.0 - 1.0;
    float vignette = 1.0 - 0.10*smoothstep(0.30, 1.55, dot(centered, centered));
    color *= vignette;

    finalColor = vec4(clamp(color, 0.0, 1.0), source.a)*colDiffuse*fragColor;
}
)GLSL";
    }

    Shader shader_{};
    RenderTexture2D resolvedTarget_{};
    int texelSizeLocation_ = -1;
    int timeLocation_ = -1;
    bool valid_ = false;
};

#endif

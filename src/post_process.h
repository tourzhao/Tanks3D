#ifndef TANKS3D_POST_PROCESS_H
#define TANKS3D_POST_PROCESS_H

#include <raylib.h>

// Optional crisp pixel treatment and filmic color for an HDR player view.
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
        pixelStyleLocation_ = GetShaderLocation(shader_, "pixelStyle");
        valid_ = texelSizeLocation_ >= 0 && timeLocation_ >= 0 &&
                 pixelStyleLocation_ >= 0;
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
        pixelStyleLocation_ = -1;
        valid_ = false;
    }

    void setPixelStyleEnabled(bool enabled)
    {
        pixelStyleEnabled_ = enabled;
    }

    bool pixelStyleEnabled() const
    {
        return pixelStyleEnabled_;
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
        // tone map at logical resolution. Pixel style optionally groups two
        // source pixels; nearest presentation keeps both modes sharp. The
        // caller draws the HUD afterward at native resolution.
        BeginTextureMode(resolvedTarget_);
        ClearBackground(BLACK);
        BeginShaderMode(shader_);
        SetShaderValue(shader_, texelSizeLocation_, &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_, timeLocation_, &time, SHADER_UNIFORM_FLOAT);
        const int pixelStyle = pixelStyleEnabled_ ? 1 : 0;
        SetShaderValue(shader_, pixelStyleLocation_, &pixelStyle,
                       SHADER_UNIFORM_INT);
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
        SetTextureFilter(resolvedTarget_.texture, TEXTURE_FILTER_POINT);
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
uniform int pixelStyle;

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

ivec2 sceneTexel()
{
    ivec2 size = textureSize(texture0, 0);
    ivec2 pixel = ivec2(floor(fragTexCoord*vec2(size)));
    if (pixelStyle != 0)
        pixel = (pixel/2)*2 + ivec2(1);
    // A normalized two-pixel block center lies between four source texels.
    // Fetch one explicit texel so bilinear source filtering cannot average
    // their colors. Clamp the last block for odd-sized render targets.
    return clamp(pixel, ivec2(0), size - ivec2(1));
}

vec3 glowAt(vec2 offset)
{
    // Light can spread smoothly around crisp geometry, independently of its
    // pixel grid. This is the only pass that mixes neighboring source colors.
    vec2 tap = clamp(fragTexCoord + offset*texelSize,
                     texelSize*0.5, vec2(1.0) - texelSize*0.5);
    vec3 sampleColor = texture(texture0, tap).rgb;
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
    vec4 source = texelFetch(texture0, sceneTexel(), 0);
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
    float glowStrength = pixelStyle != 0 ? 0.16 : 0.20;
    vec3 exposed = (sourceLinear + glow*glowStrength*glowPulse)*1.06;
    vec3 mapped = sqrt(max(acesApprox(exposed), vec3(0.0)));
    vec3 color = mapped;

    // Cool shadows and warm highlights, both intentionally subtle.
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 cool = vec3(0.985, 1.002, 1.025);
    vec3 warm = vec3(1.020, 1.004, 0.982);
    color *= mix(cool, warm, smoothstep(0.24, 0.80, luminance));

    // Very light palette stepping belongs before the lens falloff: quantizing
    // that smooth falloff would turn a plain ground plane into concentric bands.
    if (pixelStyle != 0)
    {
        vec3 stepped = floor(clamp(color, 0.0, 1.0)*63.0 + 0.5)/63.0;
        color = mix(color, stepped, 0.18);
    }

    vec2 centered = fragTexCoord*2.0 - 1.0;
    float vignette = 1.0 - 0.10*smoothstep(0.30, 1.55, dot(centered, centered));
    color *= vignette;

    // This target is a complete opaque world. Its foliage, smoke and overlays
    // have already blended into RGB; reusing their accumulated framebuffer
    // alpha here would composite them twice and darken the scene.
    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0)*colDiffuse*fragColor;
}
)GLSL";
    }

    Shader shader_{};
    RenderTexture2D resolvedTarget_{};
    int texelSizeLocation_ = -1;
    int timeLocation_ = -1;
    int pixelStyleLocation_ = -1;
    bool pixelStyleEnabled_ = false;
    bool valid_ = false;
};

#endif

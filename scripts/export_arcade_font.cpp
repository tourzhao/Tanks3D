// Reproduce resources/fonts/arcade.{png,fnt} from raylib's built-in font.
// Source: raylib 6.0 GetFontDefault(), by Ramon Santamaria (raysan5), zlib.
// The exported atlas retains raylib's license; it is not a new project font.
// Build on macOS:
// c++ -std=c++17 scripts/export_arcade_font.cpp -I/opt/homebrew/opt/raylib/include
//     -L/opt/homebrew/opt/raylib/lib -lraylib -o build/export_arcade_font
// Run from the repository root: build/export_arcade_font [output-directory]

#include <raylib.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
struct FontContext
{
    Image atlas{};

    ~FontContext()
    {
        if (atlas.data != nullptr)
            UnloadImage(atlas);
        if (IsWindowReady())
            CloseWindow();
    }
};

int integerMetric(float value)
{
    if (!std::isfinite(value) || value != std::floor(value))
        throw std::runtime_error("The built-in bitmap font has a non-integer metric");
    return static_cast<int>(value);
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc > 2)
            throw std::runtime_error("Usage: export_arcade_font [output-directory]");
        const std::filesystem::path output = argc == 2 ? argv[1] : "resources/fonts";
        std::filesystem::create_directories(output);

        FontContext context;
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(10, 10, "Tanks3D built-in font export");
        if (!IsWindowReady())
            throw std::runtime_error("Unable to initialize the raylib font context");
        const Font font = GetFontDefault();
        if (font.baseSize != 10 || font.glyphCount <= 0 || font.recs == nullptr ||
            font.glyphs == nullptr)
            throw std::runtime_error("Unexpected raylib default font; expected base size 10");
        context.atlas = LoadImageFromTexture(font.texture);
        if (context.atlas.data == nullptr)
            throw std::runtime_error("Unable to read the original font atlas");
        // Godot's BMFont loader accepts RGBA8, not raylib's grayscale-alpha
        // storage. Expand channels without changing any glyph/color/alpha pixel.
        ImageFormat(&context.atlas, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (!ExportImage(context.atlas, (output / "arcade.png").string().c_str()))
            throw std::runtime_error("Unable to export the original font atlas");

        std::ofstream metrics(output / "arcade.fnt", std::ios::binary);
        if (!metrics)
            throw std::runtime_error("Unable to write bitmap font metrics");
        // DrawText uses one base-size pixel of character spacing. The default
        // glyphs use rectangle widths (advanceX=0); preserve explicit advances
        // if raylib supplies any. Do not trim, redraw, or resample the atlas.
        constexpr int kDefaultSpacing = 1;
        metrics << "info face=\"raylib Default\" size=" << font.baseSize
                << " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=0 aa=1"
                   " padding=0,0,0,0 spacing=1,1 outline=0\n"
                << "common lineHeight=" << font.baseSize << " base=" << font.baseSize
                << " scaleW=" << context.atlas.width << " scaleH=" << context.atlas.height
                << " pages=1 packed=0 alphaChnl=0 redChnl=4 greenChnl=4 blueChnl=4\n"
                << "page id=0 file=\"arcade.png\"\n"
                << "chars count=" << font.glyphCount << '\n';
        for (int index = 0; index < font.glyphCount; ++index)
        {
            const GlyphInfo &glyph = font.glyphs[index];
            const Rectangle &rectangle = font.recs[index];
            const int width = integerMetric(rectangle.width);
            const int advance = glyph.advanceX != 0 ? glyph.advanceX
                                                    : width + kDefaultSpacing;
            metrics << "char id=" << glyph.value
                    << " x=" << integerMetric(rectangle.x)
                    << " y=" << integerMetric(rectangle.y)
                    << " width=" << width << " height=" << integerMetric(rectangle.height)
                    << " xoffset=" << glyph.offsetX << " yoffset=" << glyph.offsetY
                    << " xadvance=" << advance << " page=0 chnl=15\n";
        }
        metrics << "kernings count=0\n";
        metrics.close();
        if (!metrics)
            throw std::runtime_error("Unable to finish bitmap font metrics");
        std::cout << "ARCADE_FONT_EXPORTED raylib=" << RAYLIB_VERSION
                  << " base=" << font.baseSize << " glyphs=" << font.glyphCount
                  << " atlas=" << context.atlas.width << 'x' << context.atlas.height
                  << " output=" << output << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

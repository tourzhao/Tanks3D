#ifndef TANKS3D_APP_RELEASE_SCREENSHOT_OPTIONS_H
#define TANKS3D_APP_RELEASE_SCREENSHOT_OPTIONS_H

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace tanks3d::app
{
inline constexpr int kDefaultReleaseScreenshotFrame = 240;
inline constexpr int kMaximumReleaseScreenshotFrame = 3600;
inline constexpr int kReleaseScreenshotWidth = 1280;
inline constexpr int kReleaseScreenshotHeight = 720;

struct ReleaseScreenshotOptions
{
    std::string outputPath;
    int frame = kDefaultReleaseScreenshotFrame;

    bool requested() const
    {
        return !outputPath.empty();
    }

    bool due(int renderedGameFrames) const
    {
        return requested() && renderedGameFrames >= frame;
    }
};

struct ReleaseScreenshotParseResult
{
    ReleaseScreenshotOptions options;
    std::string error;

    bool valid() const
    {
        return error.empty();
    }
};

inline bool hasPngExtension(const std::string &path)
{
    return path.size() >= 4U &&
           path.compare(path.size() - 4U, 4U, ".png") == 0;
}

inline bool parseReleaseScreenshotFrame(const std::string &text, int &frame)
{
    if (text.empty())
        return false;

    int parsed = 0;
    const char *const begin = text.data();
    const char *const end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed < 1 ||
        parsed > kMaximumReleaseScreenshotFrame)
    {
        return false;
    }
    frame = parsed;
    return true;
}

inline ReleaseScreenshotParseResult parseReleaseScreenshotOptions(
    const std::vector<std::string> &arguments)
{
    constexpr const char *kOutputPrefix = "--release-screenshot=";
    constexpr const char *kFramePrefix = "--release-screenshot-frame=";
    const std::size_t outputPrefixSize =
        std::char_traits<char>::length(kOutputPrefix);
    const std::size_t framePrefixSize =
        std::char_traits<char>::length(kFramePrefix);

    ReleaseScreenshotParseResult result;
    bool outputSeen = false;
    bool frameSeen = false;
    for (const std::string &argument : arguments)
    {
        if (argument == "--release-screenshot")
        {
            result.error = "--release-screenshot requires =PATH.png";
            return result;
        }
        if (argument == "--release-screenshot-frame")
        {
            result.error =
                "--release-screenshot-frame requires =FRAME";
            return result;
        }
        if (argument.rfind(kOutputPrefix, 0) == 0)
        {
            if (outputSeen)
            {
                result.error = "--release-screenshot was provided twice";
                return result;
            }
            outputSeen = true;
            result.options.outputPath = argument.substr(outputPrefixSize);
            if (result.options.outputPath.empty())
            {
                result.error = "release screenshot path is empty";
                return result;
            }
            if (result.options.outputPath.find_first_of("\r\n") !=
                std::string::npos)
            {
                result.error =
                    "release screenshot path contains a newline";
                return result;
            }
            if (!hasPngExtension(result.options.outputPath))
            {
                result.error = "release screenshot path must end in .png";
                return result;
            }
            continue;
        }
        if (argument.rfind(kFramePrefix, 0) == 0)
        {
            if (frameSeen)
            {
                result.error =
                    "--release-screenshot-frame was provided twice";
                return result;
            }
            frameSeen = true;
            if (!parseReleaseScreenshotFrame(
                    argument.substr(framePrefixSize), result.options.frame))
            {
                result.error =
                    "release screenshot frame must be an integer from 1 to " +
                    std::to_string(kMaximumReleaseScreenshotFrame);
                return result;
            }
        }
    }

    if (frameSeen && !outputSeen)
    {
        result.error =
            "--release-screenshot-frame requires --release-screenshot";
    }
    return result;
}
} // namespace tanks3d::app

#endif

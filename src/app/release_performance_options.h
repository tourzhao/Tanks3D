#ifndef TANKS3D_APP_RELEASE_PERFORMANCE_OPTIONS_H
#define TANKS3D_APP_RELEASE_PERFORMANCE_OPTIONS_H

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace tanks3d::app
{
inline constexpr const char *kReleasePerformanceQuickStartArgument =
    "--quick-start";
inline constexpr const char *kReleasePerformanceLogArgumentPrefix =
    "--release-performance-log=";
inline constexpr const char *kReleasePerformanceCandidateArgumentPrefix =
    "--release-candidate-sha256=";
inline constexpr const char *kReleasePerformanceNonceArgumentPrefix =
    "--release-session-nonce=";
inline constexpr const char *kReleasePerformanceDurationArgumentPrefix =
    "--release-performance-duration-seconds=";
inline constexpr int kDefaultReleasePerformanceDurationSeconds = 1801;
inline constexpr int kMaximumReleasePerformanceDurationSeconds = 14400;
inline constexpr std::int64_t kReleasePerformanceIntervalMicroseconds =
    1000000;

struct ReleasePerformanceOptions
{
    std::string outputPath;
    std::string candidateSha256;
    std::string sessionNonce;
    int durationSeconds = kDefaultReleasePerformanceDurationSeconds;

    bool requested() const
    {
        return !outputPath.empty();
    }

    std::int64_t targetDurationMicroseconds() const
    {
        return static_cast<std::int64_t>(durationSeconds) * 1000000;
    }
};

struct ReleasePerformanceParseResult
{
    ReleasePerformanceOptions options;
    std::string error;

    bool valid() const
    {
        return error.empty();
    }
};

inline bool hasLowercaseHexLength(const std::string &text,
                                  std::size_t firstLength,
                                  std::size_t secondLength = 0U)
{
    if (text.size() != firstLength &&
        (secondLength == 0U || text.size() != secondLength))
    {
        return false;
    }
    for (const char character : text)
    {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f')))
        {
            return false;
        }
    }
    return true;
}

inline bool hasJsonExtension(const std::string &path)
{
    return path.size() >= 5U &&
           path.compare(path.size() - 5U, 5U, ".json") == 0;
}

inline bool parseReleasePerformanceDuration(const std::string &text,
                                            int &durationSeconds)
{
    if (text.empty())
        return false;

    int parsed = 0;
    const char *const begin = text.data();
    const char *const end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed < 1 ||
        parsed > kMaximumReleasePerformanceDurationSeconds)
    {
        return false;
    }
    durationSeconds = parsed;
    return true;
}

inline ReleasePerformanceParseResult parseReleasePerformanceOptions(
    const std::vector<std::string> &arguments)
{
    const std::size_t outputPrefixSize =
        std::char_traits<char>::length(kReleasePerformanceLogArgumentPrefix);
    const std::size_t candidatePrefixSize =
        std::char_traits<char>::length(
            kReleasePerformanceCandidateArgumentPrefix);
    const std::size_t noncePrefixSize =
        std::char_traits<char>::length(kReleasePerformanceNonceArgumentPrefix);
    const std::size_t durationPrefixSize =
        std::char_traits<char>::length(
            kReleasePerformanceDurationArgumentPrefix);

    ReleasePerformanceParseResult result;
    bool outputSeen = false;
    bool candidateSeen = false;
    bool nonceSeen = false;
    bool durationSeen = false;
    bool screenshotOptionSeen = false;
    int quickStartCount = 0;
    std::string incompatibleArgument;

    for (const std::string &argument : arguments)
    {
        const bool performanceArgument =
            argument == "--release-performance-log" ||
            argument == "--release-candidate-sha256" ||
            argument == "--release-session-nonce" ||
            argument == "--release-performance-duration-seconds" ||
            argument.rfind(kReleasePerformanceLogArgumentPrefix, 0) == 0 ||
            argument.rfind(kReleasePerformanceCandidateArgumentPrefix, 0) ==
                0 ||
            argument.rfind(kReleasePerformanceNonceArgumentPrefix, 0) == 0 ||
            argument.rfind(kReleasePerformanceDurationArgumentPrefix, 0) == 0;
        if (argument == kReleasePerformanceQuickStartArgument)
            ++quickStartCount;
        else if (!performanceArgument && incompatibleArgument.empty())
            incompatibleArgument = argument;

        if (argument == "--release-screenshot" ||
            argument == "--release-screenshot-frame" ||
            argument.rfind("--release-screenshot=", 0) == 0 ||
            argument.rfind("--release-screenshot-frame=", 0) == 0)
        {
            screenshotOptionSeen = true;
        }

        if (argument == "--release-performance-log")
        {
            result.error = "--release-performance-log requires =PATH.json";
            return result;
        }
        if (argument == "--release-candidate-sha256")
        {
            result.error = "--release-candidate-sha256 requires =SHA256";
            return result;
        }
        if (argument == "--release-session-nonce")
        {
            result.error = "--release-session-nonce requires =NONCE";
            return result;
        }
        if (argument == "--release-performance-duration-seconds")
        {
            result.error =
                "--release-performance-duration-seconds requires =SECONDS";
            return result;
        }

        if (argument.rfind(kReleasePerformanceLogArgumentPrefix, 0) == 0)
        {
            if (outputSeen)
            {
                result.error =
                    "--release-performance-log was provided twice";
                return result;
            }
            outputSeen = true;
            result.options.outputPath = argument.substr(outputPrefixSize);
            if (result.options.outputPath.empty())
            {
                result.error = "release performance log path is empty";
                return result;
            }
            if (result.options.outputPath.find('\0') != std::string::npos ||
                result.options.outputPath.find_first_of("\r\n") !=
                    std::string::npos)
            {
                result.error =
                    "release performance log path contains NUL or a newline";
                return result;
            }
            if (!hasJsonExtension(result.options.outputPath))
            {
                result.error =
                    "release performance log path must end in .json";
                return result;
            }
            continue;
        }

        if (argument.rfind(kReleasePerformanceCandidateArgumentPrefix, 0) ==
            0)
        {
            if (candidateSeen)
            {
                result.error =
                    "--release-candidate-sha256 was provided twice";
                return result;
            }
            candidateSeen = true;
            result.options.candidateSha256 =
                argument.substr(candidatePrefixSize);
            if (!hasLowercaseHexLength(result.options.candidateSha256, 64U))
            {
                result.error =
                    "release candidate SHA-256 must be 64 lowercase hex "
                    "characters";
                return result;
            }
            continue;
        }

        if (argument.rfind(kReleasePerformanceNonceArgumentPrefix, 0) == 0)
        {
            if (nonceSeen)
            {
                result.error = "--release-session-nonce was provided twice";
                return result;
            }
            nonceSeen = true;
            result.options.sessionNonce = argument.substr(noncePrefixSize);
            if (!hasLowercaseHexLength(result.options.sessionNonce, 32U, 64U))
            {
                result.error =
                    "release session nonce must be 32 or 64 lowercase hex "
                    "characters";
                return result;
            }
            continue;
        }

        if (argument.rfind(kReleasePerformanceDurationArgumentPrefix, 0) ==
            0)
        {
            if (durationSeen)
            {
                result.error =
                    "--release-performance-duration-seconds was provided "
                    "twice";
                return result;
            }
            durationSeen = true;
            if (!parseReleasePerformanceDuration(
                    argument.substr(durationPrefixSize),
                    result.options.durationSeconds))
            {
                result.error =
                    "release performance duration must be an integer from 1 "
                    "to " +
                    std::to_string(kMaximumReleasePerformanceDurationSeconds);
                return result;
            }
        }
    }

    const bool anyPerformanceOption =
        outputSeen || candidateSeen || nonceSeen || durationSeen;
    if (!anyPerformanceOption)
        return result;
    if (!(outputSeen && candidateSeen && nonceSeen))
    {
        result.error =
            "release performance log, candidate SHA-256, and session nonce "
            "must be provided together";
        return result;
    }
    if (screenshotOptionSeen)
    {
        result.error =
            "release performance telemetry cannot be combined with release "
            "screenshot capture";
        return result;
    }
    if (quickStartCount != 1)
    {
        result.error =
            "release performance telemetry requires exactly one "
            "--quick-start mode";
        return result;
    }
    if (!incompatibleArgument.empty())
    {
        result.error =
            "release performance telemetry cannot be combined with " +
            incompatibleArgument;
    }
    return result;
}
} // namespace tanks3d::app

#endif

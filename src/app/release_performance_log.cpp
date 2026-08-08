#include "release_performance_log.h"

#include "atomic_output_file.h"

#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

#if defined(__APPLE__)
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace tanks3d::app
{
namespace
{
bool hasSafeSourceTag(const std::string &tag)
{
    if (tag.empty() || tag.size() > 128U ||
        !((tag.front() >= 'A' && tag.front() <= 'Z') ||
          (tag.front() >= 'a' && tag.front() <= 'z') ||
          (tag.front() >= '0' && tag.front() <= '9')))
    {
        return false;
    }
    for (const char character : tag)
    {
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '.' || character == '_' || character == '-'))
        {
            return false;
        }
    }
    return true;
}

bool hasSafeAppState(const std::string &state)
{
    return state == "gameplay" || state == "settlement" ||
           state == "high_score";
}

std::string jsonString(const std::string &text)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '"';
    for (const unsigned char character : text)
    {
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U)
            {
                output << "\\u00" << std::hex << std::setw(2)
                       << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec;
            }
            else
            {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    output << '"';
    return output.str();
}

std::optional<std::string> utcTimestamp(
    std::chrono::system_clock::time_point value)
{
    const auto wholeSeconds =
        std::chrono::time_point_cast<std::chrono::seconds>(value);
    const std::time_t raw =
        std::chrono::system_clock::to_time_t(wholeSeconds);
    std::tm utc{};
    if (::gmtime_r(&raw, &utc) == nullptr)
        return std::nullopt;
    char buffer[21]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) !=
        20U)
    {
        return std::nullopt;
    }
    return std::string{buffer};
}

bool hasSafeOutputPath(const std::string &path)
{
    return !path.empty() && hasJsonExtension(path) &&
           path.find('\0') == std::string::npos &&
           path.find_first_of("\r\n") == std::string::npos;
}
} // namespace

std::optional<std::uint64_t>
sampleCurrentProcessPhysicalFootprintBytes()
{
#if defined(__APPLE__)
    rusage_info_v0 usage{};
    if (::proc_pid_rusage(
            ::getpid(), RUSAGE_INFO_V0,
            reinterpret_cast<rusage_info_t *>(&usage)) != 0 ||
        usage.ri_phys_footprint == 0U)
    {
        return std::nullopt;
    }
    return usage.ri_phys_footprint;
#else
    return std::nullopt;
#endif
}

ReleasePerformanceRecorder::ReleasePerformanceRecorder(
    ReleasePerformanceOptions options, std::string sourceCommit,
    std::string sourceTag,
    std::chrono::system_clock::time_point startedAtUtc,
    ReleasePerformanceResidentBytesSampler residentBytesSampler)
    : options_(std::move(options)), sourceCommit_(std::move(sourceCommit)),
      sourceTag_(std::move(sourceTag)), startedAtUtc_(startedAtUtc),
      residentBytesSampler_(std::move(residentBytesSampler))
{
    if (!options_.requested())
    {
        error_ = "release performance telemetry is not requested";
        return;
    }
    if (!hasSafeOutputPath(options_.outputPath))
    {
        error_ = "release performance output path is invalid";
        return;
    }
    if (!hasLowercaseHexLength(options_.candidateSha256, 64U))
    {
        error_ = "release candidate SHA-256 is invalid";
        return;
    }
    if (!hasLowercaseHexLength(options_.sessionNonce, 32U, 64U))
    {
        error_ = "release session nonce is invalid";
        return;
    }
    if (options_.durationSeconds < 1 ||
        options_.durationSeconds >
            kMaximumReleasePerformanceDurationSeconds)
    {
        error_ = "release performance duration is invalid";
        return;
    }
    if (!hasLowercaseHexLength(sourceCommit_, 40U, 64U))
    {
        error_ = "release source commit is invalid";
        return;
    }
    if (!hasSafeSourceTag(sourceTag_))
    {
        error_ = "release source tag is invalid";
        return;
    }
    if (!residentBytesSampler_)
    {
        error_ = "release resident-memory sampler is unavailable";
        return;
    }
    const auto timestamp = utcTimestamp(startedAtUtc_);
    if (!timestamp.has_value())
    {
        error_ = "release performance start timestamp is invalid";
        return;
    }
    startedAtUtcText_ = *timestamp;
    configurationValid_ = true;
}

bool ReleasePerformanceRecorder::valid() const
{
    return configurationValid_ && error_.empty();
}

bool ReleasePerformanceRecorder::failed() const
{
    return !error_.empty();
}

bool ReleasePerformanceRecorder::finalized() const
{
    return finalized_;
}

bool ReleasePerformanceRecorder::saved() const
{
    return saved_;
}

bool ReleasePerformanceRecorder::targetDurationReached() const
{
    return targetDurationReached_;
}

const std::string &ReleasePerformanceRecorder::error() const
{
    return error_;
}

bool ReleasePerformanceRecorder::reject(std::string message)
{
    if (error_.empty())
        error_ = std::move(message);
    return false;
}

bool ReleasePerformanceRecorder::recordFrame(
    double actualDurationSeconds, int stageNumber, int playerCount,
    const std::string &appState, bool windowFocused,
    std::uint64_t completedStages)
{
    if (!valid())
        return false;
    if (finalized_)
        return reject("cannot record a frame after finalization");
    if (targetDurationReached_)
        return true;
    if (!std::isfinite(actualDurationSeconds) ||
        actualDurationSeconds <= 0.0)
    {
        return reject("frame duration must be finite and positive");
    }
    if (stageNumber < 1)
        return reject("frame stage number must be positive");
    if (playerCount != 1 && playerCount != 2)
        return reject("frame player count must be one or two");
    if (!hasSafeAppState(appState))
        return reject("frame app state is invalid");
    if (completedStages < completedStages_)
        return reject("frame completed-stage count moved backwards");
    const std::uint64_t stageClearEvents =
        completedStages - completedStages_;
    if (stageClearEvents > 1U)
        return reject("frame completed-stage count advanced by more than one");

    const double durationMicroseconds = actualDurationSeconds * 1000000.0;
    if (!std::isfinite(durationMicroseconds) ||
        durationMicroseconds >=
            static_cast<double>(std::numeric_limits<long long>::max()))
    {
        return reject("frame duration exceeds the telemetry range");
    }
    const long long roundedDuration = std::llround(durationMicroseconds);
    if (roundedDuration <= 0)
        return reject("frame duration rounds to zero microseconds");
    const std::uint64_t duration =
        static_cast<std::uint64_t>(roundedDuration);
    if (elapsedMicroseconds_ >
            std::numeric_limits<std::uint64_t>::max() - duration ||
        windowDurationMicroseconds_ >
            std::numeric_limits<std::uint64_t>::max() - duration ||
        (appState == "gameplay" &&
         windowGameplayDurationMicroseconds_ >
             std::numeric_limits<std::uint64_t>::max() - duration) ||
        (windowFocused &&
         windowFocusedDurationMicroseconds_ >
             std::numeric_limits<std::uint64_t>::max() - duration) ||
        (stageClearEvents != 0U &&
         windowStageClearEvents_ ==
             std::numeric_limits<std::uint64_t>::max()) ||
        windowRenderedFrames_ ==
            std::numeric_limits<std::uint64_t>::max())
    {
        return reject("release performance counters overflowed");
    }

    elapsedMicroseconds_ += duration;
    windowDurationMicroseconds_ += duration;
    if (appState == "gameplay")
        windowGameplayDurationMicroseconds_ += duration;
    if (windowFocused)
        windowFocusedDurationMicroseconds_ += duration;
    ++windowRenderedFrames_;
    windowStageClearEvents_ += stageClearEvents;
    completedStages_ = completedStages;

    bool sampleAppended = false;
    if (windowDurationMicroseconds_ >=
        static_cast<std::uint64_t>(
            kReleasePerformanceIntervalMicroseconds))
    {
        if (!appendSample(stageNumber, playerCount, appState, windowFocused))
            return false;
        sampleAppended = true;
    }
    if (elapsedMicroseconds_ >= static_cast<std::uint64_t>(
                                      options_.targetDurationMicroseconds()) &&
        sampleAppended)
    {
        targetDurationReached_ = true;
    }
    return true;
}

bool ReleasePerformanceRecorder::appendSample(
    int stageNumber, int playerCount, const std::string &appState,
    bool windowFocused)
{
    const std::optional<std::uint64_t> residentBytes =
        residentBytesSampler_();
    if (!residentBytes.has_value() || *residentBytes == 0U)
    {
        return reject("physical-footprint sampling failed");
    }
    if (samples_.size() ==
        static_cast<std::size_t>(
            std::numeric_limits<std::uint64_t>::max()))
    {
        return reject("release performance sample sequence overflowed");
    }

    ReleasePerformanceSample sample;
    sample.sequence = static_cast<std::uint64_t>(samples_.size()) + 1U;
    sample.elapsedMicroseconds = elapsedMicroseconds_;
    sample.windowDurationMicroseconds = windowDurationMicroseconds_;
    sample.gameplayDurationMicroseconds =
        windowGameplayDurationMicroseconds_;
    sample.focusedDurationMicroseconds =
        windowFocusedDurationMicroseconds_;
    sample.renderedFrames = windowRenderedFrames_;
    sample.residentBytes = *residentBytes;
    sample.completedStages = completedStages_;
    sample.stageClearEvents = windowStageClearEvents_;
    sample.stageNumber = stageNumber;
    sample.playerCount = playerCount;
    sample.appState = appState;
    sample.windowFocused = windowFocused;
    samples_.push_back(std::move(sample));
    windowDurationMicroseconds_ = 0U;
    windowGameplayDurationMicroseconds_ = 0U;
    windowFocusedDurationMicroseconds_ = 0U;
    windowRenderedFrames_ = 0U;
    windowStageClearEvents_ = 0U;
    return true;
}

ReleasePerformanceResult ReleasePerformanceRecorder::finalize(
    std::chrono::system_clock::time_point completedAtUtc,
    bool cleanShutdown)
{
    if (!valid())
        return {false, error_};
    if (finalized_)
        return {false, "release performance log was already finalized"};
    if (completedAtUtc < startedAtUtc_)
    {
        reject("release performance wall clock moved backwards");
        return {false, error_};
    }
    const auto completedTimestamp = utcTimestamp(completedAtUtc);
    if (!completedTimestamp.has_value())
    {
        reject("release performance completion timestamp is invalid");
        return {false, error_};
    }
    serializedJson_ = buildJson(*completedTimestamp, cleanShutdown);
    if (serializedJson_.empty())
    {
        reject("release performance JSON serialization failed");
        return {false, error_};
    }
    finalized_ = true;
    return {true, {}};
}

ReleasePerformanceResult ReleasePerformanceRecorder::saveNoReplace()
{
    if (!valid())
        return {false, error_};
    if (!finalized_)
        return {false, "release performance log is not finalized"};
    if (saved_)
        return {false, "release performance log was already saved"};
    const auto result = saveAtomicOutputFileNoReplace(
        options_.outputPath, serializedJson_.data(), serializedJson_.size());
    if (!result.saved())
    {
        return {false, result.message.empty()
                           ? "release performance log could not be saved"
                           : result.message};
    }
    saved_ = true;
    return {true, result.message};
}

std::uint64_t
ReleasePerformanceRecorder::monotonicDurationMicroseconds() const
{
    return elapsedMicroseconds_;
}

const std::vector<ReleasePerformanceSample> &
ReleasePerformanceRecorder::samples() const
{
    return samples_;
}

const std::string &ReleasePerformanceRecorder::serializedJson() const
{
    return serializedJson_;
}

std::string ReleasePerformanceRecorder::buildJson(
    const std::string &completedAtUtc, bool cleanShutdown) const
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\n"
           << "  \"schema\": "
           << jsonString(kReleasePerformanceLogSchema) << ",\n"
           << "  \"producer\": "
           << jsonString(kReleasePerformanceProducer) << ",\n"
           << "  \"source_commit\": " << jsonString(sourceCommit_)
           << ",\n"
           << "  \"source_tag\": " << jsonString(sourceTag_) << ",\n"
           << "  \"candidate_sha256\": "
           << jsonString(options_.candidateSha256) << ",\n"
           << "  \"session_nonce\": "
           << jsonString(options_.sessionNonce) << ",\n"
           << "  \"started_at_utc\": " << jsonString(startedAtUtcText_)
           << ",\n"
           << "  \"completed_at_utc\": "
           << jsonString(completedAtUtc) << ",\n"
           << "  \"monotonic_duration_us\": " << elapsedMicroseconds_
           << ",\n"
           << "  \"target_interval_us\": "
           << kReleasePerformanceIntervalMicroseconds << ",\n"
           << "  \"clock\": " << jsonString(kReleasePerformanceClock)
           << ",\n"
           << "  \"memory_metric\": "
           << jsonString(kReleasePerformanceMemoryMetric) << ",\n"
           << "  \"memory_unit\": "
           << jsonString(kReleasePerformanceMemoryUnit) << ",\n"
           << "  \"clean_shutdown\": "
           << (cleanShutdown ? "true" : "false") << ",\n"
           << "  \"samples\": [";
    if (!samples_.empty())
        output << '\n';
    for (std::size_t index = 0; index < samples_.size(); ++index)
    {
        const ReleasePerformanceSample &sample = samples_[index];
        output << "    {\"sequence\": " << sample.sequence
               << ", \"elapsed_us\": " << sample.elapsedMicroseconds
               << ", \"window_duration_us\": "
               << sample.windowDurationMicroseconds
               << ", \"gameplay_duration_us\": "
               << sample.gameplayDurationMicroseconds
               << ", \"focused_duration_us\": "
               << sample.focusedDurationMicroseconds
               << ", \"rendered_frames\": " << sample.renderedFrames
               << ", \"resident_bytes\": " << sample.residentBytes
               << ", \"stage_number\": " << sample.stageNumber
               << ", \"completed_stages\": " << sample.completedStages
               << ", \"stage_clear_events\": " << sample.stageClearEvents
               << ", \"player_count\": " << sample.playerCount
               << ", \"app_state\": " << jsonString(sample.appState)
               << ", \"window_focused\": "
               << (sample.windowFocused ? "true" : "false") << '}';
        if (index + 1U != samples_.size())
            output << ',';
        output << '\n';
    }
    if (!samples_.empty())
        output << "  ";
    output << "]\n}\n";
    return output.str();
}
} // namespace tanks3d::app

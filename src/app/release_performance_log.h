#ifndef TANKS3D_APP_RELEASE_PERFORMANCE_LOG_H
#define TANKS3D_APP_RELEASE_PERFORMANCE_LOG_H

#include "release_performance_options.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tanks3d::app
{
inline constexpr const char *kReleasePerformanceLogSchema =
    "tanks3d-performance-log-v2";
inline constexpr const char *kReleasePerformanceProducer = "Tanks3D";
inline constexpr const char *kReleasePerformanceClock = "steady_clock";
inline constexpr const char *kReleasePerformanceMemoryMetric =
    "proc_pid_rusage.ri_phys_footprint";
inline constexpr const char *kReleasePerformanceMemoryUnit = "bytes";

using ReleasePerformanceResidentBytesSampler =
    std::function<std::optional<std::uint64_t>()>;

std::optional<std::uint64_t>
sampleCurrentProcessPhysicalFootprintBytes();

struct ReleasePerformanceSample
{
    std::uint64_t sequence = 0U;
    std::uint64_t elapsedMicroseconds = 0U;
    std::uint64_t windowDurationMicroseconds = 0U;
    std::uint64_t gameplayDurationMicroseconds = 0U;
    std::uint64_t focusedDurationMicroseconds = 0U;
    std::uint64_t renderedFrames = 0U;
    std::uint64_t residentBytes = 0U;
    std::uint64_t completedStages = 0U;
    std::uint64_t stageClearEvents = 0U;
    int stageNumber = 0;
    int playerCount = 0;
    std::string appState;
    bool windowFocused = false;
};

struct ReleasePerformanceResult
{
    bool success = false;
    std::string message;

    bool succeeded() const
    {
        return success;
    }
};

class ReleasePerformanceRecorder
{
public:
    ReleasePerformanceRecorder(
        ReleasePerformanceOptions options, std::string sourceCommit,
        std::string sourceTag,
        std::chrono::system_clock::time_point startedAtUtc,
        ReleasePerformanceResidentBytesSampler residentBytesSampler =
            sampleCurrentProcessPhysicalFootprintBytes);

    bool valid() const;
    bool failed() const;
    bool finalized() const;
    bool saved() const;
    bool targetDurationReached() const;
    const std::string &error() const;

    // Records one genuinely completed frame. Durations are accumulated as
    // integer microseconds within the current sample window, and completed
    // stages must never decrease or advance by more than one per frame. A
    // sample is emitted only when the real window has reached one second; a
    // delayed frame produces one longer window rather than fabricated
    // catch-up samples.
    bool recordFrame(double actualDurationSeconds, int stageNumber,
                     int playerCount, const std::string &appState,
                     bool windowFocused, std::uint64_t completedStages);

    // Finalization does not manufacture a trailing partial sample. The caller
    // supplies the wall-clock endpoint and whether shutdown was orderly.
    ReleasePerformanceResult finalize(
        std::chrono::system_clock::time_point completedAtUtc,
        bool cleanShutdown);

    // Publishes the finalized JSON through the existing same-directory,
    // atomic, no-replace release writer.
    ReleasePerformanceResult saveNoReplace();

    std::uint64_t monotonicDurationMicroseconds() const;
    const std::vector<ReleasePerformanceSample> &samples() const;
    const std::string &serializedJson() const;

private:
    bool reject(std::string message);
    bool appendSample(int stageNumber, int playerCount,
                      const std::string &appState, bool windowFocused);
    std::string buildJson(const std::string &completedAtUtc,
                          bool cleanShutdown) const;

    ReleasePerformanceOptions options_;
    std::string sourceCommit_;
    std::string sourceTag_;
    std::chrono::system_clock::time_point startedAtUtc_;
    ReleasePerformanceResidentBytesSampler residentBytesSampler_;
    std::uint64_t elapsedMicroseconds_ = 0U;
    std::uint64_t windowDurationMicroseconds_ = 0U;
    std::uint64_t windowGameplayDurationMicroseconds_ = 0U;
    std::uint64_t windowFocusedDurationMicroseconds_ = 0U;
    std::uint64_t windowRenderedFrames_ = 0U;
    std::uint64_t windowStageClearEvents_ = 0U;
    std::uint64_t completedStages_ = 0U;
    std::vector<ReleasePerformanceSample> samples_;
    std::string startedAtUtcText_;
    std::string serializedJson_;
    std::string error_;
    bool configurationValid_ = false;
    bool targetDurationReached_ = false;
    bool finalized_ = false;
    bool saved_ = false;
};
} // namespace tanks3d::app

#endif

#include "release_performance_capabilities.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tanks3d::app
{
namespace
{
bool contains(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}
} // namespace

ReleasePerformanceCapabilityCheck checkReleasePerformanceCapabilities(
    const std::string &sourceCommit, const std::string &sourceTag)
{
    const std::string candidateSha256(64U, 'a');
    const std::string sessionNonce(32U, 'b');
    const std::vector<std::string> arguments{
        kReleasePerformanceQuickStartArgument,
        std::string{kReleasePerformanceLogArgumentPrefix} +
            "capability-probe.json",
        std::string{kReleasePerformanceCandidateArgumentPrefix} +
            candidateSha256,
        std::string{kReleasePerformanceNonceArgumentPrefix} + sessionNonce,
        std::string{kReleasePerformanceDurationArgumentPrefix} +
            std::to_string(kDefaultReleasePerformanceDurationSeconds),
    };
    const auto parsed = parseReleasePerformanceOptions(arguments);
    if (!parsed.valid() || !parsed.options.requested() ||
        parsed.options.durationSeconds !=
            kDefaultReleasePerformanceDurationSeconds ||
        parsed.options.targetDurationMicroseconds() !=
            static_cast<std::int64_t>(
                kDefaultReleasePerformanceDurationSeconds) *
                1000000 ||
        parsed.options.candidateSha256 != candidateSha256 ||
        parsed.options.sessionNonce != sessionNonce)
    {
        return {false, "canonical release-performance arguments were rejected"};
    }

    std::vector<std::string> nonDefaultDurationArguments = arguments;
    nonDefaultDurationArguments.back() =
        std::string{kReleasePerformanceDurationArgumentPrefix} + "7";
    const auto nonDefaultDuration =
        parseReleasePerformanceOptions(nonDefaultDurationArguments);
    if (!nonDefaultDuration.valid() ||
        nonDefaultDuration.options.durationSeconds != 7 ||
        nonDefaultDuration.options.targetDurationMicroseconds() != 7000000)
    {
        return {false, "release-performance duration argument was not honored"};
    }

    ReleasePerformanceOptions oneWindowOptions = parsed.options;
    oneWindowOptions.durationSeconds = 1;
    const auto startedAt = std::chrono::system_clock::time_point{
        std::chrono::seconds{1700000000}};
    ReleasePerformanceRecorder recorder{
        oneWindowOptions, sourceCommit, sourceTag, startedAt,
        []() -> std::optional<std::uint64_t> {
            return 128U * 1024U * 1024U;
        }};
    if (!recorder.valid())
        return {false, recorder.error()};
    if (!recorder.recordFrame(1.0, 1, 1, "gameplay", true, 0U) ||
        !recorder.targetDurationReached() || recorder.samples().size() != 1U)
    {
        return {false, "deterministic release-performance window failed"};
    }
    const auto finalized = recorder.finalize(
        startedAt + std::chrono::seconds{1}, true);
    if (!finalized.succeeded())
        return {false, finalized.message};

    const std::string &json = recorder.serializedJson();
    const std::vector<std::string> requiredFragments{
        "\"schema\": \"" + std::string{kReleasePerformanceLogSchema} + "\"",
        "\"producer\": \"" + std::string{kReleasePerformanceProducer} + "\"",
        "\"source_commit\": \"" + sourceCommit + "\"",
        "\"source_tag\": \"" + sourceTag + "\"",
        "\"candidate_sha256\": \"" + candidateSha256 + "\"",
        "\"session_nonce\": \"" + sessionNonce + "\"",
        "\"target_interval_us\": 1000000",
        "\"clock\": \"" + std::string{kReleasePerformanceClock} + "\"",
        "\"memory_metric\": \"" +
            std::string{kReleasePerformanceMemoryMetric} + "\"",
        "\"memory_unit\": \"" +
            std::string{kReleasePerformanceMemoryUnit} + "\"",
        "\"clean_shutdown\": true",
        "\"sequence\": 1",
        "\"app_state\": \"gameplay\"",
    };
    for (const std::string &fragment : requiredFragments)
    {
        if (!contains(json, fragment))
            return {false, "deterministic telemetry serialization drifted"};
    }
    return {true, {}};
}
} // namespace tanks3d::app

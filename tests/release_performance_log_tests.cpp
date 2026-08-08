#include "app/release_performance_log.h"
#include "app/release_performance_options.h"
#include "test_support.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <unistd.h>

using namespace tanks3d::app;

namespace
{
namespace fs = std::filesystem;

const std::string kCandidateSha(64U, 'a');
const std::string kNonce32(32U, 'b');
const std::string kNonce64(64U, 'c');
const std::string kSourceCommit(40U, 'd');
const std::string kSourceTag = "v0.1.0-alpha.4";
const auto kStartedAt = std::chrono::system_clock::time_point{
    std::chrono::seconds{1700000000}} + std::chrono::milliseconds{123};

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        std::string path =
            (fs::temp_directory_path() /
             "tanks3d-performance-log-test.XXXXXX")
                .string();
        std::vector<char> mutablePath(path.begin(), path.end());
        mutablePath.push_back('\0');
        if (::mkdtemp(mutablePath.data()) != nullptr)
            path_ = mutablePath.data();
    }

    ~TemporaryDirectory()
    {
        if (!path_.empty())
        {
            std::error_code error;
            fs::remove_all(path_, error);
        }
    }

    bool valid() const
    {
        return !path_.empty();
    }

    const fs::path &path() const
    {
        return path_;
    }

private:
    fs::path path_;
};

ReleasePerformanceOptions validOptions(const fs::path &output,
                                       int durationSeconds = 1801)
{
    ReleasePerformanceOptions options;
    options.outputPath = output.string();
    options.candidateSha256 = kCandidateSha;
    options.sessionNonce = kNonce32;
    options.durationSeconds = durationSeconds;
    return options;
}

std::string readText(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

ReleasePerformanceRecorder recorderWithConstantRss(
    const ReleasePerformanceOptions &options,
    std::uint64_t residentBytes = 200U * 1024U * 1024U)
{
    return ReleasePerformanceRecorder{
        options, kSourceCommit, kSourceTag, kStartedAt,
        [residentBytes]() -> std::optional<std::uint64_t> {
            return residentBytes;
        }};
}

bool contains(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        passed = reporter.check(condition, message) && passed;
    };

    reporter.beginSuite("release-performance-options-default-and-valid");
    const auto disabled = parseReleasePerformanceOptions(
        {"--quick-start", "--stage=7", "--release-screenshot=shot.png"});
    expect(disabled.valid() && !disabled.options.requested() &&
               disabled.options.durationSeconds ==
                   kDefaultReleasePerformanceDurationSeconds,
           "unrelated arguments enabled or changed telemetry defaults");
    const auto valid = parseReleasePerformanceOptions(
        {"--release-session-nonce=" + kNonce32,
         "--release-performance-duration-seconds=1",
         "--quick-start",
         "--release-candidate-sha256=" + kCandidateSha,
         "--release-performance-log=/tmp/performance log.json"});
    expect(valid.valid() && valid.options.requested() &&
               valid.options.outputPath == "/tmp/performance log.json" &&
               valid.options.candidateSha256 == kCandidateSha &&
               valid.options.sessionNonce == kNonce32 &&
               valid.options.durationSeconds == 1 &&
               valid.options.targetDurationMicroseconds() == 1000000,
           "a valid order-independent telemetry tuple was rejected");
    const auto maximum = parseReleasePerformanceOptions(
        {"--release-performance-log=maximum.json",
         "--release-candidate-sha256=" + kCandidateSha,
         "--release-session-nonce=" + kNonce64,
         "--quick-start",
         "--release-performance-duration-seconds=14400"});
    expect(maximum.valid() && maximum.options.sessionNonce == kNonce64 &&
               maximum.options.durationSeconds ==
                   kMaximumReleasePerformanceDurationSeconds,
           "a 64-character nonce or maximum duration was rejected");

    reporter.beginSuite("release-performance-options-invalid-values");
    for (const std::vector<std::string> &arguments : {
             std::vector<std::string>{"--release-performance-log"},
             std::vector<std::string>{"--release-candidate-sha256"},
             std::vector<std::string>{"--release-session-nonce"},
             std::vector<std::string>{
                 "--release-performance-duration-seconds"},
             std::vector<std::string>{"--release-performance-log="},
             std::vector<std::string>{
                 "--release-performance-log=performance.JSON"},
             std::vector<std::string>{
                 "--release-performance-log=bad\npath.json"},
         })
    {
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "an incomplete or invalid output option was accepted");
    }
    std::string nulPath = "--release-performance-log=bad";
    nulPath.push_back('\0');
    nulPath += "path.json";
    expect(!parseReleasePerformanceOptions({nulPath}).valid(),
           "an output path containing NUL was accepted");
    for (const std::string &badSha : {
             std::string(63U, 'a'), std::string(65U, 'a'),
             std::string(64U, 'A'), std::string(63U, 'a') + "g",
         })
    {
        expect(!parseReleasePerformanceOptions(
                    {"--release-performance-log=out.json",
                     "--release-candidate-sha256=" + badSha,
                     "--release-session-nonce=" + kNonce32})
                    .valid(),
               "an invalid candidate digest was accepted");
    }
    for (const std::string &badNonce : {
             std::string(31U, 'b'), std::string(33U, 'b'),
             std::string(32U, 'B'), std::string(31U, 'b') + "g",
         })
    {
        expect(!parseReleasePerformanceOptions(
                    {"--release-performance-log=out.json",
                     "--release-candidate-sha256=" + kCandidateSha,
                     "--release-session-nonce=" + badNonce})
                    .valid(),
               "an invalid session nonce was accepted");
    }
    for (const std::string &badDuration : {
             "", "0", "-1", "1x", "14401",
             "999999999999999999999999",
         })
    {
        expect(!parseReleasePerformanceOptions(
                    {"--release-performance-log=out.json",
                     "--release-candidate-sha256=" + kCandidateSha,
                     "--release-session-nonce=" + kNonce32,
                     "--release-performance-duration-seconds=" + badDuration})
                    .valid(),
               "an invalid performance duration was accepted");
    }

    reporter.beginSuite("release-performance-options-completeness-and-conflicts");
    for (const std::vector<std::string> &arguments : {
             std::vector<std::string>{
                 "--release-performance-log=out.json"},
             std::vector<std::string>{
                 "--release-candidate-sha256=" + kCandidateSha},
             std::vector<std::string>{
                 "--release-session-nonce=" + kNonce32},
             std::vector<std::string>{
                 "--release-performance-duration-seconds=2"},
             std::vector<std::string>{
                 "--release-performance-log=out.json",
                 "--release-candidate-sha256=" + kCandidateSha},
         })
    {
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "an incomplete telemetry tuple was accepted");
    }
    const std::vector<std::string> baseTuple{
        "--quick-start",
        "--release-performance-log=out.json",
        "--release-candidate-sha256=" + kCandidateSha,
        "--release-session-nonce=" + kNonce32};
    for (const std::string &duplicate : std::vector<std::string>{
             "--release-performance-log=two.json",
             "--release-candidate-sha256=" + kCandidateSha,
             "--release-session-nonce=" + kNonce32,
             "--release-performance-duration-seconds=2",
         })
    {
        std::vector<std::string> arguments = baseTuple;
        if (duplicate.find("duration") != std::string::npos)
            arguments.push_back("--release-performance-duration-seconds=1");
        arguments.push_back(duplicate);
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "a duplicate telemetry option was accepted");
    }
    for (const std::string &screenshotOption : {
             "--release-screenshot=shot.png",
             "--release-screenshot-frame=2",
             "--release-screenshot",
         })
    {
        std::vector<std::string> arguments = baseTuple;
        arguments.push_back(screenshotOption);
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "performance telemetry was combined with screenshot capture");
    }
    for (const std::string &incompatible : {
             "--quick-start-2p",
             "--quick-start-ussr",
             "--tank-showcase",
             "--stage=2",
             "--gltf-tank-qa=model.glb",
         })
    {
        std::vector<std::string> arguments = baseTuple;
        arguments.push_back(incompatible);
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "performance telemetry accepted an incompatible mode");
    }
    {
        std::vector<std::string> arguments = baseTuple;
        arguments.push_back("--quick-start");
        expect(!parseReleasePerformanceOptions(arguments).valid(),
               "performance telemetry accepted duplicate quick-start");
    }
    expect(!parseReleasePerformanceOptions(
                {"--release-performance-log=out.json",
                 "--release-candidate-sha256=" + kCandidateSha,
                 "--release-session-nonce=" + kNonce32})
                .valid(),
           "performance telemetry accepted a missing quick-start");

    TemporaryDirectory temporaryDirectory;
    reporter.beginSuite("release-performance-recorder-configuration");
    expect(temporaryDirectory.valid(),
           "a private performance test directory could not be created");
    if (!temporaryDirectory.valid())
    {
        reporter.finish();
        return 1;
    }
    const fs::path output = temporaryDirectory.path() / "performance.json";
    auto configured = recorderWithConstantRss(validOptions(output));
    expect(configured.valid() && !configured.failed() &&
               !configured.finalized() && !configured.saved() &&
               !configured.targetDurationReached(),
           "a valid recorder configuration was rejected");
    {
        ReleasePerformanceOptions options = validOptions(output);
        options.outputPath.clear();
        auto recorder = recorderWithConstantRss(options);
        expect(!recorder.valid(), "disabled telemetry constructed a recorder");
    }
    for (const std::pair<std::string, std::string> &metadata : {
             std::pair<std::string, std::string>{"bad", kSourceTag},
             std::pair<std::string, std::string>{kSourceCommit, ""},
             std::pair<std::string, std::string>{kSourceCommit,
                                                 "bad tag"},
             std::pair<std::string, std::string>{kSourceCommit,
                                                 ".hidden-tag"},
             std::pair<std::string, std::string>{kSourceCommit,
                                                 "bad+tag"},
         })
    {
        ReleasePerformanceRecorder recorder{
            validOptions(output), metadata.first, metadata.second, kStartedAt,
            []() -> std::optional<std::uint64_t> { return 1U; }};
        expect(!recorder.valid(), "invalid source metadata was accepted");
    }
    {
        ReleasePerformanceRecorder recorder{
            validOptions(output), kSourceCommit, kSourceTag, kStartedAt, {}};
        expect(!recorder.valid(), "an absent RSS sampler was accepted");
    }

    reporter.beginSuite("release-performance-real-windows-and-state");
    int samplerCalls = 0;
    std::uint64_t nextResidentBytes = 1000U;
    ReleasePerformanceRecorder recorder{
        validOptions(output), kSourceCommit, kSourceTag, kStartedAt,
        [&]() -> std::optional<std::uint64_t> {
            ++samplerCalls;
            return nextResidentBytes++;
        }};
    for (int frame = 0; frame < 3; ++frame)
    {
        expect(recorder.recordFrame(0.25, 2, 1, "gameplay", true, 0U),
               "a valid sub-window frame was rejected");
    }
    expect(recorder.samples().empty() && samplerCalls == 0,
           "RSS was sampled before a real one-second window completed");
    expect(recorder.recordFrame(0.25, 3, 2, "settlement", false, 1U),
           "the one-second boundary frame was rejected");
    expect(recorder.samples().size() == 1U && samplerCalls == 1,
           "the one-second window did not produce exactly one sample");
    if (!recorder.samples().empty())
    {
        const auto &sample = recorder.samples().front();
        expect(sample.sequence == 1U &&
                   sample.elapsedMicroseconds == 1000000U &&
                   sample.windowDurationMicroseconds == 1000000U &&
                   sample.gameplayDurationMicroseconds == 750000U &&
                   sample.focusedDurationMicroseconds == 750000U &&
                   sample.renderedFrames == 4U &&
                   sample.residentBytes == 1000U &&
                   sample.completedStages == 1U &&
                   sample.stageClearEvents == 1U &&
                   sample.stageNumber == 3 && sample.playerCount == 2 &&
                   sample.appState == "settlement" &&
                   !sample.windowFocused,
               "the first raw sample changed timing, RSS, or app state");
    }
    expect(recorder.recordFrame(0.6, 4, 1, "gameplay", true, 1U) &&
               recorder.recordFrame(0.5, 4, 1, "gameplay", true, 2U),
           "a valid overrun window was rejected");
    expect(recorder.samples().size() == 2U && samplerCalls == 2,
           "an overrun window did not produce one real sample");
    if (recorder.samples().size() == 2U)
    {
        const auto &sample = recorder.samples()[1];
        expect(sample.sequence == 2U &&
                   sample.elapsedMicroseconds == 2100000U &&
                   sample.windowDurationMicroseconds == 1100000U &&
                   sample.gameplayDurationMicroseconds == 1100000U &&
                   sample.focusedDurationMicroseconds == 1100000U &&
                   sample.renderedFrames == 2U &&
                   sample.residentBytes == 1001U &&
                   sample.completedStages == 2U &&
                   sample.stageClearEvents == 1U,
               "the overrun window was rounded, backfilled, or failed to "
               "reset its duration counters");
    }
    expect(recorder.recordFrame(0.5, 4, 1, "gameplay", true, 2U) &&
               recorder.recordFrame(0.5, 4, 1, "gameplay", true, 2U),
           "a clear-free follow-up window was rejected");
    expect(recorder.samples().size() == 3U && samplerCalls == 3 &&
               recorder.samples()[2].elapsedMicroseconds == 3100000U &&
               recorder.samples()[2].windowDurationMicroseconds ==
                   1000000U &&
               recorder.samples()[2].stageClearEvents == 0U &&
               recorder.samples()[2].completedStages == 2U,
           "stage clear events did not reset independently of the cumulative "
           "stage count");
    std::uint64_t totalStageClearEvents = 0U;
    for (const auto &sample : recorder.samples())
        totalStageClearEvents += sample.stageClearEvents;
    expect(totalStageClearEvents == recorder.samples().back().completedStages,
           "window stage-clear events do not reconstruct the cumulative "
           "stage count");

    reporter.beginSuite("release-performance-completed-stages-monotonic");
    auto regressingStages = recorderWithConstantRss(validOptions(
        temporaryDirectory.path() / "regressing-stages.json"));
    expect(regressingStages.recordFrame(
               0.4, 2, 1, "gameplay", true, 1U),
           "an initial completed-stage count was rejected");
    expect(!regressingStages.recordFrame(
               0.6, 2, 1, "gameplay", true, 0U) &&
               regressingStages.failed() &&
               regressingStages.samples().empty() &&
               regressingStages.monotonicDurationMicroseconds() == 400000U,
           "a backwards completed-stage count was accepted or mutated "
           "telemetry");
    auto jumpingStages = recorderWithConstantRss(validOptions(
        temporaryDirectory.path() / "jumping-stages.json"));
    expect(jumpingStages.recordFrame(
               0.4, 2, 1, "gameplay", true, 1U),
           "a single stage-clear event was rejected");
    expect(!jumpingStages.recordFrame(
               0.6, 4, 1, "gameplay", true, 3U) &&
               jumpingStages.failed() && jumpingStages.samples().empty() &&
               jumpingStages.monotonicDurationMicroseconds() == 400000U,
           "a two-stage jump in one frame was accepted or mutated telemetry");

    reporter.beginSuite("release-performance-no-catch-up-samples");
    int delayedSamplerCalls = 0;
    ReleasePerformanceRecorder delayed{
        validOptions(temporaryDirectory.path() / "delayed.json", 2),
        kSourceCommit, kSourceTag, kStartedAt,
        [&]() -> std::optional<std::uint64_t> {
            ++delayedSamplerCalls;
            return 4096U;
        }};
    expect(delayed.recordFrame(2.5, 7, 1, "gameplay", true, 1U),
           "a genuine delayed frame was rejected");
    expect(delayed.samples().size() == 1U && delayedSamplerCalls == 1 &&
               delayed.samples()[0].elapsedMicroseconds == 2500000U &&
               delayed.samples()[0].windowDurationMicroseconds == 2500000U &&
               delayed.samples()[0].gameplayDurationMicroseconds ==
                   2500000U &&
               delayed.samples()[0].focusedDurationMicroseconds ==
                   2500000U &&
               delayed.samples()[0].renderedFrames == 1U &&
               delayed.samples()[0].completedStages == 1U &&
               delayed.samples()[0].stageClearEvents == 1U &&
               delayed.targetDurationReached(),
           "a delayed frame was split into fabricated interval samples");
    expect(delayed.recordFrame(1.0, 8, 2, "gameplay", false, 1U) &&
               delayed.samples().size() == 1U &&
               delayed.monotonicDurationMicroseconds() == 2500000U &&
               delayedSamplerCalls == 1,
           "frames after the requested endpoint changed the captured session");

    reporter.beginSuite("release-performance-target-finishes-real-window");
    ReleasePerformanceRecorder roundedTarget{
        validOptions(temporaryDirectory.path() / "rounded-target.json", 2),
        kSourceCommit, kSourceTag, kStartedAt,
        []() -> std::optional<std::uint64_t> { return 8192U; }};
    expect(roundedTarget.recordFrame(
               0.6, 1, 1, "gameplay", true, 0U) &&
               roundedTarget.recordFrame(
                   0.5, 1, 1, "gameplay", true, 0U) &&
               roundedTarget.samples().size() == 1U &&
               !roundedTarget.targetDurationReached(),
           "the first real overrun window ended a two-second session");
    expect(roundedTarget.recordFrame(
               0.6, 1, 1, "gameplay", true, 0U) &&
               roundedTarget.recordFrame(
                   0.3, 1, 1, "gameplay", true, 0U) &&
               roundedTarget.monotonicDurationMicroseconds() == 2000000U &&
               !roundedTarget.targetDurationReached() &&
               roundedTarget.samples().size() == 1U,
           "a partial window at the duration target ended telemetry early");
    expect(roundedTarget.recordFrame(
               0.2, 1, 1, "gameplay", true, 0U) &&
               roundedTarget.targetDurationReached() &&
               roundedTarget.samples().size() == 2U &&
               roundedTarget.monotonicDurationMicroseconds() == 2200000U,
           "telemetry did not stop at the first full window after its target");
    std::uint64_t coveredMicroseconds = 0U;
    for (const auto &sample : roundedTarget.samples())
        coveredMicroseconds += sample.windowDurationMicroseconds;
    expect(coveredMicroseconds ==
               roundedTarget.monotonicDurationMicroseconds() &&
               roundedTarget.samples()[1].windowDurationMicroseconds ==
                   1100000U,
           "target completion left unrepresented monotonic duration");

    reporter.beginSuite("release-performance-invalid-frame-input");
    for (const double duration : {
             0.0, -0.1, std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN(), 0.0000004, 1.0e13,
         })
    {
        auto invalid = recorderWithConstantRss(
            validOptions(temporaryDirectory.path() / "invalid-frame.json"));
        expect(!invalid.recordFrame(
                   duration, 1, 1, "gameplay", true, 0U) &&
                   invalid.failed() && invalid.samples().empty(),
               "a non-finite, non-positive, or zero-rounded frame was accepted");
    }
    struct InvalidState
    {
        int stage;
        int players;
        const char *state;
    };
    for (const InvalidState &value : {
             InvalidState{0, 1, "gameplay"},
             InvalidState{1, 0, "gameplay"},
             InvalidState{1, 3, "gameplay"},
             InvalidState{1, 1, ""},
             InvalidState{1, 1, "Game Play"},
             InvalidState{1, 1, "menu"},
             InvalidState{1, 1, "advanced_menu"},
             InvalidState{1, 1, "paused"},
         })
    {
        auto invalid = recorderWithConstantRss(
            validOptions(temporaryDirectory.path() / "invalid-state.json"));
        expect(!invalid.recordFrame(1.0, value.stage, value.players,
                                    value.state, true, 0U) &&
                   invalid.failed() && invalid.samples().empty(),
               "invalid frame context was accepted");
    }
    for (const std::string &canonicalState : {
             "gameplay", "settlement", "high_score",
         })
    {
        auto canonical = recorderWithConstantRss(
            validOptions(temporaryDirectory.path() / "canonical-state.json"));
        expect(canonical.recordFrame(
                   0.5, 1, 1, canonicalState, true, 0U) &&
                   !canonical.failed(),
               "a canonical v2 app state was rejected");
    }

    reporter.beginSuite("release-performance-sampler-failure");
    for (const std::optional<std::uint64_t> sampled : {
             std::optional<std::uint64_t>{},
             std::optional<std::uint64_t>{0U},
         })
    {
        int calls = 0;
        ReleasePerformanceRecorder failing{
            validOptions(temporaryDirectory.path() / "rss-failure.json"),
            kSourceCommit, kSourceTag, kStartedAt,
            [&]() -> std::optional<std::uint64_t> {
                ++calls;
                return sampled;
            }};
        expect(failing.recordFrame(
                   0.5, 1, 1, "gameplay", true, 0U) &&
                   calls == 0,
               "RSS was sampled before its window boundary");
        expect(!failing.recordFrame(
                   0.5, 1, 1, "gameplay", true, 0U) &&
                   failing.failed() && failing.samples().empty() && calls == 1,
               "an absent or zero physical-footprint sample was accepted");
    }

    reporter.beginSuite("release-performance-strict-v2-json");
    const fs::path strictOutput = temporaryDirectory.path() / "strict.json";
    auto strict = recorderWithConstantRss(validOptions(strictOutput), 4096U);
    expect(strict.recordFrame(1.0, 7, 2, "gameplay", true, 1U),
           "the strict JSON fixture frame was rejected");
    const auto strictFinish = strict.finalize(
        kStartedAt + std::chrono::seconds{2}, true);
    expect(strictFinish.succeeded() && strict.finalized(),
           "a valid strict JSON log could not be finalized");
    const std::string expectedJson =
        "{\n"
        "  \"schema\": \"tanks3d-performance-log-v2\",\n"
        "  \"producer\": \"Tanks3D\",\n"
        "  \"source_commit\": \"" + kSourceCommit + "\",\n"
        "  \"source_tag\": \"v0.1.0-alpha.4\",\n"
        "  \"candidate_sha256\": \"" + kCandidateSha + "\",\n"
        "  \"session_nonce\": \"" + kNonce32 + "\",\n"
        "  \"started_at_utc\": \"2023-11-14T22:13:20Z\",\n"
        "  \"completed_at_utc\": \"2023-11-14T22:13:22Z\",\n"
        "  \"monotonic_duration_us\": 1000000,\n"
        "  \"target_interval_us\": 1000000,\n"
        "  \"clock\": \"steady_clock\",\n"
        "  \"memory_metric\": "
        "\"proc_pid_rusage.ri_phys_footprint\",\n"
        "  \"memory_unit\": \"bytes\",\n"
        "  \"clean_shutdown\": true,\n"
        "  \"samples\": [\n"
        "    {\"sequence\": 1, \"elapsed_us\": 1000000, "
        "\"window_duration_us\": 1000000, "
        "\"gameplay_duration_us\": 1000000, "
        "\"focused_duration_us\": 1000000, \"rendered_frames\": 1, "
        "\"resident_bytes\": 4096, \"stage_number\": 7, "
        "\"completed_stages\": 1, \"stage_clear_events\": 1, "
        "\"player_count\": 2, \"app_state\": \"gameplay\", "
        "\"window_focused\": true}\n"
        "  ]\n"
        "}\n";
    expect(strict.serializedJson() == expectedJson,
           "the v2 JSON keys, order, integer values, or UTC format changed");
    expect(!contains(strict.serializedJson(), "1000000.0") &&
               !contains(strict.serializedJson(), "4096.0") &&
               !contains(strict.serializedJson(), "nan") &&
               !contains(strict.serializedJson(), "inf"),
           "the strict JSON contains floating-point telemetry values");
    expect(!strict.finalize(kStartedAt + std::chrono::seconds{3}, true)
                .succeeded(),
           "a finalized log was finalized twice");

    reporter.beginSuite("release-performance-partial-and-clock-failures");
    auto partial = recorderWithConstantRss(
        validOptions(temporaryDirectory.path() / "partial.json"));
    expect(partial.recordFrame(0.75, 1, 1, "settlement", false, 0U),
           "a valid partial window was rejected");
    expect(partial.finalize(kStartedAt + std::chrono::seconds{1}, false)
               .succeeded() &&
               partial.samples().empty() &&
               partial.monotonicDurationMicroseconds() == 750000U &&
               contains(partial.serializedJson(),
                        "\"clean_shutdown\": false") &&
               contains(partial.serializedJson(), "\"samples\": []"),
           "finalization fabricated a partial sample or lost shutdown state");
    auto backwards = recorderWithConstantRss(
        validOptions(temporaryDirectory.path() / "backwards.json"));
    expect(!backwards.finalize(kStartedAt - std::chrono::seconds{1}, true)
                .succeeded() &&
               backwards.failed() && !backwards.finalized() &&
               backwards.serializedJson().empty(),
           "a backwards wall clock produced a release log");

    reporter.beginSuite("release-performance-atomic-no-replace-save");
    expect(strict.saveNoReplace().succeeded() && strict.saved(),
           "a finalized performance log could not be atomically saved");
    expect(readText(strictOutput) == expectedJson,
           "saved performance JSON differs from the finalized bytes");
    expect(!strict.saveNoReplace().succeeded() &&
               readText(strictOutput) == expectedJson,
           "a second save replaced the release performance log");
    auto conflicting = recorderWithConstantRss(validOptions(strictOutput), 1U);
    expect(conflicting.recordFrame(
               1.0, 1, 1, "gameplay", true, 0U) &&
               conflicting.finalize(kStartedAt + std::chrono::seconds{1}, true)
                   .succeeded() &&
               !conflicting.saveNoReplace().succeeded() &&
               readText(strictOutput) == expectedJson,
           "an existing performance log was overwritten");
    auto missingParent = recorderWithConstantRss(validOptions(
        temporaryDirectory.path() / "missing" / "performance.json"));
    expect(missingParent.recordFrame(
               1.0, 1, 1, "gameplay", true, 0U) &&
               missingParent.finalize(
                   kStartedAt + std::chrono::seconds{1}, true)
                   .succeeded() &&
               !missingParent.saveNoReplace().succeeded() &&
               !fs::exists(temporaryDirectory.path() / "missing" /
                           "performance.json"),
           "a missing parent produced a partial final output");

    reporter.beginSuite("release-performance-production-rss");
#if defined(__APPLE__)
    const auto physicalFootprint =
        sampleCurrentProcessPhysicalFootprintBytes();
    expect(physicalFootprint.has_value() && *physicalFootprint > 0U,
           "proc_pid_rusage did not return a current physical footprint");
#else
    expect(!sampleCurrentProcessPhysicalFootprintBytes().has_value(),
           "a non-macOS build claimed to provide the macOS memory metric");
#endif

    reporter.finish();
    return passed ? 0 : 1;
}

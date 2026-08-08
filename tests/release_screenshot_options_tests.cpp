#include "app/release_screenshot_file.h"
#include "app/release_screenshot_options.h"

#include "test_support.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
namespace fs = std::filesystem;

using tanks3d::app::ReleaseScreenshotFileStatus;
using tanks3d::app::kDefaultReleaseScreenshotFrame;
using tanks3d::app::kMaximumReleaseScreenshotFrame;
using tanks3d::app::kReleaseScreenshotHeight;
using tanks3d::app::kReleaseScreenshotWidth;
using tanks3d::app::parseReleaseScreenshotOptions;
using tanks3d::app::saveReleaseScreenshotFileNoReplace;

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        std::error_code error;
        const fs::path base = fs::temp_directory_path(error);
        if (error)
            return;
        const auto nonce = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            const fs::path candidate =
                base / ("tanks3d-screenshot-file-test." +
                        std::to_string(nonce) + "." +
                        std::to_string(attempt));
            error.clear();
            if (fs::create_directory(candidate, error))
            {
                path_ = candidate;
                return;
            }
            if (error != std::errc::file_exists)
                return;
        }
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    ~TemporaryDirectory()
    {
        std::error_code error;
        if (!path_.empty())
            fs::remove_all(path_, error);
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

bool writeBytes(const fs::path &path,
                const std::vector<unsigned char> &bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

std::vector<unsigned char> readBytes(const fs::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
}

bool hasScreenshotTemporaryFile(const fs::path &directory)
{
    std::error_code error;
    for (fs::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error))
    {
        const std::string name = iterator->path().filename().string();
        if (name.rfind(".tanks3d-release-screenshot.", 0) == 0)
            return true;
    }
    return static_cast<bool>(error);
}

struct PublishConflictContext
{
    fs::path output;
    std::vector<unsigned char> bytes;
    bool created = false;
};

void createPublishConflict(void *opaque)
{
    auto &context = *static_cast<PublishConflictContext *>(opaque);
    context.created = writeBytes(context.output, context.bytes);
}
}

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        passed = reporter.check(condition, message) && passed;
    };

    reporter.beginSuite("release-screenshot-disabled-and-defaults");
    const auto disabled = parseReleaseScreenshotOptions(
        {"--quick-start", "--stage=7"});
    expect(disabled.valid() && !disabled.options.requested() &&
               disabled.options.frame == kDefaultReleaseScreenshotFrame,
           "unrelated gameplay arguments enabled or changed screenshot mode");
    expect(kReleaseScreenshotWidth == 1280 &&
               kReleaseScreenshotHeight == 720,
           "the release screenshot canvas dimensions changed");

    const auto defaultFrame = parseReleaseScreenshotOptions(
        {"--quick-start", "--tank-showcase", "--stage=35",
         "--release-screenshot=release shots/hero image.png"});
    expect(defaultFrame.valid() && defaultFrame.options.requested() &&
               defaultFrame.options.outputPath ==
                   "release shots/hero image.png" &&
               defaultFrame.options.frame == kDefaultReleaseScreenshotFrame,
           "a valid spaced output path or the default frame changed");

    reporter.beginSuite("release-screenshot-explicit-frame-and-due-gate");
    const auto explicitFrame = parseReleaseScreenshotOptions(
        {"--release-screenshot-frame=17", "--quick-start-2p",
         "--release-screenshot=/tmp/two-player.png"});
    expect(explicitFrame.valid() && explicitFrame.options.requested() &&
               explicitFrame.options.frame == 17 &&
               explicitFrame.options.outputPath == "/tmp/two-player.png",
           "valid screenshot options became order-dependent");
    expect(!explicitFrame.options.due(16) &&
               explicitFrame.options.due(17) &&
               explicitFrame.options.due(18),
           "the rendered-frame capture gate changed");

    const auto boundaryFrames = parseReleaseScreenshotOptions(
        {"--release-screenshot=boundary.png",
         "--release-screenshot-frame=" +
             std::to_string(kMaximumReleaseScreenshotFrame)});
    expect(boundaryFrames.valid() &&
               boundaryFrames.options.frame ==
                   kMaximumReleaseScreenshotFrame,
           "the documented maximum capture frame was rejected");

    reporter.beginSuite("release-screenshot-output-validation");
    for (const std::vector<std::string> &arguments : {
             std::vector<std::string>{"--release-screenshot"},
             std::vector<std::string>{"--release-screenshot="},
             std::vector<std::string>{"--release-screenshot=shot.jpg"},
             std::vector<std::string>{
                 "--release-screenshot=bad\npath.png"},
         })
    {
        expect(!parseReleaseScreenshotOptions(arguments).valid(),
               "an invalid screenshot output path was accepted");
    }

    reporter.beginSuite("release-screenshot-frame-validation");
    for (const std::string &frame : {
             "", "0", "-1", "12x", "not-a-number", "3601",
             "999999999999999999999999",
         })
    {
        const auto invalidFrame = parseReleaseScreenshotOptions(
            {"--release-screenshot=shot.png",
             "--release-screenshot-frame=" + frame});
        expect(!invalidFrame.valid(),
               "an invalid screenshot frame was accepted: " + frame);
    }
    expect(!parseReleaseScreenshotOptions(
                {"--release-screenshot-frame"}).valid(),
           "a frame flag without a value was accepted");
    expect(!parseReleaseScreenshotOptions(
                {"--release-screenshot-frame=2"}).valid(),
           "a frame flag without an output path was accepted");

    reporter.beginSuite("release-screenshot-duplicate-validation");
    expect(!parseReleaseScreenshotOptions(
                {"--release-screenshot=one.png",
                 "--release-screenshot=two.png"}).valid(),
           "duplicate screenshot output flags were accepted");
    expect(!parseReleaseScreenshotOptions(
                {"--release-screenshot=one.png",
                 "--release-screenshot-frame=2",
                 "--release-screenshot-frame=3"}).valid(),
           "duplicate screenshot frame flags were accepted");

    TemporaryDirectory temporaryDirectory;
    reporter.beginSuite("release-screenshot-file-input-and-save");
    expect(temporaryDirectory.valid(),
           "a private screenshot test directory could not be created");
    if (!temporaryDirectory.valid())
    {
        reporter.finish();
        return 1;
    }

    const std::vector<unsigned char> screenshotBytes{
        0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU,
        0x42U, 0x17U};
    const fs::path output = temporaryDirectory.path() / "capture.png";
    expect(saveReleaseScreenshotFileNoReplace(
               {}, screenshotBytes.data(), screenshotBytes.size()).status ==
               ReleaseScreenshotFileStatus::InvalidInput,
           "an empty output path was accepted");
    expect(saveReleaseScreenshotFileNoReplace(
               output.string(), nullptr, screenshotBytes.size()).status ==
               ReleaseScreenshotFileStatus::InvalidInput,
           "null screenshot bytes were accepted");
    expect(saveReleaseScreenshotFileNoReplace(
               output.string(), screenshotBytes.data(), 0U).status ==
               ReleaseScreenshotFileStatus::InvalidInput,
           "an empty screenshot payload was accepted");
    const auto saved = saveReleaseScreenshotFileNoReplace(
        output.string(), screenshotBytes.data(), screenshotBytes.size());
    expect(saved.saved(), "a new screenshot file was not published");
    expect(readBytes(output) == screenshotBytes,
           "published screenshot bytes changed");

    reporter.beginSuite("release-screenshot-file-no-replace");
    const std::vector<unsigned char> replacementBytes{0x10U, 0x20U, 0x30U};
    const auto existing = saveReleaseScreenshotFileNoReplace(
        output.string(), replacementBytes.data(), replacementBytes.size());
    expect(existing.status == ReleaseScreenshotFileStatus::OutputExists,
           "an existing regular screenshot was replaced");
    expect(readBytes(output) == screenshotBytes,
           "an existing screenshot changed after a rejected write");

    PublishConflictContext conflict{
        temporaryDirectory.path() / "concurrent.png",
        {0xa1U, 0xb2U, 0xc3U}, false};
    const auto concurrent = saveReleaseScreenshotFileNoReplace(
        conflict.output.string(), screenshotBytes.data(), screenshotBytes.size(),
        createPublishConflict, &conflict);
    expect(conflict.created,
           "the deterministic pre-publish conflict was not created");
    expect(concurrent.status == ReleaseScreenshotFileStatus::OutputExists,
           "a concurrently created output was replaced");
    expect(readBytes(conflict.output) == conflict.bytes,
           "the concurrent output contents changed");

    const fs::path symlinkTarget = temporaryDirectory.path() / "target.txt";
    const fs::path symlinkOutput = temporaryDirectory.path() / "linked.png";
    const std::vector<unsigned char> targetBytes{0xdeU, 0xadU, 0xbeU, 0xefU};
    expect(writeBytes(symlinkTarget, targetBytes),
           "the symlink target fixture could not be written");
    std::error_code symlinkError;
    fs::create_symlink(symlinkTarget, symlinkOutput, symlinkError);
    expect(!symlinkError, "the screenshot symlink fixture could not be created");
    if (!symlinkError)
    {
        const auto symlinkResult = saveReleaseScreenshotFileNoReplace(
            symlinkOutput.string(), screenshotBytes.data(),
            screenshotBytes.size());
        expect(symlinkResult.status ==
                   ReleaseScreenshotFileStatus::OutputExists,
               "an existing screenshot symlink was followed or replaced");
        expect(readBytes(symlinkTarget) == targetBytes,
               "the screenshot symlink target changed");
    }

    reporter.beginSuite("release-screenshot-file-failure-cleanup");
    const fs::path missingOutput =
        temporaryDirectory.path() / "missing" / "capture.png";
    const auto missingParent = saveReleaseScreenshotFileNoReplace(
        missingOutput.string(), screenshotBytes.data(), screenshotBytes.size());
    expect(missingParent.status ==
               ReleaseScreenshotFileStatus::TemporaryFileError,
           "a screenshot with a missing parent directory was accepted");
    expect(!fs::exists(missingOutput),
           "a failed screenshot write left a final output");
    expect(!hasScreenshotTemporaryFile(temporaryDirectory.path()),
           "a screenshot operation left a temporary file behind");

    reporter.finish();
    return passed ? 0 : 1;
}

#include "app/release_screenshot_options.h"

#include "test_support.h"

#include <string>
#include <vector>

namespace
{
using tanks3d::app::kDefaultReleaseScreenshotFrame;
using tanks3d::app::kMaximumReleaseScreenshotFrame;
using tanks3d::app::kReleaseScreenshotHeight;
using tanks3d::app::kReleaseScreenshotWidth;
using tanks3d::app::parseReleaseScreenshotOptions;
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

    reporter.finish();
    return passed ? 0 : 1;
}

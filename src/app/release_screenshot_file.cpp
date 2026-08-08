#include "release_screenshot_file.h"

#include "atomic_output_file.h"

#include <utility>

namespace tanks3d::app
{
namespace
{
ReleaseScreenshotFileStatus screenshotStatus(AtomicOutputFileStatus status)
{
    switch (status)
    {
    case AtomicOutputFileStatus::Saved:
        return ReleaseScreenshotFileStatus::Saved;
    case AtomicOutputFileStatus::InvalidInput:
        return ReleaseScreenshotFileStatus::InvalidInput;
    case AtomicOutputFileStatus::TemporaryFileError:
        return ReleaseScreenshotFileStatus::TemporaryFileError;
    case AtomicOutputFileStatus::WriteError:
        return ReleaseScreenshotFileStatus::WriteError;
    case AtomicOutputFileStatus::OutputExists:
        return ReleaseScreenshotFileStatus::OutputExists;
    case AtomicOutputFileStatus::PublishError:
        return ReleaseScreenshotFileStatus::PublishError;
    }
    return ReleaseScreenshotFileStatus::PublishError;
}

} // namespace

ReleaseScreenshotFileResult saveReleaseScreenshotFileNoReplace(
    const std::string &outputPath, const unsigned char *data,
    std::size_t size, ReleaseScreenshotBeforePublish beforePublish,
    void *publishContext)
{
    AtomicOutputFileResult result = saveAtomicOutputFileNoReplace(
        outputPath, data, size, beforePublish, publishContext);
    return {screenshotStatus(result.status), std::move(result.message)};
}
} // namespace tanks3d::app

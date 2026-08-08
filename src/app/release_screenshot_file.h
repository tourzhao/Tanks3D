#ifndef TANKS3D_APP_RELEASE_SCREENSHOT_FILE_H
#define TANKS3D_APP_RELEASE_SCREENSHOT_FILE_H

#include <cstddef>
#include <string>

namespace tanks3d::app
{
enum class ReleaseScreenshotFileStatus
{
    Saved,
    InvalidInput,
    TemporaryFileError,
    WriteError,
    OutputExists,
    PublishError,
};

struct ReleaseScreenshotFileResult
{
    ReleaseScreenshotFileStatus status =
        ReleaseScreenshotFileStatus::InvalidInput;
    std::string message;

    bool saved() const
    {
        return status == ReleaseScreenshotFileStatus::Saved;
    }
};

using ReleaseScreenshotBeforePublish = void (*)(void *context);

// Writes encoded PNG bytes to a private same-directory temporary file, then
// publishes them with an atomic hard link. The final path is never replaced.
ReleaseScreenshotFileResult saveReleaseScreenshotFileNoReplace(
    const std::string &outputPath, const unsigned char *data,
    std::size_t size,
    ReleaseScreenshotBeforePublish beforePublish = nullptr,
    void *publishContext = nullptr);
} // namespace tanks3d::app

#endif

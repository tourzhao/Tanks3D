#ifndef TANKS3D_APP_ATOMIC_OUTPUT_FILE_H
#define TANKS3D_APP_ATOMIC_OUTPUT_FILE_H

#include <cstddef>
#include <string>

namespace tanks3d::app
{
enum class AtomicOutputFileStatus
{
    Saved,
    InvalidInput,
    TemporaryFileError,
    WriteError,
    OutputExists,
    PublishError,
};

struct AtomicOutputFileResult
{
    AtomicOutputFileStatus status = AtomicOutputFileStatus::InvalidInput;
    std::string message;

    bool saved() const
    {
        return status == AtomicOutputFileStatus::Saved;
    }
};

using AtomicOutputBeforePublish = void (*)(void *context);

// Writes bytes to a private same-directory temporary file, then publishes
// them with an atomic hard link. The final path is never replaced.
AtomicOutputFileResult saveAtomicOutputFileNoReplace(
    const std::string &outputPath, const void *data, std::size_t size,
    AtomicOutputBeforePublish beforePublish = nullptr,
    void *publishContext = nullptr);
} // namespace tanks3d::app

#endif

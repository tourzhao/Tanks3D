#include "atomic_output_file.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace tanks3d::app
{
namespace
{
namespace fs = std::filesystem;

std::string systemErrorMessage(const std::string &operation, int error)
{
    return operation + ": " +
           std::error_code(error, std::generic_category()).message();
}

class TemporaryFile
{
public:
    TemporaryFile(int descriptor, std::string path)
        : descriptor_(descriptor), path_(std::move(path))
    {
    }

    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;

    ~TemporaryFile()
    {
        if (descriptor_ >= 0)
            ::close(descriptor_);
        if (!path_.empty())
            ::unlink(path_.c_str());
    }

    int descriptor() const
    {
        return descriptor_;
    }

    const std::string &path() const
    {
        return path_;
    }

    int closeForPublish()
    {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        if (::close(descriptor) == 0)
            return 0;
        return errno == 0 ? EIO : errno;
    }

    int removeAfterPublish()
    {
        if (path_.empty())
            return 0;
        if (::unlink(path_.c_str()) == 0)
        {
            path_.clear();
            return 0;
        }
        return errno == 0 ? EIO : errno;
    }

private:
    int descriptor_ = -1;
    std::string path_;
};

AtomicOutputFileResult fileResult(AtomicOutputFileStatus status,
                                  std::string message)
{
    return {status, std::move(message)};
}
} // namespace

AtomicOutputFileResult saveAtomicOutputFileNoReplace(
    const std::string &outputPath, const void *data, std::size_t size,
    AtomicOutputBeforePublish beforePublish, void *publishContext)
{
    if (outputPath.empty() ||
        outputPath.find('\0') != std::string::npos || data == nullptr ||
        size == 0U)
    {
        return fileResult(AtomicOutputFileStatus::InvalidInput,
                          "output bytes or path are empty or invalid");
    }

    const fs::path destination{outputPath};
    const fs::path parent = destination.has_parent_path()
                                ? destination.parent_path()
                                : fs::path{"."};
    const fs::path temporaryTemplate =
        parent / ".tanks3d-atomic-output.XXXXXX";
    const std::string templateText = temporaryTemplate.string();
    std::vector<char> mutableTemplate(templateText.begin(),
                                      templateText.end());
    mutableTemplate.push_back('\0');

    const int descriptor = ::mkstemp(mutableTemplate.data());
    if (descriptor < 0)
    {
        const int error = errno == 0 ? EIO : errno;
        return fileResult(
            AtomicOutputFileStatus::TemporaryFileError,
            systemErrorMessage("unable to create output temporary file",
                               error));
    }
    TemporaryFile temporary{descriptor, mutableTemplate.data()};
    const auto *const bytes = static_cast<const unsigned char *>(data);

    std::size_t offset = 0U;
    while (offset < size)
    {
        const ssize_t written =
            ::write(temporary.descriptor(), bytes + offset, size - offset);
        if (written > 0)
        {
            offset += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        const int error = errno == 0 ? EIO : errno;
        return fileResult(
            AtomicOutputFileStatus::WriteError,
            systemErrorMessage("unable to write output temporary file",
                               error));
    }

    if (::fchmod(temporary.descriptor(), 0644) != 0)
    {
        const int error = errno == 0 ? EIO : errno;
        return fileResult(
            AtomicOutputFileStatus::WriteError,
            systemErrorMessage("unable to set output permissions", error));
    }
    if (::fsync(temporary.descriptor()) != 0)
    {
        const int error = errno == 0 ? EIO : errno;
        return fileResult(
            AtomicOutputFileStatus::WriteError,
            systemErrorMessage("unable to flush output temporary file",
                               error));
    }
    const int closeError = temporary.closeForPublish();
    if (closeError != 0)
    {
        return fileResult(
            AtomicOutputFileStatus::WriteError,
            systemErrorMessage("unable to close output temporary file",
                               closeError));
    }

    if (beforePublish != nullptr)
        beforePublish(publishContext);

    if (::link(temporary.path().c_str(), outputPath.c_str()) != 0)
    {
        const int error = errno == 0 ? EIO : errno;
        if (error == EEXIST)
        {
            return fileResult(AtomicOutputFileStatus::OutputExists,
                              "output already exists");
        }
        return fileResult(
            AtomicOutputFileStatus::PublishError,
            systemErrorMessage("unable to publish output", error));
    }

    const int cleanupError = temporary.removeAfterPublish();
    if (cleanupError != 0)
    {
        return fileResult(
            AtomicOutputFileStatus::Saved,
            systemErrorMessage(
                "output saved but temporary cleanup failed", cleanupError));
    }
    return fileResult(AtomicOutputFileStatus::Saved, {});
}
} // namespace tanks3d::app

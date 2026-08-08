#include "app/atomic_output_file.h"

#include "test_support.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>

namespace
{
namespace fs = std::filesystem;

using tanks3d::app::AtomicOutputFileStatus;
using tanks3d::app::saveAtomicOutputFileNoReplace;

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
                base / ("tanks3d-atomic-output-test." +
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

bool hasAtomicTemporaryFile(const fs::path &directory)
{
    std::error_code error;
    for (fs::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error))
    {
        const std::string name = iterator->path().filename().string();
        if (name.rfind(".tanks3d-atomic-output.", 0) == 0)
            return true;
    }
    return static_cast<bool>(error);
}

bool hasMode0644(const fs::path &path)
{
    struct stat information{};
    if (::stat(path.c_str(), &information) != 0)
        return false;
    return (information.st_mode & 0777) == 0644;
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
} // namespace

int main()
{
    tanks3d_test::Reporter reporter;
    reporter.reset();
    bool passed = true;
    const auto expect = [&](bool condition, const std::string &message) {
        passed = reporter.check(condition, message) && passed;
    };

    TemporaryDirectory temporaryDirectory;
    reporter.beginSuite("atomic-output-private-test-directory");
    expect(temporaryDirectory.valid(),
           "a private atomic-output test directory could not be created");
    if (!temporaryDirectory.valid())
    {
        reporter.finish();
        return 1;
    }

    const std::vector<unsigned char> originalBytes{
        0x00U, 0x01U, 0x7fU, 0x80U, 0xfeU, 0xffU};
    const std::vector<unsigned char> replacementBytes{
        0x10U, 0x20U, 0x30U};
    const fs::path output = temporaryDirectory.path() / "output.bin";

    reporter.beginSuite("atomic-output-input-validation");
    expect(saveAtomicOutputFileNoReplace(
               {}, originalBytes.data(), originalBytes.size()).status ==
               AtomicOutputFileStatus::InvalidInput,
           "an empty output path was accepted");
    expect(saveAtomicOutputFileNoReplace(
               output.string(), nullptr, originalBytes.size()).status ==
               AtomicOutputFileStatus::InvalidInput,
           "a null byte buffer was accepted");
    expect(saveAtomicOutputFileNoReplace(
               output.string(), originalBytes.data(), 0U).status ==
               AtomicOutputFileStatus::InvalidInput,
           "an empty byte payload was accepted");
    std::string nulPath = output.string();
    nulPath.push_back('\0');
    nulPath += ".ignored";
    expect(saveAtomicOutputFileNoReplace(
               nulPath, originalBytes.data(), originalBytes.size()).status ==
               AtomicOutputFileStatus::InvalidInput,
           "an output path containing an embedded NUL was accepted");
    expect(!fs::exists(output),
           "invalid input created a truncated output path");

    reporter.beginSuite("atomic-output-save-and-permissions");
    const auto saved = saveAtomicOutputFileNoReplace(
        output.string(), originalBytes.data(), originalBytes.size());
    expect(saved.saved(), "a new atomic output was not published");
    expect(saved.message.empty(),
           "a successful atomic output reported an unexpected warning");
    expect(readBytes(output) == originalBytes,
           "published atomic-output bytes changed");
    expect(hasMode0644(output),
           "published atomic output did not have mode 0644");

    reporter.beginSuite("atomic-output-existing-targets");
    const auto existingFile = saveAtomicOutputFileNoReplace(
        output.string(), replacementBytes.data(), replacementBytes.size());
    expect(existingFile.status == AtomicOutputFileStatus::OutputExists,
           "an existing regular output was not rejected");
    expect(readBytes(output) == originalBytes,
           "an existing regular output was modified");

    const fs::path directoryOutput =
        temporaryDirectory.path() / "existing-directory";
    std::error_code directoryError;
    fs::create_directory(directoryOutput, directoryError);
    expect(!directoryError, "the existing-directory fixture was not created");
    if (!directoryError)
    {
        const auto existingDirectory = saveAtomicOutputFileNoReplace(
            directoryOutput.string(), replacementBytes.data(),
            replacementBytes.size());
        expect(existingDirectory.status ==
                   AtomicOutputFileStatus::OutputExists,
               "an existing output directory was not rejected");
        expect(fs::is_directory(directoryOutput),
               "an existing output directory was modified");
    }

    const fs::path missingTarget =
        temporaryDirectory.path() / "missing-target.bin";
    const fs::path danglingOutput =
        temporaryDirectory.path() / "dangling-output.bin";
    std::error_code symlinkError;
    fs::create_symlink(missingTarget, danglingOutput, symlinkError);
    expect(!symlinkError, "the dangling-symlink fixture was not created");
    if (!symlinkError)
    {
        const auto danglingSymlink = saveAtomicOutputFileNoReplace(
            danglingOutput.string(), replacementBytes.data(),
            replacementBytes.size());
        expect(danglingSymlink.status ==
                   AtomicOutputFileStatus::OutputExists,
               "a dangling output symlink was followed or replaced");
        expect(fs::is_symlink(danglingOutput),
               "a dangling output symlink was modified");
        expect(!fs::exists(missingTarget),
               "the dangling symlink target was created or modified");
    }

    reporter.beginSuite("atomic-output-concurrent-no-replace");
    PublishConflictContext conflict{
        temporaryDirectory.path() / "concurrent.bin",
        {0xa1U, 0xb2U, 0xc3U}, false};
    const auto concurrent = saveAtomicOutputFileNoReplace(
        conflict.output.string(), originalBytes.data(), originalBytes.size(),
        createPublishConflict, &conflict);
    expect(conflict.created,
           "the deterministic pre-publish conflict was not created");
    expect(concurrent.status == AtomicOutputFileStatus::OutputExists,
           "a concurrently created output was replaced");
    expect(readBytes(conflict.output) == conflict.bytes,
           "the concurrently created output bytes changed");

    reporter.beginSuite("atomic-output-failure-cleanup");
    const fs::path missingParentOutput =
        temporaryDirectory.path() / "missing" / "output.bin";
    const auto missingParent = saveAtomicOutputFileNoReplace(
        missingParentOutput.string(), originalBytes.data(),
        originalBytes.size());
    expect(missingParent.status ==
               AtomicOutputFileStatus::TemporaryFileError,
           "an output with a missing parent directory was accepted");
    expect(!fs::exists(missingParentOutput),
           "a missing-parent failure left a final output");
    expect(!hasAtomicTemporaryFile(temporaryDirectory.path()),
           "an atomic output operation left a temporary file behind");
    expect(readBytes(output) == originalBytes,
           "a later failed operation modified the original output");

    reporter.finish();
    return passed ? 0 : 1;
}

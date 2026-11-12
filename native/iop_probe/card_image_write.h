#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace drakengard {
// Keep the lease on a stable sidecar because a commit replaces the image inode.
// The operating system releases the lease even when the process is terminated.
class CardImageWriter {
public:
    explicit CardImageWriter(const std::filesystem::path& path) : image(std::filesystem::canonical(path)) {
        if (!std::filesystem::is_regular_file(image))
            throw std::runtime_error("Writable memory card must be an existing regular file");
        auto lockPath = image;
        lockPath += ".drakengard.lock";
#ifdef _WIN32
        lock = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot acquire memory card writer lease");
#else
        lock = open(lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (lock == -1) throw std::runtime_error("Cannot open memory card writer lease");
        if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
            close(lock);
            throw std::runtime_error("Cannot acquire memory card writer lease");
        }
#endif
    }
    CardImageWriter(const CardImageWriter&) = delete;
    CardImageWriter& operator=(const CardImageWriter&) = delete;
    ~CardImageWriter() {
#ifdef _WIN32
        CloseHandle(lock);
#else
        close(lock);
#endif
    }
    const std::filesystem::path& path() const { return image; }

    void replace(std::span<const uint8_t> bytes) const {
        auto directory = image;
        directory += ".drakengard-stage-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Cannot create private memory card staging directory");
        const auto stage = directory / "image";
        struct Cleanup {
            const std::filesystem::path& stage;
            const std::filesystem::path& directory;
            ~Cleanup() {
                std::error_code ignored;
                std::filesystem::remove(stage, ignored);
                std::filesystem::remove(directory, ignored);
            }
        } cleanup{stage, directory};
        std::ofstream output(stage, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        output.close();
        if (!output) throw std::runtime_error("Cannot write complete staged memory card image");
        std::filesystem::rename(stage, image);
    }
private:
    std::filesystem::path image;
#ifdef _WIN32
    HANDLE lock{INVALID_HANDLE_VALUE};
#else
    int lock{-1};
#endif
};
}

#include "pad_control_file.h"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace drakengard {

std::size_t readPadControlFile(const char* path, PadControlFileBuffer& bytes) {
    std::size_t size{};
#ifdef _WIN32
    // Rename/replace requests DELETE access. CRT ifstream sharing denies it,
    // including when the renamer already has its handle open.
    const auto file = CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        throw std::system_error(static_cast<int>(error), std::system_category(),
                                "Cannot open bounded pad control command");
    }
    while (size < bytes.size()) {
        DWORD count{};
        if (!ReadFile(file, bytes.data() + size,
                      static_cast<DWORD>(bytes.size() - size), &count, nullptr)) {
            const auto error = GetLastError();
            CloseHandle(file);
            throw std::system_error(static_cast<int>(error), std::system_category(),
                                    "Cannot read bounded pad control command");
        }
        if (!count) break;
        size += count;
    }
    CloseHandle(file);
#else
    const int file = open(path, O_RDONLY | O_CLOEXEC);
    if (file < 0) {
        const int error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Cannot open bounded pad control command");
    }
    while (size < bytes.size()) {
        const auto count = read(file, bytes.data() + size, bytes.size() - size);
        if (count < 0) {
            const int error = errno;
            close(file);
            throw std::system_error(error, std::generic_category(),
                                    "Cannot read bounded pad control command");
        }
        if (!count) break;
        size += static_cast<std::size_t>(count);
    }
    close(file);
#endif
    if (size > PadControl::MaxCommandBytes)
        throw std::runtime_error("Pad control command exceeds its bounded size");
    return size;
}

} // namespace drakengard

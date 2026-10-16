#include "verify_input.h"
#include <array>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>

namespace {
struct HashHandles {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    ~HashHandles() {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }
};
}
#endif

bool verifyEuropeElf(const char* path) {
#ifdef _WIN32
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    HashHandles handles;
    if (BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    if (BCryptCreateHash(handles.algorithm, &handles.hash, nullptr, 0, nullptr, 0, 0) < 0)
        return false;
    std::array<unsigned char, 65536> buffer{};
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        auto size = static_cast<ULONG>(stream.gcount());
        if (size && BCryptHashData(handles.hash, buffer.data(), size, 0) < 0) return false;
    }
    if (!stream.eof()) return false;
    std::array<unsigned char, 32> digest{};
    if (BCryptFinishHash(handles.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        return false;
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result == "065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918";
#else
    // No silent bypass on a platform without this diagnostic input verifier.
    return false;
#endif
}

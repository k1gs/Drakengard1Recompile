#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace drakengard {
// Card-only MagicGate exchange. These credentials belong to the two virtual
// devices for one attachment. No console, physical-card or KELF keys are used.
// Packet contracts: PS2SDK CardAuth/MechaAuth; challenge chaining: LRPS2 MechaMg
// and the SD2PSX card implementation.
class VirtualCardAuth {
    using Block = std::array<uint8_t, 8>;
    using Key = std::array<uint8_t, 16>;
    enum class MechaStep { Idle, Started, Selected, Material, Challenge, FirstHalf,
                           ChallengeSent, FirstResponses, Verified, Failed };
    struct Card {
        Block iv{}, seed{}, nonce{};
        std::array<Block, 3> challenge{}, response{};
        unsigned nextMode{};
    } card;
    struct Mecha {
        Block iv{}, seed{}, nonce{}, random{};
        std::array<Block, 3> challenge{};
        MechaStep step{MechaStep::Idle};
        bool firstResponsesValid{};
    } mecha;
    Key rootKey{};
    Block challengeIv{}, identity{};

    static Block xorBlock(Block a, const Block& b) {
        for (size_t i = 0; i < a.size(); ++i) a[i] ^= b[i];
        return a;
    }
    static Block block(std::span<const uint8_t> input) {
        Block value;
        std::copy_n(input.begin(), value.size(), value.begin());
        return value;
    }
    static void randomBytes(std::span<uint8_t> bytes) {
#ifdef _WIN32
        if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0) return;
#endif
        throw std::runtime_error("Virtual memory card authentication needs a host random source");
    }
    static Block crypt(Key key, Block input, bool decrypt = false) {
#ifdef _WIN32
        struct Handles {
            BCRYPT_ALG_HANDLE provider{};
            BCRYPT_KEY_HANDLE key{};
            ~Handles() {
                if (key) BCryptDestroyKey(key);
                if (provider) BCryptCloseAlgorithmProvider(provider, 0);
            }
        } handles;
        wchar_t mode[] = BCRYPT_CHAIN_MODE_ECB;
        if (BCryptOpenAlgorithmProvider(&handles.provider, BCRYPT_3DES_112_ALGORITHM, nullptr, 0) >= 0 &&
            BCryptSetProperty(handles.provider, BCRYPT_CHAINING_MODE,
                              reinterpret_cast<PUCHAR>(mode), sizeof(mode), 0) >= 0 &&
            BCryptGenerateSymmetricKey(handles.provider, &handles.key, nullptr, 0, key.data(),
                                       static_cast<ULONG>(key.size()), 0) >= 0) {
            Block output;
            ULONG written{};
            const auto operation = decrypt ? BCryptDecrypt : BCryptEncrypt;
            if (operation(handles.key, input.data(), static_cast<ULONG>(input.size()), nullptr, nullptr, 0,
                          output.data(), static_cast<ULONG>(output.size()), &written, 0) >= 0 &&
                written == output.size()) return output;
        }
#endif
        throw std::runtime_error("Virtual memory card authentication needs the host 3DES provider");
    }
    Key sessionKey(const Block& iv, const Block& seed) const {
        const auto material = xorBlock(iv, seed);
        Block other = material;
        for (auto& byte : other) byte ^= 0xff;
        const auto left = crypt(rootKey, material);
        const auto right = crypt(rootKey, other);
        Key key;
        std::copy(left.begin(), left.end(), key.begin());
        std::copy(right.begin(), right.end(), key.begin() + left.size());
        return key;
    }
    bool makeCardResponse() {
        const auto key = sessionKey(card.iv, card.seed);
        if (crypt(key, card.challenge[2], true) != xorBlock(card.iv, card.challenge[1]) ||
            crypt(key, card.challenge[1], true) != xorBlock(card.nonce, card.challenge[0])) return false;
        const auto random = xorBlock(crypt(key, card.challenge[0], true), challengeIv);
        card.response[0] = crypt(key, xorBlock(card.nonce, challengeIv));
        card.response[1] = crypt(key, xorBlock(random, card.response[0]));
        card.response[2] = crypt(key, xorBlock(identity, card.response[1]));
        return true;
    }
    Block lastResponseBlock{};

public:
    VirtualCardAuth() {
        randomBytes(rootKey);
        randomBytes(challengeIv);
        randomBytes(identity);
        // Fail at attachment if the Windows provider cannot execute 3DES.
        (void)crypt(rootKey, challengeIv);
    }
    void reset() {
        card = {};
        mecha = {};
        lastResponseBlock = {};
    }
    std::vector<uint8_t> cdvd(uint8_t command, std::span<const uint8_t> args) {
        switch (command) {
        case 0x80:
            mecha = {};
            if (args.size() != 1 || args[0] != 0) break;
            mecha.step = MechaStep::Started;
            return {0};
        case 0x81:
            if (mecha.step != MechaStep::Started || args.size() != 1 || args[0] != 0) break;
            mecha.step = MechaStep::Selected;
            return {0};
        case 0x82:
            if (mecha.step != MechaStep::Selected || args.size() != 16) break;
            mecha.iv = block(args);
            mecha.seed = block(args.subspan(8));
            mecha.step = MechaStep::Material;
            return {0};
        case 0x83: {
            if (mecha.step != MechaStep::Material || args.size() != 8) break;
            mecha.nonce = block(args);
            randomBytes(mecha.random);
            const auto key = sessionKey(mecha.iv, mecha.seed);
            mecha.challenge[0] = crypt(key, xorBlock(challengeIv, mecha.random));
            mecha.challenge[1] = crypt(key, xorBlock(mecha.nonce, mecha.challenge[0]));
            mecha.challenge[2] = crypt(key, xorBlock(mecha.iv, mecha.challenge[1]));
            mecha.step = MechaStep::Challenge;
            return {0};
        }
        case 0x84: case 0x85: {
            const bool first = command == 0x84;
            if (!args.empty() || mecha.step != (first ? MechaStep::Challenge : MechaStep::FirstHalf)) break;
            std::vector<uint8_t> reply{0};
            if (first) {
                reply.insert(reply.end(), mecha.challenge[0].begin(), mecha.challenge[0].end());
                reply.insert(reply.end(), mecha.challenge[1].begin(), mecha.challenge[1].begin() + 4);
            } else {
                reply.insert(reply.end(), mecha.challenge[1].begin() + 4, mecha.challenge[1].end());
                reply.insert(reply.end(), mecha.challenge[2].begin(), mecha.challenge[2].end());
            }
            mecha.step = first ? MechaStep::FirstHalf : MechaStep::ChallengeSent;
            return reply;
        }
        case 0x86: {
            if (mecha.step != MechaStep::ChallengeSent || args.size() != 16) break;
            const auto key = sessionKey(mecha.iv, mecha.seed);
            const auto first = block(args);
            lastResponseBlock = block(args.subspan(8));
            mecha.firstResponsesValid =
                crypt(key, first, true) == xorBlock(mecha.nonce, challengeIv) &&
                crypt(key, lastResponseBlock, true) == xorBlock(mecha.random, first);
            mecha.step = MechaStep::FirstResponses;
            return {0};
        }
        case 0x87:
            if (mecha.step != MechaStep::FirstResponses || args.size() != 8) break;
            mecha.step = mecha.firstResponsesValid &&
                crypt(sessionKey(mecha.iv, mecha.seed), block(args), true) == xorBlock(identity, lastResponseBlock)
                ? MechaStep::Verified : MechaStep::Failed;
            return {0}; // Command received. The poll reports the verification result.
        case 0x8f:
            if (args.empty() && (mecha.step == MechaStep::Challenge || mecha.step == MechaStep::Verified))
                return {0};
            break;
        case 0x88:
            if (args.empty() && mecha.step == MechaStep::Verified) {
                mecha.step = MechaStep::Idle;
                return {0};
            }
            break;
        }
        mecha.step = MechaStep::Failed;
        return {0x80};
    }

    std::vector<uint8_t> serial(std::span<const uint8_t> tx, size_t capacity, uint8_t term) {
        if ((tx.size() != 5 && tx.size() != 14) || tx.size() > capacity)
            throw std::runtime_error("Malformed virtual MagicGate packet");
        std::vector<uint8_t> reply(tx.size());
        reply[reply.size() - 2] = 0x2b;
        reply.back() = term;
        const unsigned mode = tx[2];
        if (tx[1] == 0xf7) {
            if (mode != 1 || tx.size() != 5 || card.nextMode != 0) reply.back() = 0x66;
            return reply;
        }
        const bool read = mode == 1 || mode == 2 || mode == 4 || mode == 15 || mode == 17 || mode == 19;
        const bool write = mode == 6 || mode == 7 || mode == 11;
        if (mode > 20 || mode != card.nextMode || tx.size() != (read || write ? 14u : 5u)) {
            reply.back() = 0x66;
            return reply;
        }
        if (mode == 0) {
            randomBytes(card.iv);
            randomBytes(card.seed);
            randomBytes(card.nonce);
        }
        if (write) {
            Block value;
            uint8_t checksum{};
            for (size_t i = 0; i < value.size(); ++i) {
                value[i] = tx[10 - i];
                checksum ^= value[i];
            }
            if (checksum != tx[11]) {
                reply.back() = 0x66;
                return reply;
            }
            const size_t index = mode == 6 ? 2 : mode == 7 ? 1 : 0;
            card.challenge[index] = value;
        }
        if (mode == 12 && !makeCardResponse()) {
            reply.back() = 0x66;
            return reply;
        }
        if (read) {
            const auto& value = mode == 1 ? card.iv : mode == 2 ? card.seed : mode == 4 ? card.nonce :
                card.response[(mode - 15) / 2];
            reply[3] = 0x2b;
            reply[12] = 0;
            for (size_t i = 0; i < value.size(); ++i) {
                reply[4 + i] = value[7 - i];
                reply[12] ^= value[i];
            }
        }
        ++card.nextMode;
        return reply;
    }
};
}

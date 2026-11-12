#pragma once
#include "card_image_write.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace drakengard {
// Raw PS2 8 MiB image, including the 16 spare bytes per 512-byte page.
// Packet geometry follows PCSX2 v2.8.2 Sio2::Memcard / MemoryCardProtocol.
class Ps2MemoryCard {
public:
    static constexpr size_t PageBytes = 512 + 16;
    static constexpr size_t Pages = 0x4000;
    static constexpr size_t PagesPerBlock = 16;
    static constexpr size_t ImageBytes = PageBytes * Pages;
    static constexpr size_t MaxPacketBytes = 255 + 6;
    uint8_t currentTerminator() const { return terminator; }

    explicit Ps2MemoryCard(const std::filesystem::path& path, bool writable = false) {
        if (writable) writer.emplace(path);
        std::ifstream file(writer ? writer->path() : path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() != static_cast<std::streamoff>(ImageBytes))
            throw std::runtime_error("Memory card must be an existing raw 8 MiB PS2 image with spare bytes");
        image.resize(ImageBytes);
        if (!file.seekg(0).read(reinterpret_cast<char*>(image.data()), ImageBytes) ||
            file.peek() != std::char_traits<char>::eof() || file.bad())
            throw std::runtime_error("Could not read the complete memory card image");
    }

    std::vector<uint8_t> exchange(std::span<const uint8_t> tx, size_t capacity) {
        if (tx.size() < 2 || tx[0] != 0x81)
            throw std::runtime_error("Malformed memory card request");
        const auto reply = [&](size_t size) {
            if (size > capacity) throw std::runtime_error("Memory card response exceeds SIO2 capacity");
            return std::vector<uint8_t>(size);
        };
        const auto ready = [&](size_t size, uint8_t term) {
            auto response = reply(size);
            response[size - 2] = 0x2b;
            response.back() = term;
            return response;
        };
        switch (tx[1]) {
        case 0x11: // Probe an image actually present in this slot.
            return ready(4, terminator);
        case 0xf3: { // Reset the card-ready terminator. No challenge exchange.
            auto response = ready(5, 0x55);
            terminator = 0x55;
            operation = Operation::Idle;
            return response;
        }
        case 0x26: {
            auto response = reply(13);
            constexpr std::array<uint8_t, 8> specs{0x00, 0x02, 0x10, 0x00, 0x00, 0x40, 0x00, 0x00};
            response[2] = 0x2b;
            for (size_t i = 0; i < specs.size(); ++i) {
                response[3 + i] = specs[i];
                response[11] ^= specs[i];
            }
            response[12] = terminator;
            return response;
        }
        case 0x27: {
            if (tx.size() < 3) throw std::runtime_error("Truncated memory card terminator");
            auto response = ready(5, tx[2]);
            terminator = tx[2];
            return response;
        }
        case 0x28: {
            auto response = reply(5);
            response[2] = 0x2b;
            response[3] = response[4] = terminator;
            return response;
        }
        case 0x21: case 0x22: case 0x23: {
            if (tx[1] != 0x23) requireWriter();
            if (tx.size() < 7) throw std::runtime_error("Truncated memory card page address");
            uint32_t page = 0;
            uint8_t checksum = 0;
            for (unsigned i = 0; i < 4; ++i) {
                page |= uint32_t(tx[2 + i]) << (i * 8);
                checksum ^= tx[2 + i];
            }
            if (page >= Pages || checksum != tx[6])
                throw std::runtime_error("Invalid memory card page address or checksum");
            if (tx[1] == 0x21 && page % PagesPerBlock)
                throw std::runtime_error("Memory card erase requires an aligned block");
            auto response = ready(9, terminator);
            offset = size_t(page) * PageBytes;
            operation = tx[1] == 0x21 ? Operation::Erase : tx[1] == 0x22 ? Operation::Write : Operation::Read;
            programmed = 0;
            return response;
        }
        case 0x42: {
            requireWriter();
            if (operation != Operation::Write || tx.size() < 3)
                throw std::runtime_error("Memory card write requires a selected page and payload");
            const size_t length = tx[2];
            if (!length || length > PageBytes - programmed || tx.size() < length + 4)
                throw std::runtime_error("Invalid memory card write length");
            uint8_t checksum = 0;
            for (auto byte : tx.subspan(3, length)) checksum ^= byte;
            if (checksum != tx[3 + length])
                throw std::runtime_error("Invalid memory card write checksum");
            auto response = reply(length + 6);
            response[3] = 0x2b;
            response[4 + length] = checksum;
            response.back() = terminator;
            std::copy_n(tx.begin() + 3, length, pendingPage.begin() + programmed);
            programmed += length;
            return response;
        }
        case 0x43: {
            if (tx.size() < 3 || operation != Operation::Read || !tx[2] || tx[2] > image.size() - offset)
                throw std::runtime_error("Memory card read requires a selected page and in-range length");
            const size_t length = tx[2];
            auto response = reply(length + 6);
            response[3] = 0x2b;
            for (size_t i = 0; i < length; ++i) {
                const auto byte = image[offset + i];
                response[4 + i] = byte;
                response[4 + length] ^= byte;
            }
            response.back() = terminator;
            offset += length;
            return response;
        }
        case 0x52: {
            // MCMAN's three-clock fallback probe. An attached PS2 card has
            // a zero flag byte here. No PS1 address or data is transferred.
            if (tx.size() != 3) throw std::runtime_error("Unsupported PS1 read on a PS2 memory card");
            auto response = reply(3);
            response[2] = 0x5a;
            return response;
        }
        case 0xbf: {
            // The five-clock status query returns the live terminator.
            // SD2PSX drives FF on the third clock; no storage is accessed.
            if (tx.size() != 5) throw std::runtime_error("Invalid PS2 memory card status packet");
            auto response = ready(5, terminator);
            response[2] = 0xff;
            return response;
        }
        case 0x81: {
            if (operation != Operation::Read && operation != Operation::Write)
                throw std::runtime_error("No memory card transfer to finish");
            auto response = ready(4, terminator);
            if (operation == Operation::Write) {
                if (programmed != 512 && programmed != PageBytes)
                    throw std::runtime_error("Cannot commit an incomplete memory card page");
                auto next = image;
                for (size_t i = 0; i < programmed; ++i)
                    next[offset + i] &= pendingPage[i];
                commit(next);
            }
            operation = Operation::Idle;
            return response;
        }
        case 0x82: {
            requireWriter();
            if (operation != Operation::Erase) throw std::runtime_error("No memory card block selected for erase");
            auto response = ready(4, terminator);
            auto next = image;
            std::fill_n(next.begin() + offset, PageBytes * PagesPerBlock, uint8_t{0xff});
            commit(next);
            operation = Operation::Erased;
            return response;
        }
        case 0x12:
            if (operation != Operation::Erased) throw std::runtime_error("No completed memory card erase");
            return ready(4, terminator);
        default:
            throw std::runtime_error("Unsupported PS2 memory card command: " + std::to_string(tx[1]));
        }
    }
private:
    void requireWriter() const {
        if (!writer) throw std::runtime_error("PS2 memory card is read-only");
    }
    void commit(std::vector<uint8_t>& next) {
        if (next == image) return;
        writer->replace(next);
        image.swap(next);
    }
    enum class Operation { Idle, Read, Write, Erase, Erased };
    std::optional<CardImageWriter> writer;
    std::vector<uint8_t> image;
    std::array<uint8_t, PageBytes> pendingPage{};
    size_t offset{}, programmed{};
    Operation operation{Operation::Idle};
    uint8_t terminator{0x55};
};
}

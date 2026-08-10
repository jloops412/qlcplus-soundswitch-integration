#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace emberlights {
namespace {

inline constexpr std::array<std::uint32_t, 64> kSha256Constants{{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U}};

[[nodiscard]] constexpr std::uint32_t rotate_right(
    std::uint32_t value,
    std::uint32_t amount) noexcept {
    return (value >> amount) | (value << (32U - amount));
}

class Sha256 {
public:
    void update(const std::uint8_t* bytes, std::size_t count) noexcept {
        total_bytes_ += static_cast<std::uint64_t>(count);
        while (count > 0U) {
            const auto copied = std::min(count, block_.size() - block_size_);
            std::copy_n(bytes, copied, block_.data() + block_size_);
            block_size_ += copied;
            bytes += copied;
            count -= copied;
            if (block_size_ == block_.size()) {
                transform(block_.data());
                block_size_ = 0U;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 32> finish() noexcept {
        const auto bit_count = total_bytes_ * 8U;
        block_[block_size_++] = 0x80U;
        if (block_size_ > 56U) {
            std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                      block_.end(), std::uint8_t{0});
            transform(block_.data());
            block_size_ = 0U;
        }
        std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                  block_.begin() + 56, std::uint8_t{0});
        for (std::size_t index = 0; index < 8U; ++index) {
            block_[63U - index] = static_cast<std::uint8_t>(bit_count >> (index * 8U));
        }
        transform(block_.data());

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t word = 0; word < state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4U; ++byte) {
                digest[word * 4U + byte] = static_cast<std::uint8_t>(
                    state_[word] >> ((3U - byte) * 8U));
            }
        }
        return digest;
    }

private:
    void transform(const std::uint8_t* block) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            words[index] =
                (static_cast<std::uint32_t>(block[index * 4U]) << 24U) |
                (static_cast<std::uint32_t>(block[index * 4U + 1U]) << 16U) |
                (static_cast<std::uint32_t>(block[index * 4U + 2U]) << 8U) |
                static_cast<std::uint32_t>(block[index * 4U + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto first = rotate_right(words[index - 15U], 7U) ^
                rotate_right(words[index - 15U], 18U) ^
                (words[index - 15U] >> 3U);
            const auto second = rotate_right(words[index - 2U], 17U) ^
                rotate_right(words[index - 2U], 19U) ^
                (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + first + words[index - 7U] + second;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto sum_one = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
                rotate_right(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temporary_one = h + sum_one + choice + kSha256Constants[index] + words[index];
            const auto sum_zero = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
                rotate_right(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary_two = sum_zero + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary_one;
            d = c;
            c = b;
            b = a;
            a = temporary_one + temporary_two;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{{
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U}};
    std::array<std::uint8_t, 64> block_{};
    std::size_t block_size_{0};
    std::uint64_t total_bytes_{0};
};

[[nodiscard]] std::string hexadecimal(const std::array<std::uint8_t, 32>& digest) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(digest.size() * 2U);
    for (const auto byte : digest) {
        encoded.push_back(digits[byte >> 4U]);
        encoded.push_back(digits[byte & 0x0FU]);
    }
    return encoded;
}

}  // namespace

FileIdentityResult identify_file_sha256(
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes) {
    FileIdentityResult result;
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(path, filesystem_error) || filesystem_error) {
        result.message = "The selected path is not a readable regular file.";
        return result;
    }
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        result.message = "Unable to determine the file size.";
        return result;
    }
    if (size > maximum_bytes) {
        result.message = "The file exceeds EmberLights' safe inspection limit.";
        return result;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.message = "Unable to open the file for read-only inspection.";
        return result;
    }
    Sha256 hash;
    std::array<std::uint8_t, 64U * 1024U> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto read = input.gcount();
        if (read < 0) {
            result.message = "The file returned an invalid read length.";
            return result;
        }
        const auto count = static_cast<std::size_t>(read);
        if (result.header_size < result.header.size()) {
            const auto header_count = std::min(count, result.header.size() - result.header_size);
            std::copy_n(buffer.data(), header_count, result.header.data() + result.header_size);
            result.header_size += header_count;
        }
        hash.update(buffer.data(), count);
        result.bytes += static_cast<std::uint64_t>(count);
    }
    if (!input.eof() || result.bytes != size) {
        result.message = "The complete file could not be read.";
        return result;
    }
    result.sha256 = hexadecimal(hash.finish());
    result.success = true;
    return result;
}

bool is_sha256_digest(std::string_view value) noexcept {
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f');
    });
}

}  // namespace emberlights

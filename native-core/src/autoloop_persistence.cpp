#include "emberlights/autoloop_persistence.hpp"

#include "emberlights/file_identity.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

[[nodiscard]] bool hex_value(char character, std::uint8_t& value) noexcept {
    if (character >= '0' && character <= '9') {
        value = static_cast<std::uint8_t>(character - '0');
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint8_t>(10 + character - 'A');
        return true;
    }
    if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint8_t>(10 + character - 'a');
        return true;
    }
    return false;
}

[[nodiscard]] char hex_digit(std::uint8_t value) noexcept {
    constexpr std::string_view digits = "0123456789abcdef";
    return digits[value & 0x0FU];
}

[[nodiscard]] std::string hex_encode(std::string_view bytes) {
    std::string encoded;
    encoded.reserve(bytes.size() * 2U);
    for (const auto character : bytes) {
        const auto byte = static_cast<std::uint8_t>(character);
        encoded.push_back(hex_digit(static_cast<std::uint8_t>(byte >> 4U)));
        encoded.push_back(hex_digit(byte));
    }
    return encoded;
}

[[nodiscard]] bool hex_decode(
    std::string_view encoded,
    std::string& decoded) {
    if ((encoded.size() & 1U) != 0U) {
        return false;
    }
    decoded.clear();
    decoded.reserve(encoded.size() / 2U);
    for (std::size_t index = 0U; index < encoded.size(); index += 2U) {
        std::uint8_t high = 0U;
        std::uint8_t low = 0U;
        if (!hex_value(encoded[index], high) ||
            !hex_value(encoded[index + 1U], low)) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4U) | low));
    }
    return true;
}

[[nodiscard]] bool decode_project_field(
    std::string_view encoded,
    std::string& decoded) {
    decoded.clear();
    decoded.reserve(encoded.size());
    for (std::size_t index = 0U; index < encoded.size(); ++index) {
        if (encoded[index] != '%') {
            decoded.push_back(encoded[index]);
            continue;
        }
        if (index + 2U >= encoded.size()) {
            return false;
        }
        std::uint8_t high = 0U;
        std::uint8_t low = 0U;
        if (!hex_value(encoded[index + 1U], high) ||
            !hex_value(encoded[index + 2U], low)) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4U) | low));
        index += 2U;
    }
    return true;
}

template <typename Value>
[[nodiscard]] bool parse_number(
    std::string_view text,
    Value& value) noexcept {
    if (text.empty()) {
        return false;
    }
    Value parsed{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size()) {
        return false;
    }
    value = parsed;
    return true;
}

template <typename Value>
[[nodiscard]] std::string number_text(Value value) {
    std::array<char, 32U> buffer{};
    const auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    return result.ec == std::errc{}
        ? std::string(buffer.data(), result.ptr)
        : std::string{};
}

[[nodiscard]] std::vector<std::string_view> split_fields(
    std::string_view record) {
    std::vector<std::string_view> fields;
    std::size_t offset = 0U;
    while (offset <= record.size()) {
        const auto tab = record.find('\t', offset);
        const auto end = tab == std::string_view::npos ? record.size() : tab;
        fields.push_back(record.substr(offset, end - offset));
        if (tab == std::string_view::npos) {
            break;
        }
        offset = tab + 1U;
    }
    return fields;
}

[[nodiscard]] bool recognized_record_line(std::string_view line) {
    const auto tab = line.find('\t');
    const auto encoded_kind = line.substr(
        0U, tab == std::string_view::npos ? line.size() : tab);
    std::string decoded_kind;
    return decode_project_field(encoded_kind, decoded_kind) &&
        decoded_kind == kPersistedAutoloopSourceRecordKind;
}

[[nodiscard]] bool contains_recognized_record_line(
    std::string_view carrier) {
    std::size_t offset = 0U;
    while (offset <= carrier.size()) {
        const auto end = carrier.find_first_of("\r\n", offset);
        const auto line_end = end == std::string_view::npos
            ? carrier.size() : end;
        if (recognized_record_line(
                carrier.substr(offset, line_end - offset))) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
        if (carrier[end] == '\r' && offset < carrier.size() &&
            carrier[offset] == '\n') {
            ++offset;
        }
    }
    return false;
}

[[nodiscard]] PersistedAutoloopSourceResult persistence_error(
    AutoloopPersistenceError error,
    std::string message,
    std::size_t record_index = kNoPersistedAutoloopSourceRecord) {
    PersistedAutoloopSourceResult result;
    result.error = error;
    result.record_index = record_index;
    result.message = std::move(message);
    return result;
}

[[nodiscard]] PersistedAutoloopSourceResult inspect_records(
    const std::vector<std::string>& records) {
    std::size_t record_index = kNoPersistedAutoloopSourceRecord;
    std::string_view record;
    for (std::size_t index = 0U; index < records.size(); ++index) {
        const auto& candidate = records[index];
        if (candidate.find_first_of("\r\n") != std::string::npos) {
            if (contains_recognized_record_line(candidate)) {
                return persistence_error(
                    AutoloopPersistenceError::MalformedRecord,
                    "The persisted Autoloop source record must occupy one canonical project record.",
                    index);
            }
            continue;
        }
        if (!recognized_record_line(candidate)) {
            continue;
        }
        if (record_index != kNoPersistedAutoloopSourceRecord) {
            return persistence_error(
                AutoloopPersistenceError::DuplicateRecord,
                "The project contains more than one persisted Autoloop source record.",
                index);
        }
        record_index = index;
        record = candidate;
    }

    if (record_index == kNoPersistedAutoloopSourceRecord) {
        return {};
    }
    if (record.size() > (kMaximumPersistedAutoloopSourceBytes * 2U) + 256U) {
        return persistence_error(
            AutoloopPersistenceError::SourceTooLarge,
            "The persisted Autoloop source record exceeds its bounded size.",
            record_index);
    }
    if (record.find('%') != std::string_view::npos) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source envelope is not canonical.",
            record_index);
    }

    const auto fields = split_fields(record);
    if (fields.size() != 6U ||
        fields.front() != kPersistedAutoloopSourceRecordKind) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source record has malformed fields.",
            record_index);
    }

    std::uint32_t record_version = 0U;
    std::uint32_t source_version = 0U;
    std::uint64_t source_size = 0U;
    if (!parse_number(fields[1], record_version) ||
        !parse_number(fields[2], source_version) ||
        !parse_number(fields[3], source_size)) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source version or size is malformed.",
            record_index);
    }
    if (fields[1] != number_text(record_version) ||
        fields[2] != number_text(source_version) ||
        fields[3] != number_text(source_size)) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source numeric envelope is not canonical.",
            record_index);
    }
    if (record_version != kPersistedAutoloopSourceRecordVersion) {
        return persistence_error(
            AutoloopPersistenceError::UnsupportedRecordVersion,
            "The persisted Autoloop source record version is unsupported.",
            record_index);
    }
    if (source_version != kAutoloopSourceFormatVersion) {
        return persistence_error(
            AutoloopPersistenceError::UnsupportedSourceVersion,
            "The persisted Autoloop source format version is unsupported.",
            record_index);
    }
    if (source_size > kMaximumPersistedAutoloopSourceBytes) {
        return persistence_error(
            AutoloopPersistenceError::SourceTooLarge,
            "The persisted Autoloop source exceeds its bounded size.",
            record_index);
    }
    const auto bounded_size = static_cast<std::size_t>(source_size);
    if (fields[5].size() != bounded_size * 2U) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source byte count does not match its payload.",
            record_index);
    }
    if (!is_sha256_digest(fields[4])) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source digest is malformed.",
            record_index);
    }

    std::string serialized_source;
    if (!hex_decode(fields[5], serialized_source) ||
        serialized_source.size() != bounded_size) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source payload encoding is malformed.",
            record_index);
    }
    if (hex_encode(serialized_source) != fields[5]) {
        return persistence_error(
            AutoloopPersistenceError::MalformedRecord,
            "The persisted Autoloop source payload encoding is not canonical.",
            record_index);
    }
    if (sha256_text(serialized_source) != fields[4]) {
        return persistence_error(
            AutoloopPersistenceError::DigestMismatch,
            "The persisted Autoloop source digest does not match its payload.",
            record_index);
    }

    AutoloopSourceDocument source;
    const auto parsed = parse_autoloop_source(serialized_source, source);
    if (!parsed) {
        return persistence_error(
            AutoloopPersistenceError::InvalidSource,
            "The persisted Autoloop source payload is invalid: " + parsed.message,
            record_index);
    }
    if (source.format_version != source_version) {
        return persistence_error(
            AutoloopPersistenceError::UnsupportedSourceVersion,
            "The persisted source envelope and payload versions disagree.",
            record_index);
    }
    if (serialize_autoloop_source(source) != serialized_source) {
        return persistence_error(
            AutoloopPersistenceError::NonCanonicalSource,
            "The persisted Autoloop source payload is not canonical.",
            record_index);
    }

    PersistedAutoloopSourceResult result;
    result.record_index = record_index;
    result.stamp = {
        true, record_version, source_version, std::string(fields[4])};
    result.source = std::move(source);
    result.message = "The canonical persisted Autoloop source is valid.";
    return result;
}

[[nodiscard]] std::string make_record(
    std::string_view serialized_source,
    std::string_view digest) {
    std::string record;
    record.reserve(serialized_source.size() * 2U + 160U);
    record.append(kPersistedAutoloopSourceRecordKind);
    record.push_back('\t');
    record.append(number_text(kPersistedAutoloopSourceRecordVersion));
    record.push_back('\t');
    record.append(number_text(kAutoloopSourceFormatVersion));
    record.push_back('\t');
    record.append(number_text(serialized_source.size()));
    record.push_back('\t');
    record.append(digest);
    record.push_back('\t');
    record.append(hex_encode(serialized_source));
    return record;
}

}  // namespace

PersistedAutoloopSourceResult inspect_persisted_autoloop_source(
    const ProjectDocument& project) {
    return inspect_records(project.unknown_records);
}

PersistedAutoloopSourceResult upsert_persisted_autoloop_source(
    ProjectDocument& project,
    const AutoloopSourceDocument& source) {
    const auto existing = inspect_records(project.unknown_records);
    if (!existing) {
        return existing;
    }
    if (source.format_version != kAutoloopSourceFormatVersion) {
        return persistence_error(
            AutoloopPersistenceError::UnsupportedSourceVersion,
            "The candidate Autoloop source format version is unsupported.");
    }
    const auto serialized_source = serialize_autoloop_source(source);
    if (serialized_source.empty()) {
        return persistence_error(
            AutoloopPersistenceError::InvalidSource,
            "The candidate Autoloop source failed canonical validation.");
    }
    if (serialized_source.size() > kMaximumPersistedAutoloopSourceBytes) {
        return persistence_error(
            AutoloopPersistenceError::SourceTooLarge,
            "The candidate Autoloop source exceeds its bounded size.");
    }

    const auto record = make_record(
        serialized_source, sha256_text(serialized_source));
    auto candidate_records = project.unknown_records;
    bool changed = false;
    if (existing.stamp.present) {
        changed = candidate_records[existing.record_index] != record;
        candidate_records[existing.record_index] = record;
    } else {
        candidate_records.push_back(record);
        changed = true;
    }

    auto verified = inspect_records(candidate_records);
    if (!verified) {
        return verified;
    }
    verified.changed = changed;
    verified.message = changed
        ? "The canonical Autoloop source record was updated."
        : "The canonical Autoloop source record is unchanged.";
    if (changed) {
        project.unknown_records = std::move(candidate_records);
    }
    return verified;
}

const char* autoloop_persistence_error_name(
    AutoloopPersistenceError error) noexcept {
    switch (error) {
    case AutoloopPersistenceError::None: return "none";
    case AutoloopPersistenceError::DuplicateRecord: return "duplicateRecord";
    case AutoloopPersistenceError::MalformedRecord: return "malformedRecord";
    case AutoloopPersistenceError::UnsupportedRecordVersion:
        return "unsupportedRecordVersion";
    case AutoloopPersistenceError::UnsupportedSourceVersion:
        return "unsupportedSourceVersion";
    case AutoloopPersistenceError::SourceTooLarge: return "sourceTooLarge";
    case AutoloopPersistenceError::DigestMismatch: return "digestMismatch";
    case AutoloopPersistenceError::NonCanonicalSource:
        return "nonCanonicalSource";
    case AutoloopPersistenceError::InvalidSource: return "invalidSource";
    }
    return "invalidSource";
}

}  // namespace emberlights

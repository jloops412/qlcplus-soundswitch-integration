#include "emberlights/raw_hardware_test_operator.hpp"

#include "emberlights/file_identity.hpp"
#include "emberlights/project_io.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

constexpr std::uintmax_t kMaximumManifestBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumAuditBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaximumPathBytes = 4096U;
constexpr std::size_t kMaximumTextBytes = 1024U;
constexpr std::size_t kMaximumCriteria = 512U;
constexpr std::size_t kMaximumMarkers = 64U;

[[nodiscard]] RawHardwareTestOperatorCheck check(
    RawHardwareTestOperatorError error,
    std::size_t line,
    std::string message) {
    return {error, line, std::move(message)};
}

[[nodiscard]] bool bounded_plain_text(
    std::string_view value,
    std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum &&
        std::none_of(value.begin(), value.end(), [](char character) {
            const auto byte = static_cast<unsigned char>(character);
            return byte == 0U || byte == '\r' || byte == '\n' || byte == '\t';
        });
}

[[nodiscard]] bool bounded_marker(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 4096U &&
        std::none_of(value.begin(), value.end(), [](char character) {
            return character == '\0' || character == '\r' || character == '\n';
        });
}

template <typename Integer>
[[nodiscard]] bool parse_integer(std::string_view text, Integer& value) noexcept {
    if (text.empty()) {
        return false;
    }
    unsigned long long parsed = 0U;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > static_cast<unsigned long long>(
                     std::numeric_limits<Integer>::max())) {
        return false;
    }
    value = static_cast<Integer>(parsed);
    return true;
}

[[nodiscard]] std::vector<std::string_view> fields(std::string_view line) {
    std::vector<std::string_view> result;
    while (true) {
        const auto separator = line.find('\t');
        result.push_back(line.substr(0U, separator));
        if (separator == std::string_view::npos) {
            return result;
        }
        line.remove_prefix(separator + 1U);
    }
}

[[nodiscard]] std::filesystem::path utf8_path(std::string_view value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        encoded.push_back(static_cast<char8_t>(
            static_cast<unsigned char>(character)));
    }
    return std::filesystem::path(encoded);
}

[[nodiscard]] bool normalized_path(
    const std::filesystem::path& source_directory,
    std::string_view text,
    std::filesystem::path& output) {
    if (text.empty() || text.size() > kMaximumPathBytes ||
        std::find(text.begin(), text.end(), '\0') != text.end()) {
        return false;
    }
    auto path = utf8_path(text);
    if (path.empty()) {
        return false;
    }
    if (path.is_relative()) {
        path = source_directory / path;
    }
    std::error_code error;
    path = std::filesystem::absolute(path, error).lexically_normal();
    if (error || path.empty()) {
        return false;
    }
    output = std::move(path);
    return true;
}

[[nodiscard]] RawHardwareTestOperatorCheck read_bounded_file(
    const std::filesystem::path& path,
    std::uintmax_t maximum,
    std::string& text) {
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        return check(
            RawHardwareTestOperatorError::ReadFailed,
            0U,
            "The requested operator file could not be sized.");
    }
    if (size > maximum) {
        return check(
            RawHardwareTestOperatorError::TooLarge,
            0U,
            "The requested operator file exceeds its bounded size.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return check(
            RawHardwareTestOperatorError::ReadFailed,
            0U,
            "The requested operator file could not be opened.");
    }
    text.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    if (input.bad() || text.size() != size) {
        text.clear();
        return check(
            RawHardwareTestOperatorError::ReadFailed,
            0U,
            "The requested operator file could not be read completely.");
    }
    return {};
}

[[nodiscard]] bool soundswitch_binding(std::string_view binding) noexcept {
    constexpr std::string_view prefix = "soundswitch-micro:u";
    if (!binding.starts_with(prefix)) {
        return false;
    }
    std::uint16_t universe = 0U;
    return parse_integer(binding.substr(prefix.size()), universe) &&
        universe > 0U && universe <= showcore::kV1UniverseCount;
}

[[nodiscard]] RawHardwareTestOperatorCheck validate_paths(
    const RawHardwareTestOperatorManifest& manifest) {
    if (manifest.project_path.empty() || manifest.graduated_project_path.empty() ||
        manifest.audit_path.empty() ||
        !manifest.project_path.is_absolute() ||
        !manifest.graduated_project_path.is_absolute() ||
        !manifest.audit_path.is_absolute() ||
        manifest.project_path == manifest.graduated_project_path ||
        manifest.project_path == manifest.audit_path ||
        manifest.graduated_project_path == manifest.audit_path) {
        return check(
            RawHardwareTestOperatorError::InvalidPath,
            0U,
            "Project, graduated-project, and audit paths must be three distinct absolute paths.");
    }
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(
            manifest.project_path, filesystem_error) || filesystem_error) {
        return check(
            RawHardwareTestOperatorError::InvalidPath,
            0U,
            "The candidate project path is not a readable regular file.");
    }
    filesystem_error.clear();
    if (std::filesystem::exists(
            manifest.graduated_project_path, filesystem_error) ||
        filesystem_error) {
        return check(
            RawHardwareTestOperatorError::InvalidPath,
            0U,
            "The graduated-project path already exists or cannot be inspected; no file will be overwritten.");
    }
    for (const auto& destination : {
             manifest.graduated_project_path, manifest.audit_path}) {
        const auto parent = destination.parent_path();
        filesystem_error.clear();
        if (parent.empty() ||
            !std::filesystem::is_directory(parent, filesystem_error) ||
            filesystem_error) {
            return check(
                RawHardwareTestOperatorError::InvalidPath,
                0U,
                "Every operator output path must have an existing directory.");
        }
    }
    return {};
}

[[nodiscard]] std::string audit_header() {
    return std::string(kRawHardwareTestOperatorAuditHeader) + "\t" +
        std::to_string(kRawHardwareTestOperatorAuditVersion);
}

[[nodiscard]] RawHardwareTestOperatorCheck inspect_audit(
    std::string_view text,
    std::unordered_set<std::string>& digests) {
    std::size_t line_number = 0U;
    std::size_t cursor = 0U;
    while (cursor <= text.size()) {
        const auto end = text.find('\n', cursor);
        auto line = text.substr(
            cursor,
            end == std::string_view::npos ? text.size() - cursor : end - cursor);
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (line_number == 1U) {
            if (line != audit_header()) {
                return check(
                    RawHardwareTestOperatorError::AuditInvalid,
                    line_number,
                    "The append-only audit header or version is invalid.");
            }
        } else if (!line.empty()) {
            RawHardwareTestAttempt attempt;
            const auto parsed = parse_raw_hardware_test_attempt_record(line, attempt);
            auto resealed = attempt;
            resealed.content_sha256.clear();
            const auto sealed = parsed.ok()
                ? seal_raw_hardware_test_attempt(resealed)
                : RawHardwareTestCheck{
                      RawHardwareTestError::InvalidAuditRecord, {}, {}};
            if (!parsed.ok() || !sealed.ok() ||
                resealed.content_sha256 != attempt.content_sha256 ||
                !digests.insert(attempt.content_sha256).second) {
                return check(
                    RawHardwareTestOperatorError::AuditInvalid,
                    line_number,
                    "The append-only audit contains a malformed or replayed attempt.");
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        cursor = end + 1U;
    }
    return {};
}

[[nodiscard]] RawHardwareTestOperatorCheck append_attempt(
    const std::filesystem::path& path,
    const RawHardwareTestAttempt& attempt) {
    std::error_code filesystem_error;
    const bool exists = std::filesystem::exists(path, filesystem_error);
    if (filesystem_error) {
        return check(
            RawHardwareTestOperatorError::AuditWriteFailed,
            0U,
            "The audit destination could not be inspected before append.");
    }
    std::string current;
    std::unordered_set<std::string> digests;
    if (exists) {
        const auto read = read_bounded_file(path, kMaximumAuditBytes, current);
        if (!read.ok()) {
            return check(
                RawHardwareTestOperatorError::AuditInvalid,
                read.line,
                read.message);
        }
        const auto inspected = inspect_audit(current, digests);
        if (!inspected.ok()) {
            return inspected;
        }
    }
    if (digests.contains(attempt.content_sha256)) {
        return check(
            RawHardwareTestOperatorError::AuditInvalid,
            0U,
            "The terminal attempt is already present in the append-only audit.");
    }
    const auto record = serialize_raw_hardware_test_attempt_record(attempt);
    if (current.size() + record.size() + audit_header().size() + 3U >
        kMaximumAuditBytes) {
        return check(
            RawHardwareTestOperatorError::TooLarge,
            0U,
            "Appending this attempt would exceed the bounded audit size.");
    }
    std::ofstream output(path, std::ios::binary | std::ios::app);
    if (!output) {
        return check(
            RawHardwareTestOperatorError::AuditWriteFailed,
            0U,
            "The terminal attempt audit could not be opened for append.");
    }
    if (!exists) {
        output << audit_header() << '\n';
    } else if (!current.empty() && current.back() != '\n') {
        output << '\n';
    }
    output << record << '\n';
    output.flush();
    if (!output) {
        return check(
            RawHardwareTestOperatorError::AuditWriteFailed,
            0U,
            "The terminal attempt audit append did not complete.");
    }
    output.close();
    const auto verified = validate_raw_hardware_test_operator_audit(path);
    if (!verified.ok()) {
        return check(
            RawHardwareTestOperatorError::AuditWriteFailed,
            verified.line,
            "The terminal attempt was written but the append-only audit did not verify.");
    }
    return {};
}

}  // namespace

const char* raw_hardware_test_operator_error_name(
    RawHardwareTestOperatorError error) noexcept {
    switch (error) {
    case RawHardwareTestOperatorError::None: return "none";
    case RawHardwareTestOperatorError::ReadFailed: return "read-failed";
    case RawHardwareTestOperatorError::TooLarge: return "too-large";
    case RawHardwareTestOperatorError::InvalidHeader: return "invalid-header";
    case RawHardwareTestOperatorError::InvalidField: return "invalid-field";
    case RawHardwareTestOperatorError::DuplicateField: return "duplicate-field";
    case RawHardwareTestOperatorError::MissingField: return "missing-field";
    case RawHardwareTestOperatorError::InvalidPath: return "invalid-path";
    case RawHardwareTestOperatorError::InvalidProject: return "invalid-project";
    case RawHardwareTestOperatorError::InvalidPlan: return "invalid-plan";
    case RawHardwareTestOperatorError::SessionIncomplete:
        return "session-incomplete";
    case RawHardwareTestOperatorError::AuditInvalid: return "audit-invalid";
    case RawHardwareTestOperatorError::AuditWriteFailed:
        return "audit-write-failed";
    case RawHardwareTestOperatorError::GraduationRejected:
        return "graduation-rejected";
    case RawHardwareTestOperatorError::GraduationWriteFailed:
        return "graduation-write-failed";
    }
    return "unknown";
}

RawHardwareTestOperatorCheck parse_raw_hardware_test_operator_manifest(
    std::string_view text,
    const std::filesystem::path& source_directory,
    RawHardwareTestOperatorManifest& manifest) {
    if (text.empty() || text.size() > kMaximumManifestBytes) {
        return check(
            text.empty() ? RawHardwareTestOperatorError::InvalidHeader
                         : RawHardwareTestOperatorError::TooLarge,
            0U,
            "The operator manifest is empty or exceeds its bounded size.");
    }

    RawHardwareTestOperatorManifest parsed;
    bool project_seen = false;
    bool graduated_seen = false;
    bool audit_seen = false;
    bool input_sha_seen = false;
    bool fixture_seen = false;
    bool unit_seen = false;
    bool backend_seen = false;
    bool operator_seen = false;
    bool observation_timeout_seen = false;
    bool session_timeout_seen = false;
    bool repetitions_seen = false;
    std::size_t cursor = 0U;
    std::size_t line_number = 0U;
    while (cursor <= text.size()) {
        const auto end = text.find('\n', cursor);
        auto line = text.substr(
            cursor,
            end == std::string_view::npos ? text.size() - cursor : end - cursor);
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (line_number == 1U) {
            const auto header = fields(line);
            if (header.size() != 2U ||
                header[0] != kRawHardwareTestOperatorManifestHeader ||
                header[1] != "1") {
                return check(
                    RawHardwareTestOperatorError::InvalidHeader,
                    line_number,
                    "The operator manifest header or version is invalid.");
            }
        } else if (!line.empty()) {
            const auto parts = fields(line);
            const auto duplicate = [&](bool& seen) {
                if (seen) {
                    return true;
                }
                seen = true;
                return false;
            };
            if (parts[0] == "marker") {
                constexpr std::string_view prefix = "marker\t";
                const auto value = line.substr(prefix.size());
                if (parts.size() < 2U || !bounded_marker(value) ||
                    parsed.markers_to_supersede.size() >= kMaximumMarkers) {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "A marker field is malformed or exceeds the bounded count.");
                }
                parsed.markers_to_supersede.emplace_back(value);
            } else if (parts[0] == "criterion") {
                if (parts.size() != 4U ||
                    parsed.criteria.size() >= kMaximumCriteria) {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "A criterion must contain slot, nonzero value, and expected behavior.");
                }
                RawHardwareTestSlotCriterion criterion;
                if (!parse_integer(parts[1], criterion.relative_slot) ||
                    !parse_integer(parts[2], criterion.one_hot_value) ||
                    criterion.one_hot_value == 0U ||
                    !bounded_plain_text(parts[3], kMaximumTextBytes)) {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "A criterion slot, value, or expected behavior is invalid.");
                }
                criterion.expected_behavior = std::string(parts[3]);
                criterion.require_no_spill = true;
                parsed.criteria.push_back(std::move(criterion));
            } else {
                if (parts.size() != 2U) {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "An operator manifest field has the wrong number of values.");
                }
                const auto value = parts[1];
                bool duplicated = false;
                bool valid = true;
                if (parts[0] == "project") {
                    duplicated = duplicate(project_seen);
                    valid = normalized_path(
                        source_directory, value, parsed.project_path);
                } else if (parts[0] == "graduated_project") {
                    duplicated = duplicate(graduated_seen);
                    valid = normalized_path(
                        source_directory, value, parsed.graduated_project_path);
                } else if (parts[0] == "audit") {
                    duplicated = duplicate(audit_seen);
                    valid = normalized_path(
                        source_directory, value, parsed.audit_path);
                } else if (parts[0] == "input_project_sha256") {
                    duplicated = duplicate(input_sha_seen);
                    valid = is_sha256_digest(value);
                    parsed.input_project_sha256 = std::string(value);
                } else if (parts[0] == "fixture_id") {
                    duplicated = duplicate(fixture_seen);
                    valid = bounded_plain_text(value, 255U);
                    parsed.fixture_id = std::string(value);
                } else if (parts[0] == "unit_label") {
                    duplicated = duplicate(unit_seen);
                    valid = bounded_plain_text(value, 255U);
                    parsed.unit_label = std::string(value);
                } else if (parts[0] == "output_backend") {
                    duplicated = duplicate(backend_seen);
                    valid = bounded_plain_text(value, 128U) &&
                        soundswitch_binding(value);
                    parsed.output_backend = std::string(value);
                } else if (parts[0] == "operator_id") {
                    duplicated = duplicate(operator_seen);
                    valid = bounded_plain_text(value, 255U);
                    parsed.operator_id = std::string(value);
                } else if (parts[0] == "observation_timeout_ms") {
                    duplicated = duplicate(observation_timeout_seen);
                    std::uint64_t milliseconds = 0U;
                    valid = parse_integer(value, milliseconds) &&
                        milliseconds <= static_cast<std::uint64_t>(
                            std::numeric_limits<std::chrono::milliseconds::rep>::max());
                    if (valid) {
                        parsed.config.observation_timeout =
                            std::chrono::milliseconds{milliseconds};
                    }
                } else if (parts[0] == "session_timeout_ms") {
                    duplicated = duplicate(session_timeout_seen);
                    std::uint64_t milliseconds = 0U;
                    valid = parse_integer(value, milliseconds) &&
                        milliseconds <= static_cast<std::uint64_t>(
                            std::numeric_limits<std::chrono::milliseconds::rep>::max());
                    if (valid) {
                        parsed.config.session_timeout =
                            std::chrono::milliseconds{milliseconds};
                    }
                } else if (parts[0] == "blackout_repetitions") {
                    duplicated = duplicate(repetitions_seen);
                    valid = parse_integer(
                        value, parsed.config.blackout_frame_repetitions);
                } else {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "The operator manifest contains an unknown field.");
                }
                if (duplicated) {
                    return check(
                        RawHardwareTestOperatorError::DuplicateField,
                        line_number,
                        "A single-valued operator manifest field is duplicated.");
                }
                if (!valid) {
                    return check(
                        RawHardwareTestOperatorError::InvalidField,
                        line_number,
                        "An operator manifest field has an invalid value.");
                }
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        cursor = end + 1U;
    }

    if (!project_seen || !graduated_seen || !audit_seen || !input_sha_seen ||
        !fixture_seen || !unit_seen || !backend_seen || !operator_seen ||
        !observation_timeout_seen || !session_timeout_seen || !repetitions_seen ||
        parsed.criteria.empty() || parsed.markers_to_supersede.empty()) {
        return check(
            RawHardwareTestOperatorError::MissingField,
            0U,
            "The operator manifest is missing a required single-fixture field, criterion, or marker.");
    }
    manifest = std::move(parsed);
    return {};
}

RawHardwareTestOperatorCheck load_raw_hardware_test_operator_manifest(
    const std::filesystem::path& path,
    RawHardwareTestOperatorManifest& manifest) {
    std::string text;
    const auto read = read_bounded_file(path, kMaximumManifestBytes, text);
    if (!read.ok()) {
        return read;
    }
    return parse_raw_hardware_test_operator_manifest(
        text, path.parent_path(), manifest);
}

RawHardwareTestOperatorCheck prepare_raw_hardware_test_operator_run(
    RawHardwareTestOperatorManifest manifest,
    PreparedRawHardwareTestOperatorRun& prepared) {
    if (manifest.schema_version != kRawHardwareTestOperatorManifestVersion ||
        !soundswitch_binding(manifest.output_backend) ||
        !bounded_plain_text(manifest.operator_id, 255U)) {
        return check(
            RawHardwareTestOperatorError::InvalidPlan,
            0U,
            "The operator manifest schema, single Micro binding, or operator identity is invalid.");
    }
    const auto paths = validate_paths(manifest);
    if (!paths.ok()) {
        return paths;
    }
    const auto audit = validate_raw_hardware_test_operator_audit(
        manifest.audit_path);
    if (!audit.ok()) {
        return audit;
    }
    ProjectDocument project;
    const auto loaded = load_project(manifest.project_path, project, false);
    if (!loaded) {
        return check(
            RawHardwareTestOperatorError::InvalidProject,
            loaded.line,
            loaded.message);
    }
    const auto identity = identify_file_sha256(manifest.project_path);
    if (!identity.success) {
        return check(
            RawHardwareTestOperatorError::InvalidProject,
            0U,
            identity.message);
    }
    RawHardwareTestPlan plan;
    const auto built = build_raw_hardware_test_plan(
        project,
        manifest.input_project_sha256,
        manifest.fixture_id,
        manifest.unit_label,
        manifest.output_backend,
        manifest.criteria,
        manifest.markers_to_supersede,
        manifest.config,
        plan);
    if (!built.ok()) {
        return check(
            RawHardwareTestOperatorError::InvalidPlan,
            0U,
            std::string(raw_hardware_test_error_name(built.error)) + ": " +
                built.message);
    }
    PreparedRawHardwareTestOperatorRun candidate;
    candidate.manifest = std::move(manifest);
    candidate.candidate_project = std::move(project);
    candidate.plan = std::move(plan);
    candidate.candidate_file_sha256 = identity.sha256;
    prepared = std::move(candidate);
    return {};
}

std::string raw_hardware_test_operator_acknowledgement(
    const PreparedRawHardwareTestOperatorRun& prepared) {
    return "QUALIFY " + prepared.plan.binding.fixture_id + " | " +
        prepared.plan.binding.unit_label + " | " +
        prepared.plan.binding.output_backend + " | BASIS " +
        prepared.plan.candidate_project_sha256 + " | FILE " +
        prepared.candidate_file_sha256;
}

bool raw_hardware_test_operator_acknowledged(
    const PreparedRawHardwareTestOperatorRun& prepared,
    std::string_view response) {
    return response == raw_hardware_test_operator_acknowledgement(prepared);
}

RawHardwareTestOperatorCheck validate_raw_hardware_test_operator_audit(
    const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const bool exists = std::filesystem::exists(path, filesystem_error);
    if (filesystem_error) {
        return check(
            RawHardwareTestOperatorError::AuditInvalid,
            0U,
            "The append-only audit destination could not be inspected.");
    }
    if (!exists) {
        return {};
    }
    if (!std::filesystem::is_regular_file(path, filesystem_error) ||
        filesystem_error) {
        return check(
            RawHardwareTestOperatorError::AuditInvalid,
            0U,
            "The append-only audit destination is not a regular file.");
    }
    std::string text;
    const auto read = read_bounded_file(path, kMaximumAuditBytes, text);
    if (!read.ok()) {
        return check(
            RawHardwareTestOperatorError::AuditInvalid,
            read.line,
            read.message);
    }
    std::unordered_set<std::string> digests;
    return inspect_audit(text, digests);
}

RawHardwareTestOperatorCheck finalize_raw_hardware_test_operator_run(
    const PreparedRawHardwareTestOperatorRun& prepared,
    const RawHardwareTestSession& session,
    std::string_view completed_at_utc,
    RawHardwareTestOperatorCompletion& completion) {
    RawHardwareTestOperatorCompletion result;
    const auto made = session.make_attempt(
        prepared.candidate_project, completed_at_utc, result.attempt);
    if (!made.ok()) {
        return check(
            RawHardwareTestOperatorError::SessionIncomplete,
            0U,
            std::string(raw_hardware_test_error_name(made.error)) + ": " +
                made.message);
    }
    completion = result;
    const auto appended = append_attempt(
        prepared.manifest.audit_path, result.attempt);
    if (!appended.ok()) {
        completion = std::move(result);
        return appended;
    }
    result.audit_appended = true;
    completion = result;
    if (result.attempt.terminal_phase != RawHardwareTestPhase::Complete ||
        result.attempt.terminal_error != RawHardwareTestError::None) {
        completion = std::move(result);
        return {};
    }

    const auto current_identity = identify_file_sha256(
        prepared.manifest.project_path);
    if (!current_identity.success ||
        current_identity.sha256 != prepared.candidate_file_sha256) {
        completion = std::move(result);
        return check(
            RawHardwareTestOperatorError::GraduationRejected,
            0U,
            "The exact candidate file changed after plan acknowledgement; the attempt remains audited but no project was graduated.");
    }
    ProjectDocument current;
    const auto loaded = load_project(
        prepared.manifest.project_path, current, false);
    if (!loaded) {
        completion = std::move(result);
        return check(
            RawHardwareTestOperatorError::GraduationRejected,
            loaded.line,
            "The current candidate could not be reloaded after the attempt; audit was retained but no project was graduated.");
    }
    const auto graduated = graduate_raw_hardware_test_attempt(
        current, result.attempt);
    if (!graduated.ok()) {
        completion = std::move(result);
        return check(
            RawHardwareTestOperatorError::GraduationRejected,
            0U,
            std::string(raw_hardware_test_error_name(graduated.error)) + ": " +
                graduated.message +
                " The attempt remains audited but does not authorize output.");
    }
    std::error_code filesystem_error;
    if (std::filesystem::exists(
            prepared.manifest.graduated_project_path, filesystem_error) ||
        filesystem_error) {
        completion = std::move(result);
        return check(
            RawHardwareTestOperatorError::GraduationWriteFailed,
            0U,
            "The graduated-project destination appeared during the run; audit was retained and no file was overwritten.");
    }
    const auto saved = save_project_atomic(
        prepared.manifest.graduated_project_path, current, false);
    if (!saved) {
        completion = std::move(result);
        return check(
            RawHardwareTestOperatorError::GraduationWriteFailed,
            saved.line,
            saved.message +
                " The attempt remains audited but no graduated project was produced.");
    }
    result.graduated = true;
    completion = std::move(result);
    return {};
}

}  // namespace emberlights

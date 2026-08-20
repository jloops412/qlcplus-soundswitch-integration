#include "emberlights/ofl_fixture_catalog.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#endif

namespace emberlights {
namespace {

inline constexpr std::size_t kMaximumSearchQueryBytes = 160U;
inline constexpr std::size_t kMaximumSearchResponseBytes = 128U * 1024U;

void add_issue(
    std::vector<FixtureCatalogIssue>& issues,
    FixtureCatalogIssueSeverity severity,
    std::string code,
    std::string message) {
    issues.push_back({severity, std::move(code), std::move(message)});
}

[[nodiscard]] bool has_error(
    const std::vector<FixtureCatalogIssue>& issues) noexcept {
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == FixtureCatalogIssueSeverity::Error;
    });
}

[[nodiscard]] std::string trim_copy(std::string_view value) {
    std::size_t first = 0U;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

[[nodiscard]] bool valid_query(std::string_view query) noexcept {
    if (query.empty() || query.size() > kMaximumSearchQueryBytes) {
        return false;
    }
    return std::none_of(query.begin(), query.end(), [](const auto character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte < 0x20U && character != '\t';
    });
}

[[nodiscard]] bool valid_key_part(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 96U &&
        std::all_of(value.begin(), value.end(), [](const auto character) {
            return (character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9') || character == '-';
        });
}

[[nodiscard]] bool split_key(
    std::string_view key,
    std::string& manufacturer,
    std::string& fixture) {
    const auto separator = key.find('/');
    if (separator == std::string_view::npos ||
        key.find('/', separator + 1U) != std::string_view::npos) {
        return false;
    }
    const auto manufacturer_view = key.substr(0U, separator);
    const auto fixture_view = key.substr(separator + 1U);
    if (!valid_key_part(manufacturer_view) || !valid_key_part(fixture_view)) {
        return false;
    }
    manufacturer.assign(manufacturer_view);
    fixture.assign(fixture_view);
    return true;
}

[[nodiscard]] std::string humanize_key(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    bool capitalize = true;
    for (const auto character : value) {
        if (character == '-') {
            result.push_back(' ');
            capitalize = true;
            continue;
        }
        if (capitalize && character >= 'a' && character <= 'z') {
            result.push_back(static_cast<char>(character - 'a' + 'A'));
        } else {
            result.push_back(character);
        }
        capitalize = false;
    }
    return result;
}

[[nodiscard]] OpenFixtureLibraryEntry make_entry(
    std::string key,
    std::string manufacturer,
    std::string fixture) {
    OpenFixtureLibraryEntry entry;
    entry.key = std::move(key);
    entry.manufacturer_key = std::move(manufacturer);
    entry.fixture_key = std::move(fixture);
    entry.display_name = humanize_key(entry.manufacturer_key) + " / " +
        humanize_key(entry.fixture_key);
    const auto base = std::string("https://") +
        std::string(kOpenFixtureLibraryHost) + "/" + entry.key;
    entry.source_page_url = base;
    entry.qlc_download_url = base + "." +
        std::string(kOpenFixtureLibraryQlcPlugin);
    return entry;
}

void append_json_string(std::string& output, std::string_view value) {
    constexpr std::string_view digits = "0123456789abcdef";
    output.push_back('"');
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '"': output.append("\\\""); break;
        case '\\': output.append("\\\\"); break;
        case '\b': output.append("\\b"); break;
        case '\f': output.append("\\f"); break;
        case '\n': output.append("\\n"); break;
        case '\r': output.append("\\r"); break;
        case '\t': output.append("\\t"); break;
        default:
            if (byte < 0x20U) {
                output.append("\\u00");
                output.push_back(digits[byte >> 4U]);
                output.push_back(digits[byte & 0x0FU]);
            } else {
                output.push_back(character);
            }
            break;
        }
    }
    output.push_back('"');
}

void skip_space(std::string_view json, std::size_t& index) noexcept {
    while (index < json.size() &&
           std::isspace(static_cast<unsigned char>(json[index])) != 0) {
        ++index;
    }
}

[[nodiscard]] bool hex_value(char character, std::uint8_t& value) noexcept {
    if (character >= '0' && character <= '9') {
        value = static_cast<std::uint8_t>(character - '0');
        return true;
    }
    if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint8_t>(10 + character - 'a');
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint8_t>(10 + character - 'A');
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_json_string(
    std::string_view json,
    std::size_t& index,
    std::string& output) {
    if (index >= json.size() || json[index] != '"') {
        return false;
    }
    ++index;
    output.clear();
    while (index < json.size()) {
        const auto character = json[index++];
        if (character == '"') {
            return true;
        }
        if (static_cast<unsigned char>(character) < 0x20U) {
            return false;
        }
        if (character != '\\') {
            output.push_back(character);
            continue;
        }
        if (index >= json.size()) {
            return false;
        }
        const auto escaped = json[index++];
        switch (escaped) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u': {
            if (index + 4U > json.size()) {
                return false;
            }
            std::uint16_t codepoint = 0U;
            for (std::size_t digit = 0U; digit < 4U; ++digit) {
                std::uint8_t value = 0U;
                if (!hex_value(json[index + digit], value)) {
                    return false;
                }
                codepoint = static_cast<std::uint16_t>(
                    (codepoint << 4U) | value);
            }
            index += 4U;
            // OFL fixture keys are lower-case ASCII. Supporting non-ASCII
            // here would weaken key/path validation without adding utility.
            if (codepoint > 0x7FU) {
                return false;
            }
            output.push_back(static_cast<char>(codepoint));
            break;
        }
        default: return false;
        }
    }
    return false;
}

[[nodiscard]] std::string encode_project_field(std::string_view value) {
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '%' || character == '\t' || character == '\r' ||
            character == '\n' || byte < 0x20U) {
            encoded.push_back('%');
            encoded.push_back(digits[byte >> 4U]);
            encoded.push_back(digits[byte & 0x0FU]);
        } else {
            encoded.push_back(character);
        }
    }
    return encoded;
}

[[nodiscard]] std::string make_source_evidence_record(
    std::string_view profile_id,
    const OpenFixtureLibraryDownloadResult& result) {
    const std::array<std::string, 10> fields{{
        "PROFILE_SOURCE_EVIDENCE_V1",
        std::string(profile_id),
        "openFixtureLibrary",
        result.entry.key,
        result.entry.source_page_url,
        result.entry.qlc_download_url,
        result.source_license,
        result.source_content_sha256,
        result.adapter_version,
        "importedUnreviewed"}};
    std::string record;
    for (std::size_t index = 0U; index < fields.size(); ++index) {
        if (index != 0U) {
            record.push_back('\t');
        }
        record.append(encode_project_field(fields[index]));
    }
    return record;
}

[[nodiscard]] bool content_type_starts_with(
    std::string_view actual,
    std::string_view expected) noexcept {
    return actual.size() >= expected.size() &&
        std::equal(expected.begin(), expected.end(), actual.begin(),
                   [](const auto left, const auto right) {
                       return std::tolower(static_cast<unsigned char>(left)) ==
                           std::tolower(static_cast<unsigned char>(right));
                   });
}

#if defined(_WIN32)
struct WinHttpHandle {
    HINTERNET value{nullptr};

    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) noexcept : value(handle) {}
    ~WinHttpHandle() {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
};

[[nodiscard]] std::wstring widen_ascii(std::string_view value) {
    std::wstring result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<wchar_t>(
            static_cast<unsigned char>(character)));
    }
    return result;
}

[[nodiscard]] std::string narrow_ascii(std::wstring_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(character <= 0x7F ? static_cast<char>(character) : '?');
    }
    return result;
}

[[nodiscard]] std::string windows_error(std::string_view operation) {
    return std::string(operation) + " failed with Windows error " +
        std::to_string(static_cast<unsigned long>(GetLastError())) + ".";
}
#endif

}  // namespace

bool OpenFixtureLibrarySearchResult::ok() const noexcept {
    return !has_error(issues);
}

bool OpenFixtureLibraryDownloadResult::ok() const noexcept {
    return !has_error(issues) && static_cast<bool>(imported);
}

FixtureCatalogHttpRequest make_open_fixture_library_search_request(
    std::string_view query) {
    FixtureCatalogHttpRequest request;
    const auto trimmed = trim_copy(query);
    if (!valid_query(trimmed)) {
        return request;
    }
    request.method = FixtureCatalogHttpMethod::Post;
    request.path = std::string(kOpenFixtureLibrarySearchPath);
    request.content_type = "application/json";
    request.accept = "application/json";
    request.maximum_response_bytes = kMaximumSearchResponseBytes;
    request.body = "{\"searchQuery\":";
    append_json_string(request.body, trimmed);
    request.body.append(",\"manufacturersQuery\":[],\"categoriesQuery\":[]}");
    return request;
}

FixtureCatalogHttpRequest make_open_fixture_library_download_request(
    const OpenFixtureLibraryEntry& entry) {
    FixtureCatalogHttpRequest request;
    std::string manufacturer;
    std::string fixture;
    if (!split_key(entry.key, manufacturer, fixture) ||
        (!entry.manufacturer_key.empty() &&
         entry.manufacturer_key != manufacturer) ||
        (!entry.fixture_key.empty() && entry.fixture_key != fixture)) {
        return request;
    }
    request.method = FixtureCatalogHttpMethod::Get;
    request.path = "/" + manufacturer + "/" + fixture + "." +
        std::string(kOpenFixtureLibraryQlcPlugin);
    request.accept = "application/x-qlc-fixture, application/xml, text/xml";
    request.maximum_response_bytes = kOpenFixtureLibraryMaximumQxfBytes;
    return request;
}

OpenFixtureLibrarySearchResult parse_open_fixture_library_search(
    std::string_view query,
    std::string_view json) {
    OpenFixtureLibrarySearchResult result;
    result.query = trim_copy(query);
    if (json.empty() || json.size() > kMaximumSearchResponseBytes) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchResponseSize",
                  "Open Fixture Library returned an empty or oversized search response.");
        return result;
    }

    std::size_t index = 0U;
    skip_space(json, index);
    if (index >= json.size() || json[index++] != '[') {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchResponseJson",
                  "Open Fixture Library search did not return a JSON string array.");
        return result;
    }
    skip_space(json, index);
    std::unordered_set<std::string> seen;
    bool closed = false;
    if (index < json.size() && json[index] == ']') {
        ++index;
        closed = true;
    } else {
        bool limit_reported = false;
        while (index < json.size()) {
            if (result.entries.size() >=
                    kOpenFixtureLibraryMaximumSearchResults &&
                !limit_reported) {
                add_issue(result.issues, FixtureCatalogIssueSeverity::Warning,
                          "ofl.searchResultLimit",
                          "Search results were limited to the first 200 official OFL keys.");
                limit_reported = true;
            }
            std::string key;
            if (!parse_json_string(json, index, key)) {
                add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                          "ofl.searchResponseJson",
                          "Open Fixture Library search returned malformed JSON.");
                result.entries.clear();
                return result;
            }
            std::string manufacturer;
            std::string fixture;
            if (!split_key(key, manufacturer, fixture)) {
                add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                          "ofl.searchResultKey",
                          "Open Fixture Library returned an unsafe fixture key.");
                result.entries.clear();
                return result;
            }
            if (seen.insert(key).second &&
                result.entries.size() < kOpenFixtureLibraryMaximumSearchResults) {
                result.entries.push_back(make_entry(
                    std::move(key), std::move(manufacturer), std::move(fixture)));
            }
            skip_space(json, index);
            if (index >= json.size()) {
                break;
            }
            if (json[index] == ']') {
                ++index;
                closed = true;
                break;
            }
            if (json[index++] != ',') {
                add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                          "ofl.searchResponseJson",
                          "Open Fixture Library search returned malformed JSON.");
                result.entries.clear();
                return result;
            }
            skip_space(json, index);
        }
    }
    if (!closed) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchResponseJson",
                  "Open Fixture Library search returned an unterminated JSON array.");
        result.entries.clear();
        return result;
    }
    skip_space(json, index);
    if (index != json.size()) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchResponseJson",
                  "Open Fixture Library search returned trailing data.");
        result.entries.clear();
    }
    return result;
}

OpenFixtureLibrarySearchResult search_open_fixture_library(
    std::string_view query,
    const FixtureCatalogHttpFetch& fetch) {
    OpenFixtureLibrarySearchResult result;
    result.query = trim_copy(query);
    const auto request = make_open_fixture_library_search_request(result.query);
    if (request.path.empty()) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchQuery",
                  "Enter a fixture manufacturer or model using 160 bytes or fewer.");
        return result;
    }

    const auto response = fetch ? fetch(request) :
        default_open_fixture_library_fetch(request);
    if (!response) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchNetwork",
                  response.error.empty()
                      ? "Open Fixture Library search returned HTTP " +
                            std::to_string(response.status_code) + "."
                      : response.error);
        return result;
    }
    if (!response.content_type.empty() &&
        !content_type_starts_with(response.content_type, "application/json")) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.searchContentType",
                  "Open Fixture Library search returned an unexpected content type.");
        return result;
    }
    return parse_open_fixture_library_search(result.query, response.body);
}

OpenFixtureLibraryDownloadResult download_open_fixture_library_fixture(
    const OpenFixtureLibraryEntry& entry,
    const FixtureCatalogHttpFetch& fetch) {
    OpenFixtureLibraryDownloadResult result;
    result.entry = entry;
    const auto request = make_open_fixture_library_download_request(entry);
    if (request.path.empty()) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.fixtureKey",
                  "The selected Open Fixture Library key is invalid or inconsistent.");
        return result;
    }
    const auto response = fetch ? fetch(request) :
        default_open_fixture_library_fetch(request);
    if (!response) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.downloadNetwork",
                  response.error.empty()
                      ? "Open Fixture Library download returned HTTP " +
                            std::to_string(response.status_code) + "."
                      : response.error);
        return result;
    }
    if (response.body.empty() ||
        response.body.size() > kOpenFixtureLibraryMaximumQxfBytes) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.downloadSize",
                  "The downloaded QXF is empty or exceeds the 4 MB Studio limit.");
        return result;
    }
    const auto xml_type = response.content_type.empty() ||
        content_type_starts_with(response.content_type, "application/x-qlc-fixture") ||
        content_type_starts_with(response.content_type, "application/xml") ||
        content_type_starts_with(response.content_type, "text/xml") ||
        content_type_starts_with(response.content_type, "application/octet-stream");
    if (!xml_type) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.downloadContentType",
                  "Open Fixture Library returned an unexpected fixture content type.");
        return result;
    }

    result.source_content_sha256 = sha256_text(response.body);
    result.imported = import_qlc_fixture(response.body, entry.source_page_url);
    if (!result.imported ||
        result.imported.source != showcore::FixtureProfileSource::OpenFixtureLibrary) {
        add_issue(result.issues, FixtureCatalogIssueSeverity::Error,
                  "ofl.downloadProvenance",
                  "The official endpoint did not yield a valid OFL-authored QLC+ export; the candidate was quarantined.");
        result.imported.profiles.clear();
        return result;
    }

    result.imported.issues.push_back({
        QlcImportIssueSeverity::Warning,
        "ofl.importedUnreviewed",
        entry.key,
        "Downloaded from Open Fixture Library and converted consistently, but not compared with the manufacturer manual or physical fixture."});
    result.imported.issues.push_back({
        QlcImportIssueSeverity::Warning,
        "ofl.sourceRevisionUnpinned",
        entry.key,
        "The live OFL export does not expose its deployment commit; EmberLights preserved the exact QXF SHA-256 and will not auto-update this project snapshot."});
    add_issue(result.issues, FixtureCatalogIssueSeverity::Warning,
              "ofl.importedUnreviewed",
              "Imported OFL profiles remain unreviewed until their exact mode and channel chart are verified against the manufacturer manual and hardware.");
    add_issue(result.issues, FixtureCatalogIssueSeverity::Information,
              "ofl.immutableSnapshot",
              "The project receives an immutable native snapshot; later catalog changes do not replace it automatically.");

    for (const auto& profile : result.imported.profiles) {
        result.project_evidence_records.push_back(
            make_source_evidence_record(profile.id, result));
    }
    return result;
}

FixtureCatalogHttpResponse default_open_fixture_library_fetch(
    const FixtureCatalogHttpRequest& request) {
    FixtureCatalogHttpResponse response;
    if (request.path.empty() || request.path.front() != '/' ||
        request.path.find("..") != std::string::npos ||
        request.maximum_response_bytes == 0U ||
        request.maximum_response_bytes > kOpenFixtureLibraryMaximumQxfBytes ||
        request.body.size() > static_cast<std::size_t>(
            std::numeric_limits<unsigned long>::max())) {
        response.error = "The fixture catalog request failed local validation.";
        return response;
    }

#if !defined(_WIN32)
    response.error =
        "The built-in Open Fixture Library HTTPS transport is available in the Windows Studio build.";
    return response;
#else
    WinHttpHandle session(WinHttpOpen(
        L"EmberLights Fixture Catalog/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0U));
    if (session.value == nullptr) {
        response.error = windows_error("WinHttpOpen");
        return response;
    }
    if (WinHttpSetTimeouts(session.value, 5000, 5000, 15000, 15000) == FALSE) {
        response.error = windows_error("WinHttpSetTimeouts");
        return response;
    }

    const auto host = widen_ascii(kOpenFixtureLibraryHost);
    WinHttpHandle connection(WinHttpConnect(
        session.value, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0U));
    if (connection.value == nullptr) {
        response.error = windows_error("WinHttpConnect");
        return response;
    }
    const auto path = widen_ascii(request.path);
    const auto* verb = request.method == FixtureCatalogHttpMethod::Post
        ? L"POST" : L"GET";
    WinHttpHandle http_request(WinHttpOpenRequest(
        connection.value,
        verb,
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (http_request.value == nullptr) {
        response.error = windows_error("WinHttpOpenRequest");
        return response;
    }
    unsigned long disabled_features = WINHTTP_DISABLE_REDIRECTS;
    if (WinHttpSetOption(
            http_request.value,
            WINHTTP_OPTION_DISABLE_FEATURE,
            &disabled_features,
            static_cast<unsigned long>(sizeof(disabled_features))) == FALSE) {
        response.error = windows_error("WinHttpSetOption");
        return response;
    }

    std::string header_text;
    if (!request.content_type.empty()) {
        header_text.append("Content-Type: ");
        header_text.append(request.content_type);
        header_text.append("\r\n");
    }
    if (!request.accept.empty()) {
        header_text.append("Accept: ");
        header_text.append(request.accept);
        header_text.append("\r\n");
    }
    const auto headers = widen_ascii(header_text);
    const auto body_size = static_cast<unsigned long>(request.body.size());
    auto* body = request.body.empty()
        ? WINHTTP_NO_REQUEST_DATA
        : const_cast<char*>(request.body.data());
    if (WinHttpSendRequest(
            http_request.value,
            headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
            headers.empty() ? 0U : static_cast<unsigned long>(-1L),
            body,
            body_size,
            body_size,
            0U) == FALSE) {
        response.error = windows_error("WinHttpSendRequest");
        return response;
    }
    if (WinHttpReceiveResponse(http_request.value, nullptr) == FALSE) {
        response.error = windows_error("WinHttpReceiveResponse");
        return response;
    }

    unsigned long status = 0U;
    unsigned long status_size = static_cast<unsigned long>(sizeof(status));
    if (WinHttpQueryHeaders(
            http_request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &status_size,
            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        response.error = windows_error("WinHttpQueryHeaders(status)");
        return response;
    }
    response.status_code = status <= 65535U
        ? static_cast<std::uint16_t>(status) : 0U;

    std::array<wchar_t, 256> content_type{};
    unsigned long content_type_size =
        static_cast<unsigned long>(content_type.size() * sizeof(wchar_t));
    if (WinHttpQueryHeaders(
            http_request.value,
            WINHTTP_QUERY_CONTENT_TYPE,
            WINHTTP_HEADER_NAME_BY_INDEX,
            content_type.data(),
            &content_type_size,
            WINHTTP_NO_HEADER_INDEX) != FALSE) {
        std::size_t length = 0U;
        while (length < content_type.size() && content_type[length] != L'\0') {
            ++length;
        }
        response.content_type = narrow_ascii({content_type.data(), length});
    }

    while (true) {
        unsigned long available = 0U;
        if (WinHttpQueryDataAvailable(http_request.value, &available) == FALSE) {
            response.error = windows_error("WinHttpQueryDataAvailable");
            response.body.clear();
            return response;
        }
        if (available == 0U) {
            break;
        }
        if (available > request.maximum_response_bytes - response.body.size()) {
            response.error = "Open Fixture Library response exceeded the bounded Studio limit.";
            response.body.clear();
            return response;
        }
        const auto start = response.body.size();
        response.body.resize(start + available);
        unsigned long read = 0U;
        if (WinHttpReadData(
                http_request.value,
                response.body.data() + static_cast<std::ptrdiff_t>(start),
                available,
                &read) == FALSE) {
            response.error = windows_error("WinHttpReadData");
            response.body.clear();
            return response;
        }
        response.body.resize(start + read);
    }
    return response;
#endif
}

}  // namespace emberlights

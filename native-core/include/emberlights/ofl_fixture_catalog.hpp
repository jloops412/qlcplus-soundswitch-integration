#pragma once

#include "emberlights/qlc_fixture_import.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kOpenFixtureLibraryHost =
    "open-fixture-library.org";
inline constexpr std::string_view kOpenFixtureLibrarySearchPath =
    "/api/v1/get-search-results";
inline constexpr std::string_view kOpenFixtureLibraryQlcPlugin =
    "qlcplus_4.12.2";
inline constexpr std::string_view kOpenFixtureLibraryLicense = "MIT";
inline constexpr std::string_view kOpenFixtureLibraryCatalogAdapterVersion =
    "ofl-live-qxf-v1";
inline constexpr std::size_t kOpenFixtureLibraryMaximumSearchResults = 200U;
inline constexpr std::size_t kOpenFixtureLibraryMaximumQxfBytes =
    4U * 1024U * 1024U;

enum class FixtureCatalogHttpMethod : std::uint8_t {
    Get,
    Post
};

// Studio-only HTTPS boundary. Tests inject a deterministic fetcher; the
// Windows app uses default_open_fixture_library_fetch. No catalog/network
// object is retained by Runner or the DMX scheduler.
struct FixtureCatalogHttpRequest {
    FixtureCatalogHttpMethod method{FixtureCatalogHttpMethod::Get};
    std::string path;
    std::string content_type;
    std::string accept;
    std::string body;
    std::size_t maximum_response_bytes{128U * 1024U};
};

struct FixtureCatalogHttpResponse {
    std::uint16_t status_code{0U};
    std::string content_type;
    std::string body;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error.empty() && status_code >= 200U && status_code < 300U;
    }
};

using FixtureCatalogHttpFetch =
    std::function<FixtureCatalogHttpResponse(const FixtureCatalogHttpRequest&)>;

enum class FixtureCatalogIssueSeverity : std::uint8_t {
    Information,
    Warning,
    Error
};

struct FixtureCatalogIssue {
    FixtureCatalogIssueSeverity severity{FixtureCatalogIssueSeverity::Error};
    std::string code;
    std::string message;
};

struct OpenFixtureLibraryEntry {
    std::string key;
    std::string manufacturer_key;
    std::string fixture_key;
    std::string display_name;
    std::string source_page_url;
    std::string qlc_download_url;
};

struct OpenFixtureLibrarySearchResult {
    std::string query;
    std::vector<OpenFixtureLibraryEntry> entries;
    std::vector<FixtureCatalogIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
};

struct OpenFixtureLibraryDownloadResult {
    OpenFixtureLibraryEntry entry;
    std::string source_content_sha256;
    std::string source_license{std::string(kOpenFixtureLibraryLicense)};
    std::string adapter_version{
        std::string(kOpenFixtureLibraryCatalogAdapterVersion)};
    QlcFixtureImportResult imported;
    // Append these opaque records to ProjectDocument::unknown_records when
    // accepting the profiles. This preserves exact source identity until the
    // planned structured PROFILE_EVIDENCE model owns the same record kind.
    std::vector<std::string> project_evidence_records;
    std::vector<FixtureCatalogIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] FixtureCatalogHttpRequest make_open_fixture_library_search_request(
    std::string_view query);

[[nodiscard]] FixtureCatalogHttpRequest make_open_fixture_library_download_request(
    const OpenFixtureLibraryEntry& entry);

[[nodiscard]] OpenFixtureLibrarySearchResult parse_open_fixture_library_search(
    std::string_view query,
    std::string_view json);

[[nodiscard]] OpenFixtureLibrarySearchResult search_open_fixture_library(
    std::string_view query,
    const FixtureCatalogHttpFetch& fetch = {});

[[nodiscard]] OpenFixtureLibraryDownloadResult download_open_fixture_library_fixture(
    const OpenFixtureLibraryEntry& entry,
    const FixtureCatalogHttpFetch& fetch = {});

// Synchronous and intentionally Studio-only. Call from an authoring worker,
// not from Runner, the scheduler, or a Windows paint/input callback.
[[nodiscard]] FixtureCatalogHttpResponse default_open_fixture_library_fetch(
    const FixtureCatalogHttpRequest& request);

}  // namespace emberlights

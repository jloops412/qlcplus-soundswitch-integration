#include "emberlights/file_identity.hpp"
#include "emberlights/ofl_fixture_catalog.hpp"
#include "emberlights/project_io.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__    \
                      << ": " #condition << '\n';                            \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

[[nodiscard]] bool has_catalog_issue(
    const std::vector<emberlights::FixtureCatalogIssue>& issues,
    std::string_view code) {
    return std::any_of(issues.begin(), issues.end(), [code](const auto& issue) {
        return issue.code == code;
    });
}

[[nodiscard]] bool has_import_issue(
    const emberlights::QlcFixtureImportResult& imported,
    std::string_view code) {
    return std::any_of(
        imported.issues.begin(), imported.issues.end(), [code](const auto& issue) {
            return issue.code == code;
        });
}

[[nodiscard]] std::string ofl_qxf(std::string_view creator =
    "OFL – https://open-fixture-library.org/chauvet-dj/washfx") {
    return std::string{
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<!DOCTYPE FixtureDefinition>\n"
        "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
        " <Creator><Name>"} + std::string(creator) +
        "</Name><Version>1.3.2</Version><Author>OFL Test</Author></Creator>\n"
        " <Manufacturer>Chauvet DJ</Manufacturer>\n"
        " <Model>WashFX</Model>\n"
        " <Type>Color Changer</Type>\n"
        " <Channel Name=\"Red\" Preset=\"IntensityRed\"/>\n"
        " <Channel Name=\"Green\" Preset=\"IntensityGreen\"/>\n"
        " <Channel Name=\"Blue\" Preset=\"IntensityBlue\"/>\n"
        " <Mode Name=\"3-channel\">\n"
        "  <Channel Number=\"0\">Red</Channel>\n"
        "  <Channel Number=\"1\">Green</Channel>\n"
        "  <Channel Number=\"2\">Blue</Channel>\n"
        " </Mode>\n"
        "</FixtureDefinition>\n";
}

void test_search_request_is_bounded_and_exact() {
    const auto request =
        emberlights::make_open_fixture_library_search_request("  wash \"fx\"  ");
    CHECK(request.method == emberlights::FixtureCatalogHttpMethod::Post);
    CHECK(request.path == "/api/v1/get-search-results");
    CHECK(request.content_type == "application/json");
    CHECK(request.accept == "application/json");
    CHECK(request.maximum_response_bytes == 128U * 1024U);
    CHECK(request.body ==
        "{\"searchQuery\":\"wash \\\"fx\\\"\",\"manufacturersQuery\":[],\"categoriesQuery\":[]}");

    CHECK(emberlights::make_open_fixture_library_search_request("   ").path.empty());
    CHECK(emberlights::make_open_fixture_library_search_request(
        std::string(161U, 'x')).path.empty());
}

void test_search_response_is_strict_and_actionable() {
    const auto parsed = emberlights::parse_open_fixture_library_search(
        "washfx",
        "[\"chauvet-dj/washfx\",\"american-dj/7p-hex-ip\","
        "\"chauvet-dj/washfx\"]");
    CHECK(parsed.ok());
    CHECK(parsed.entries.size() == 2U);
    CHECK(parsed.entries[0].key == "chauvet-dj/washfx");
    CHECK(parsed.entries[0].manufacturer_key == "chauvet-dj");
    CHECK(parsed.entries[0].fixture_key == "washfx");
    CHECK(parsed.entries[0].display_name == "Chauvet Dj / Washfx");
    CHECK(parsed.entries[0].source_page_url ==
        "https://open-fixture-library.org/chauvet-dj/washfx");
    CHECK(parsed.entries[0].qlc_download_url ==
        "https://open-fixture-library.org/chauvet-dj/washfx.qlcplus_4.12.2");

    const auto unsafe = emberlights::parse_open_fixture_library_search(
        "washfx", "[\"../washfx\"]");
    CHECK(!unsafe.ok());
    CHECK(unsafe.entries.empty());
    CHECK(has_catalog_issue(unsafe.issues, "ofl.searchResultKey"));

    const auto malformed = emberlights::parse_open_fixture_library_search(
        "washfx", "[\"chauvet-dj/washfx\"");
    CHECK(!malformed.ok());
    CHECK(malformed.entries.empty());
    CHECK(has_catalog_issue(malformed.issues, "ofl.searchResponseJson"));

    const auto trailing = emberlights::parse_open_fixture_library_search(
        "washfx", "[] false");
    CHECK(!trailing.ok());
    CHECK(has_catalog_issue(trailing.issues, "ofl.searchResponseJson"));
}

void test_search_transport_is_injected_and_fail_closed() {
    std::size_t requests = 0U;
    const auto search = emberlights::search_open_fixture_library(
        "washfx",
        [&requests](const auto& request) {
            ++requests;
            CHECK(request.method == emberlights::FixtureCatalogHttpMethod::Post);
            CHECK(request.path == "/api/v1/get-search-results");
            return emberlights::FixtureCatalogHttpResponse{
                200U, "application/json; charset=utf-8",
                "[\"chauvet-dj/washfx\"]", {}};
        });
    CHECK(search.ok());
    CHECK(requests == 1U);
    CHECK(search.entries.size() == 1U);

    const auto failed = emberlights::search_open_fixture_library(
        "washfx", [](const auto&) {
            return emberlights::FixtureCatalogHttpResponse{
                503U, "application/json", {}, {}};
        });
    CHECK(!failed.ok());
    CHECK(has_catalog_issue(failed.issues, "ofl.searchNetwork"));

    const auto wrong_type = emberlights::search_open_fixture_library(
        "washfx", [](const auto&) {
            return emberlights::FixtureCatalogHttpResponse{
                200U, "text/html", "<html></html>", {}};
        });
    CHECK(!wrong_type.ok());
    CHECK(has_catalog_issue(wrong_type.issues, "ofl.searchContentType"));
}

void test_download_converts_but_never_qualifies() {
    const auto search = emberlights::parse_open_fixture_library_search(
        "washfx", "[\"chauvet-dj/washfx\"]");
    CHECK(search.ok());
    CHECK(search.entries.size() == 1U);
    if (search.entries.empty()) {
        return;
    }
    const auto qxf = ofl_qxf();
    std::size_t requests = 0U;
    const auto downloaded = emberlights::download_open_fixture_library_fixture(
        search.entries.front(),
        [&requests, &qxf](const auto& request) {
            ++requests;
            CHECK(request.method == emberlights::FixtureCatalogHttpMethod::Get);
            CHECK(request.path == "/chauvet-dj/washfx.qlcplus_4.12.2");
            CHECK(request.body.empty());
            CHECK(request.maximum_response_bytes ==
                emberlights::kOpenFixtureLibraryMaximumQxfBytes);
            return emberlights::FixtureCatalogHttpResponse{
                200U, "application/x-qlc-fixture", qxf, {}};
        });
    CHECK(requests == 1U);
    CHECK(downloaded.ok());
    CHECK(downloaded.source_content_sha256 == emberlights::sha256_text(qxf));
    CHECK(downloaded.source_license == "MIT");
    CHECK(downloaded.adapter_version == "ofl-live-qxf-v1");
    CHECK(downloaded.imported.source ==
        showcore::FixtureProfileSource::OpenFixtureLibrary);
    CHECK(downloaded.imported.profiles.size() == 1U);
    CHECK(downloaded.imported.profiles.front().source ==
        showcore::FixtureProfileSource::OpenFixtureLibrary);
    CHECK(downloaded.imported.profiles.front().source_revision ==
        std::string(emberlights::kQlcFixtureAdapterVersion) +
            "#sha256:" + emberlights::sha256_text(qxf));
    CHECK(has_import_issue(downloaded.imported, "ofl.importedUnreviewed"));
    CHECK(has_import_issue(downloaded.imported, "ofl.sourceRevisionUnpinned"));
    CHECK(has_catalog_issue(downloaded.issues, "ofl.importedUnreviewed"));
    CHECK(downloaded.project_evidence_records.size() == 1U);
    CHECK(downloaded.project_evidence_records.front().find(
        "PROFILE_SOURCE_EVIDENCE_V1\t") == 0U);
    CHECK(downloaded.project_evidence_records.front().find(
        "\topenFixtureLibrary\tchauvet-dj/washfx\t") != std::string::npos);
    CHECK(downloaded.project_evidence_records.front().find(
        "\tMIT\t" + emberlights::sha256_text(qxf) +
        "\tofl-live-qxf-v1\timportedUnreviewed") != std::string::npos);

    auto project = emberlights::make_starter_project();
    project.fixture_profiles.insert(
        project.fixture_profiles.end(),
        downloaded.imported.profiles.begin(),
        downloaded.imported.profiles.end());
    project.unknown_records.insert(
        project.unknown_records.end(),
        downloaded.project_evidence_records.begin(),
        downloaded.project_evidence_records.end());
    const auto serialized = emberlights::serialize_project(project);
    emberlights::ProjectDocument reopened;
    CHECK(emberlights::parse_project(serialized, reopened));
    CHECK(reopened.unknown_records == downloaded.project_evidence_records);
    CHECK(!reopened.fixture_profiles.empty());
    CHECK(reopened.fixture_profiles.back().source ==
        showcore::FixtureProfileSource::OpenFixtureLibrary);
    CHECK(reopened.fixture_profiles.back().source_revision ==
        downloaded.imported.profiles.back().source_revision);
}

void test_download_rejects_untrusted_paths_and_provenance() {
    emberlights::OpenFixtureLibraryEntry unsafe;
    unsafe.key = "../fixture";
    CHECK(emberlights::make_open_fixture_library_download_request(unsafe).path.empty());
    const auto unsafe_result = emberlights::download_open_fixture_library_fixture(
        unsafe, [](const auto&) { return emberlights::FixtureCatalogHttpResponse{}; });
    CHECK(!unsafe_result.ok());
    CHECK(has_catalog_issue(unsafe_result.issues, "ofl.fixtureKey"));

    const auto search = emberlights::parse_open_fixture_library_search(
        "washfx", "[\"chauvet-dj/washfx\"]");
    CHECK(!search.entries.empty());
    if (search.entries.empty()) {
        return;
    }
    const auto qxf = ofl_qxf("QLC+");
    const auto wrong_creator =
        emberlights::download_open_fixture_library_fixture(
            search.entries.front(),
            [&qxf](const auto&) {
                return emberlights::FixtureCatalogHttpResponse{
                    200U, "application/x-qlc-fixture", qxf, {}};
            });
    CHECK(!wrong_creator.ok());
    CHECK(wrong_creator.imported.profiles.empty());
    CHECK(has_catalog_issue(wrong_creator.issues, "ofl.downloadProvenance"));

    const auto wrong_content_type =
        emberlights::download_open_fixture_library_fixture(
            search.entries.front(), [](const auto&) {
                return emberlights::FixtureCatalogHttpResponse{
                    200U, "text/html", "<html>login</html>", {}};
            });
    CHECK(!wrong_content_type.ok());
    CHECK(has_catalog_issue(
        wrong_content_type.issues, "ofl.downloadContentType"));
}

}  // namespace

int main() {
    test_search_request_is_bounded_and_exact();
    test_search_response_is_strict_and_actionable();
    test_search_transport_is_injected_and_fail_closed();
    test_download_converts_but_never_qualifies();
    test_download_rejects_untrusted_paths_and_provenance();
    if (failures == 0) {
        std::cout << "All Open Fixture Library catalog tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}

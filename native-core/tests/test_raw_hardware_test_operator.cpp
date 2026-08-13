#include "emberlights/file_identity.hpp"
#include "emberlights/hardware_qualification.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/raw_hardware_test_operator.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

constexpr std::string_view kFixtureId = "ir4-bench-001";
const std::string kMarker =
    "MIGRATED_PATCH_UNVERIFIED\tfixture-mode-address-universe-review-required\t"
    "ir4-bench-001";

class FakeTransport final : public emberlights::RawHardwareTestTransport {
public:
    [[nodiscard]] bool open(
        const emberlights::FixtureQualificationBinding& binding) noexcept override {
        opened_binding = binding;
        ++open_calls;
        opened = open_succeeds;
        return opened;
    }

    [[nodiscard]] bool connected() const noexcept override {
        return opened && connected_value;
    }

    [[nodiscard]] bool send(
        const showcore::DmxUniverse& frame) noexcept override {
        ++send_calls;
        frames.push_back(frame);
        if (!opened || !connected_value || send_calls == fail_send_call) {
            return false;
        }
        return true;
    }

    void close() noexcept override {
        ++close_calls;
        opened = false;
    }

    bool open_succeeds{true};
    bool connected_value{true};
    bool opened{false};
    std::uint64_t fail_send_call{0U};
    std::uint64_t send_calls{0U};
    std::uint64_t open_calls{0U};
    std::uint64_t close_calls{0U};
    emberlights::FixtureQualificationBinding opened_binding{};
    std::vector<showcore::DmxUniverse> frames;
};

struct TestFiles {
    std::filesystem::path directory;
    std::filesystem::path project;
    std::filesystem::path graduated;
    std::filesystem::path audit;
};

[[nodiscard]] TestFiles make_files(std::string_view name) {
    const auto nonce = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    TestFiles files;
    files.directory = std::filesystem::temp_directory_path() /
        ("emberlights-raw-operator-" + std::string(name) + "-" +
         std::to_string(nonce));
    CHECK(std::filesystem::create_directories(files.directory));
    files.project = files.directory / "candidate.emberlights";
    files.graduated = files.directory / "graduated.emberlights";
    files.audit = files.directory / "attempts-v1.audit";
    auto project = emberlights::make_ir4_6ch_qualification_project();
    project.unknown_records.push_back(kMarker);
    CHECK(emberlights::save_project_atomic(files.project, project, false));
    return files;
}

[[nodiscard]] std::string manifest_text(
    std::string_view project = "candidate.emberlights",
    std::string_view graduated = "graduated.emberlights",
    std::string_view audit = "attempts-v1.audit") {
    std::ostringstream text;
    text << "EMBERLIGHTS_RAW_HARDWARE_TEST_OPERATOR\t1\n"
         << "project\t" << project << "\n"
         << "graduated_project\t" << graduated << "\n"
         << "audit\t" << audit << "\n"
         << "input_project_sha256\t" << std::string(64U, 'a') << "\n"
         << "fixture_id\t" << kFixtureId << "\n"
         << "unit_label\tIR-4 physical unit A\n"
         << "output_backend\tsoundswitch-micro:u1\n"
         << "operator_id\tbench-operator-001\n"
         << "observation_timeout_ms\t1000\n"
         << "session_timeout_ms\t60000\n"
         << "blackout_repetitions\t2\n"
         << "criterion\t1\t255\tRed emitter alone reaches full output; no spill.\n"
         << "criterion\t2\t255\tGreen emitter alone reaches full output; no spill.\n"
         << "criterion\t3\t255\tBlue emitter alone reaches full output; no spill.\n"
         << "criterion\t4\t255\tWhite emitter alone reaches full output; no spill.\n"
         << "criterion\t5\t255\tAmber emitter alone reaches full output; no spill.\n"
         << "criterion\t6\t255\tUV emitter alone reaches full output; no spill.\n"
         << "marker\t" << kMarker << "\n";
    return text.str();
}

[[nodiscard]] emberlights::PreparedRawHardwareTestOperatorRun prepare(
    const TestFiles& files,
    std::string text = {}) {
    if (text.empty()) {
        text = manifest_text();
    }
    emberlights::RawHardwareTestOperatorManifest manifest;
    const auto parsed = emberlights::parse_raw_hardware_test_operator_manifest(
        text, files.directory, manifest);
    CHECK(parsed.ok());
    emberlights::PreparedRawHardwareTestOperatorRun prepared;
    const auto result = emberlights::prepare_raw_hardware_test_operator_run(
        std::move(manifest), prepared);
    CHECK(result.ok());
    return prepared;
}

void complete_session(
    emberlights::RawHardwareTestSession& session,
    const emberlights::PreparedRawHardwareTestOperatorRun& prepared,
    FakeTransport& transport,
    std::string_view started_at = "2026-08-12T16:00:00Z") {
    const auto epoch = emberlights::RawHardwareTestSession::TimePoint{};
    CHECK(session.begin(
              prepared.plan,
              {prepared.manifest.operator_id, std::string(started_at)},
              transport,
              epoch).ok());
    for (std::size_t index = 0U;
         index < prepared.plan.requirements.size();
         ++index) {
        CHECK(session.submit_observation(
                  {index == 0U
                       ? "Fixture and every neighbor remained completely dark."
                       : "Expected single function responded; no neighbor responded.",
                   true,
                   true},
                  epoch + std::chrono::milliseconds{
                      static_cast<std::chrono::milliseconds::rep>(
                          (index + 1U) * 10U)}).ok());
    }
}

[[nodiscard]] std::size_t nonzero_slots(
    const showcore::DmxUniverse& frame) {
    return static_cast<std::size_t>(std::count_if(
        frame.begin(), frame.end(), [](std::uint8_t value) {
            return value != 0U;
        }));
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

}  // namespace

int main() {
    using emberlights::RawHardwareTestOperatorError;
    CHECK(std::string_view(emberlights::raw_hardware_test_operator_error_name(
              RawHardwareTestOperatorError::AuditInvalid)) == "audit-invalid");

    const auto files = make_files("success");
    const auto manifest_path = files.directory / "operator-v1.tsv";
    {
        std::ofstream output(manifest_path, std::ios::binary);
        output << manifest_text();
    }
    emberlights::RawHardwareTestOperatorManifest loaded_manifest;
    CHECK(emberlights::load_raw_hardware_test_operator_manifest(
              manifest_path, loaded_manifest).ok());
    CHECK(loaded_manifest.project_path == files.project);
    auto prepared = prepare(files);
    CHECK(prepared.manifest.project_path == files.project);
    CHECK(prepared.manifest.graduated_project_path == files.graduated);
    CHECK(prepared.manifest.audit_path == files.audit);
    CHECK(prepared.plan.requirements.size() == 7U);
    CHECK(prepared.plan.binding.fixture_id == kFixtureId);
    CHECK(prepared.plan.binding.unit_label == "IR-4 physical unit A");
    CHECK(prepared.plan.binding.output_backend == "soundswitch-micro:u1");
    CHECK(emberlights::is_sha256_digest(prepared.candidate_file_sha256));
    CHECK(!std::filesystem::exists(files.audit));
    CHECK(!std::filesystem::exists(files.graduated));

    const auto acknowledgement =
        emberlights::raw_hardware_test_operator_acknowledgement(prepared);
    CHECK(acknowledgement.find(prepared.candidate_file_sha256) !=
          std::string::npos);
    CHECK(acknowledgement.find(prepared.plan.candidate_project_sha256) !=
          std::string::npos);
    CHECK(emberlights::raw_hardware_test_operator_acknowledged(
        prepared, acknowledgement));
    CHECK(!emberlights::raw_hardware_test_operator_acknowledged(
        prepared, "TEST"));
    CHECK(!emberlights::raw_hardware_test_operator_acknowledged(
        prepared, acknowledgement + " "));

    FakeTransport transport;
    emberlights::RawHardwareTestSession session;
    complete_session(session, prepared, transport);
    CHECK(session.snapshot().phase == emberlights::RawHardwareTestPhase::Complete);
    CHECK(transport.open_calls == 1U);
    CHECK(transport.close_calls == 1U);
    CHECK(std::all_of(
        transport.frames.begin(), transport.frames.end(),
        [](const auto& frame) { return nonzero_slots(frame) <= 1U; }));

    emberlights::RawHardwareTestOperatorCompletion completion;
    const auto finalized = emberlights::finalize_raw_hardware_test_operator_run(
        prepared, session, "2026-08-12T16:05:00Z", completion);
    CHECK(finalized.ok());
    CHECK(completion.audit_appended);
    CHECK(completion.graduated);
    CHECK(emberlights::is_sha256_digest(completion.attempt.content_sha256));
    CHECK(std::filesystem::is_regular_file(files.audit));
    CHECK(std::filesystem::is_regular_file(files.graduated));
    CHECK(emberlights::validate_raw_hardware_test_operator_audit(files.audit).ok());
    emberlights::ProjectDocument graduated;
    CHECK(emberlights::load_project(files.graduated, graduated, false));
    CHECK(emberlights::evaluate_fixture_qualification_gate(graduated).allowed);

    const auto audit_before_replay = read_text(files.audit);
    const auto replay = emberlights::finalize_raw_hardware_test_operator_run(
        prepared, session, "2026-08-12T16:05:00Z", completion);
    CHECK(replay.error == RawHardwareTestOperatorError::AuditInvalid);
    CHECK(read_text(files.audit) == audit_before_replay);

    {
        const auto failed_files = make_files("failed-observation");
        auto failed_prepared = prepare(failed_files);
        FakeTransport failed_transport;
        emberlights::RawHardwareTestSession failed_session;
        const auto epoch = emberlights::RawHardwareTestSession::TimePoint{};
        CHECK(failed_session.begin(
                  failed_prepared.plan,
                  {failed_prepared.manifest.operator_id,
                   "2026-08-12T17:00:00Z"},
                  failed_transport,
                  epoch).ok());
        CHECK(failed_session.submit_observation(
                  {"A neighboring fixture responded.", false, false},
                  epoch + std::chrono::milliseconds{1}).error ==
              emberlights::RawHardwareTestError::ObservationRejected);
        CHECK(failed_session.snapshot().frames_accepted > 0U);
        emberlights::RawHardwareTestOperatorCompletion failed_completion;
        CHECK(emberlights::finalize_raw_hardware_test_operator_run(
                  failed_prepared,
                  failed_session,
                  "2026-08-12T17:00:01Z",
                  failed_completion).ok());
        CHECK(failed_completion.audit_appended);
        CHECK(!failed_completion.graduated);
        CHECK(!std::filesystem::exists(failed_files.graduated));
        CHECK(emberlights::validate_raw_hardware_test_operator_audit(
                  failed_files.audit).ok());
        emberlights::ProjectDocument unchanged;
        CHECK(emberlights::load_project(
            failed_files.project, unchanged, false));
        CHECK(!emberlights::evaluate_fixture_qualification_gate(unchanged).allowed);
        std::filesystem::remove_all(failed_files.directory);
    }

    {
        const auto stale_files = make_files("stale");
        auto stale_prepared = prepare(stale_files);
        FakeTransport stale_transport;
        emberlights::RawHardwareTestSession stale_session;
        complete_session(
            stale_session,
            stale_prepared,
            stale_transport,
            "2026-08-12T18:00:00Z");
        auto changed = stale_prepared.candidate_project;
        changed.name += " externally changed";
        CHECK(emberlights::save_project_atomic(
            stale_files.project, changed, false));
        emberlights::RawHardwareTestOperatorCompletion stale_completion;
        const auto stale = emberlights::finalize_raw_hardware_test_operator_run(
            stale_prepared,
            stale_session,
            "2026-08-12T18:05:00Z",
            stale_completion);
        CHECK(stale.error == RawHardwareTestOperatorError::GraduationRejected);
        CHECK(stale_completion.audit_appended);
        CHECK(!stale_completion.graduated);
        CHECK(std::filesystem::is_regular_file(stale_files.audit));
        CHECK(!std::filesystem::exists(stale_files.graduated));
        std::filesystem::remove_all(stale_files.directory);
    }

    {
        const auto byte_changed_files = make_files("byte-changed");
        auto byte_changed_prepared = prepare(byte_changed_files);
        FakeTransport byte_changed_transport;
        emberlights::RawHardwareTestSession byte_changed_session;
        complete_session(
            byte_changed_session,
            byte_changed_prepared,
            byte_changed_transport,
            "2026-08-12T18:10:00Z");
        {
            std::ofstream output(
                byte_changed_files.project,
                std::ios::binary | std::ios::app);
            output << '\n';
        }
        emberlights::RawHardwareTestOperatorCompletion byte_changed_completion;
        const auto byte_changed =
            emberlights::finalize_raw_hardware_test_operator_run(
                byte_changed_prepared,
                byte_changed_session,
                "2026-08-12T18:15:00Z",
                byte_changed_completion);
        CHECK(byte_changed.error ==
              RawHardwareTestOperatorError::GraduationRejected);
        CHECK(byte_changed_completion.audit_appended);
        CHECK(!byte_changed_completion.graduated);
        CHECK(std::filesystem::is_regular_file(byte_changed_files.audit));
        CHECK(!std::filesystem::exists(byte_changed_files.graduated));
        std::filesystem::remove_all(byte_changed_files.directory);
    }

    {
        emberlights::RawHardwareTestOperatorManifest rejected;
        auto duplicate = manifest_text();
        duplicate += "fixture_id\tsecond-fixture\n";
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  duplicate, files.directory, rejected).error ==
              RawHardwareTestOperatorError::DuplicateField);
        auto unknown = manifest_text();
        unknown += "raw_frame\t1=255\n";
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  unknown, files.directory, rejected).error ==
              RawHardwareTestOperatorError::InvalidField);
        auto backend = manifest_text();
        const auto location = backend.find("soundswitch-micro:u1");
        CHECK(location != std::string::npos);
        if (location != std::string::npos) {
            backend.replace(location, std::string("soundswitch-micro:u1").size(),
                            "artnet:u1");
        }
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  backend, files.directory, rejected).error ==
              RawHardwareTestOperatorError::InvalidField);
        const auto incomplete_files = make_files("incomplete-criteria");
        emberlights::RawHardwareTestOperatorManifest programmatic;
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  manifest_text(), incomplete_files.directory, programmatic).ok());
        auto escaped_backend = programmatic;
        escaped_backend.output_backend = "artnet:u1";
        emberlights::PreparedRawHardwareTestOperatorRun escaped_run;
        CHECK(emberlights::prepare_raw_hardware_test_operator_run(
                  std::move(escaped_backend), escaped_run).error ==
              RawHardwareTestOperatorError::InvalidPlan);
        auto wrong_schema = programmatic;
        wrong_schema.schema_version = 2U;
        CHECK(emberlights::prepare_raw_hardware_test_operator_run(
                  std::move(wrong_schema), escaped_run).error ==
              RawHardwareTestOperatorError::InvalidPlan);
        auto missing = manifest_text();
        const auto criterion = missing.find(
            "criterion\t6\t255\tUV emitter alone reaches full output; no spill.\n");
        CHECK(criterion != std::string::npos);
        if (criterion != std::string::npos) {
            missing.erase(
                criterion,
                std::string("criterion\t6\t255\tUV emitter alone reaches full output; no spill.\n").size());
        }
        emberlights::RawHardwareTestOperatorManifest incomplete;
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  missing, incomplete_files.directory, incomplete).ok());
        emberlights::PreparedRawHardwareTestOperatorRun not_prepared;
        CHECK(emberlights::prepare_raw_hardware_test_operator_run(
                  std::move(incomplete), not_prepared).error ==
              RawHardwareTestOperatorError::InvalidPlan);
        std::filesystem::remove_all(incomplete_files.directory);
    }

    {
        const auto tampered_files = make_files("tampered-audit");
        {
            std::ofstream output(tampered_files.audit, std::ios::binary);
            output << "EMBERLIGHTS_RAW_HARDWARE_TEST_AUDIT\t1\n"
                   << "RAW_HARDWARE_TEST_ATTEMPT\t1\tnot-a-digest\t00\n";
        }
        CHECK(emberlights::validate_raw_hardware_test_operator_audit(
                  tampered_files.audit).error ==
              RawHardwareTestOperatorError::AuditInvalid);
        emberlights::RawHardwareTestOperatorManifest manifest;
        CHECK(emberlights::parse_raw_hardware_test_operator_manifest(
                  manifest_text(), tampered_files.directory, manifest).ok());
        emberlights::PreparedRawHardwareTestOperatorRun rejected;
        CHECK(emberlights::prepare_raw_hardware_test_operator_run(
                  std::move(manifest), rejected).error ==
              RawHardwareTestOperatorError::AuditInvalid);
        std::filesystem::remove_all(tampered_files.directory);
    }

    std::filesystem::remove_all(files.directory);
    if (failures == 0) {
        std::cout << "Raw Hardware Test operator tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}

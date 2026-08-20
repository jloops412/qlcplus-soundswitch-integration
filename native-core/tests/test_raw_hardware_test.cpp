#include "emberlights/file_identity.hpp"
#include "emberlights/hardware_qualification.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/raw_hardware_test.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << " - " #condition "\n"; \
        ++failures; \
    } \
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
        attempted_frames.push_back(frame);
        if (!opened || !connected_value || send_calls == fail_send_call) {
            return false;
        }
        accepted_frames.push_back(frame);
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
    std::vector<showcore::DmxUniverse> attempted_frames;
    std::vector<showcore::DmxUniverse> accepted_frames;
};

[[nodiscard]] emberlights::ProjectDocument make_project() {
    auto project = emberlights::make_ir4_6ch_qualification_project();
    project.unknown_records.push_back(kMarker);
    return project;
}

[[nodiscard]] std::vector<emberlights::RawHardwareTestSlotCriterion>
make_criteria() {
    return {
        {1U, 255U, "Red emitter alone reaches full output; no other color or unit responds.", true},
        {2U, 255U, "Green emitter alone reaches full output; no other color or unit responds.", true},
        {3U, 255U, "Blue emitter alone reaches full output; no other color or unit responds.", true},
        {4U, 255U, "White emitter alone reaches full output; no other color or unit responds.", true},
        {5U, 255U, "Amber emitter alone reaches full output; no other color or unit responds.", true},
        {6U, 255U, "UV/purple emitter alone reaches full output; no other color or unit responds.", true},
    };
}

[[nodiscard]] emberlights::RawHardwareTestConfig make_config() {
    emberlights::RawHardwareTestConfig config;
    config.observation_timeout = std::chrono::milliseconds{1000};
    config.session_timeout = std::chrono::milliseconds{60000};
    config.blackout_frame_repetitions = 2U;
    return config;
}

[[nodiscard]] emberlights::RawHardwareTestPlan make_plan(
    const emberlights::ProjectDocument& project,
    std::string_view unit = "IR-4 physical unit A") {
    const auto criteria = make_criteria();
    const std::vector<std::string> markers{kMarker};
    emberlights::RawHardwareTestPlan plan;
    const auto result = emberlights::build_raw_hardware_test_plan(
        project,
        emberlights::fixture_qualification_project_basis_sha256(project),
        kFixtureId,
        unit,
        "soundswitch-micro:u1",
        criteria,
        markers,
        make_config(),
        plan);
    if (!result.ok()) {
        std::cerr << "plan failure: " << result.message << "\n";
    }
    CHECK(result.ok());
    return plan;
}

[[nodiscard]] std::size_t nonzero_slots(
    const showcore::DmxUniverse& frame) {
    return static_cast<std::size_t>(std::count_if(
        frame.begin(), frame.end(), [](std::uint8_t value) { return value != 0U; }));
}

void check_only_bounded_frames(
    const std::vector<showcore::DmxUniverse>& frames,
    std::uint16_t address,
    std::uint16_t footprint) {
    for (const auto& frame : frames) {
        CHECK(nonzero_slots(frame) <= 1U);
        for (std::size_t index = 0U; index < frame.size(); ++index) {
            if (frame[index] == 0U) {
                continue;
            }
            const auto channel = static_cast<std::uint16_t>(index + 1U);
            CHECK(channel >= address);
            CHECK(channel < static_cast<std::uint16_t>(address + footprint));
        }
    }
}

[[nodiscard]] emberlights::RawHardwareTestAttempt run_success(
    const emberlights::ProjectDocument& project,
    FakeTransport& transport) {
    emberlights::RawHardwareTestSession session;
    const auto started = emberlights::RawHardwareTestSession::TimePoint{};
    CHECK(session.begin(
              make_plan(project),
              {"bench-operator-001", "2026-08-12T16:00:00Z"},
              transport,
              started).ok());
    CHECK(session.snapshot().phase ==
          emberlights::RawHardwareTestPhase::AwaitingObservation);
    CHECK(session.plan() != nullptr);
    CHECK(session.plan()->requirements.size() == 7U);

    for (std::size_t index = 0U; index < 7U; ++index) {
        const auto result = session.submit_observation(
            {index == 0U
                 ? "Fixture and adjacent units stayed fully dark."
                 : "The expected single emitter responded with no spill.",
             true,
             true},
            started + std::chrono::milliseconds{
                static_cast<std::chrono::milliseconds::rep>((index + 1U) * 100U)});
        CHECK(result.ok());
    }
    const auto snapshot = session.snapshot();
    CHECK(snapshot.phase == emberlights::RawHardwareTestPhase::Complete);
    CHECK(snapshot.error == emberlights::RawHardwareTestError::None);
    CHECK(snapshot.completed_requirements == 7U);
    CHECK(snapshot.current_requirement == 7U);
    CHECK(snapshot.frames_attempted == snapshot.frames_accepted);
    CHECK(!snapshot.transport_open);
    CHECK(transport.open_calls == 1U);
    CHECK(transport.close_calls == 1U);
    CHECK(transport.opened_binding.fixture_id == kFixtureId);
    check_only_bounded_frames(transport.attempted_frames, 1U, 6U);

    emberlights::RawHardwareTestAttempt attempt;
    CHECK(session.make_attempt(
              project, "2026-08-12T16:05:00Z", attempt).ok());
    CHECK(attempt.terminal_phase == emberlights::RawHardwareTestPhase::Complete);
    CHECK(attempt.terminal_error == emberlights::RawHardwareTestError::None);
    CHECK(attempt.attestation.observations.size() == 7U);
    CHECK(emberlights::is_sha256_digest(attempt.content_sha256));
    CHECK(emberlights::validate_raw_hardware_test_attempt(project, attempt).ok());
    return attempt;
}

}  // namespace

int main() {
    using Clock = emberlights::RawHardwareTestSession;
    const auto epoch = Clock::TimePoint{};

    CHECK(std::string_view(emberlights::raw_hardware_test_phase_name(
              emberlights::RawHardwareTestPhase::BlackoutBefore)) ==
          "blackout-before");
    CHECK(std::string_view(emberlights::raw_hardware_test_error_name(
              emberlights::RawHardwareTestError::DeviceLost)) == "device-lost");

    const auto project = make_project();
    const auto basis = emberlights::fixture_qualification_project_basis_sha256(project);
    auto plan = make_plan(project);
    CHECK(emberlights::validate_raw_hardware_test_plan(plan).ok());
    CHECK(plan.input_project_sha256 == basis);
    CHECK(plan.candidate_project_sha256 == basis);
    CHECK(plan.footprint == 6U);
    CHECK(plan.requirements.size() == 7U);
    CHECK(plan.requirements.front().kind ==
          emberlights::FixtureQualificationRequirementKind::Blackout);
    CHECK(plan.requirements.back().absolute_channel == 6U);
    CHECK(plan.candidate_binding_sha256 ==
          emberlights::fixture_qualification_binding_sha256(plan.binding));

    for (std::size_t index = 0U; index < plan.requirements.size(); ++index) {
        showcore::DmxUniverse frame{};
        CHECK(emberlights::make_raw_hardware_test_frame(plan, index, frame).ok());
        CHECK(nonzero_slots(frame) == (index == 0U ? 0U : 1U));
        if (index > 0U) {
            CHECK(frame[index - 1U] == 255U);
        }
    }
    showcore::DmxUniverse outside{};
    CHECK(emberlights::make_raw_hardware_test_frame(
              plan, plan.requirements.size(), outside).error ==
          emberlights::RawHardwareTestError::InvalidPlan);

    auto tampered_plan = plan;
    tampered_plan.requirements[1U].absolute_channel = 2U;
    tampered_plan.requirements[1U].raw_frame_sha256 =
        emberlights::fixture_qualification_expected_frame_sha256(
            tampered_plan.requirements[1U]);
    CHECK(emberlights::validate_raw_hardware_test_plan(tampered_plan).error ==
          emberlights::RawHardwareTestError::InvalidPlan);
    tampered_plan = plan;
    tampered_plan.candidate_binding_sha256.front() = '0';
    CHECK(emberlights::validate_raw_hardware_test_plan(tampered_plan).error ==
          emberlights::RawHardwareTestError::InvalidBinding);

    {
        auto bad_criteria = make_criteria();
        bad_criteria.back().relative_slot = 5U;
        const std::vector<std::string> markers{kMarker};
        emberlights::RawHardwareTestPlan rejected;
        CHECK(emberlights::build_raw_hardware_test_plan(
                  project, basis, kFixtureId, "unit A", "soundswitch-micro:u1",
                  bad_criteria, markers, make_config(), rejected).error ==
              emberlights::RawHardwareTestError::InvalidCriteria);
        auto bad_config = make_config();
        bad_config.blackout_frame_repetitions = 0U;
        CHECK(emberlights::build_raw_hardware_test_plan(
                  project, basis, kFixtureId, "unit A", "soundswitch-micro:u1",
                  make_criteria(), markers, bad_config, rejected).error ==
              emberlights::RawHardwareTestError::InvalidConfiguration);
        const std::vector<std::string> missing_markers{
            "MIGRATED_PATCH_UNVERIFIED\tnot-in-project"};
        CHECK(emberlights::build_raw_hardware_test_plan(
                  project, basis, kFixtureId, "unit A", "soundswitch-micro:u1",
                  make_criteria(), missing_markers, make_config(), rejected).error ==
              emberlights::RawHardwareTestError::InvalidMarkers);
        CHECK(emberlights::build_raw_hardware_test_plan(
                  project, "not-a-digest", kFixtureId, "unit A",
                  "soundswitch-micro:u1", make_criteria(), markers,
                  make_config(), rejected).error ==
              emberlights::RawHardwareTestError::InvalidProjectBasis);
    }

    FakeTransport successful_transport;
    const auto successful_attempt = run_success(project, successful_transport);
    const auto encoded =
        emberlights::serialize_raw_hardware_test_attempt_record(successful_attempt);
    emberlights::RawHardwareTestAttempt parsed;
    CHECK(emberlights::parse_raw_hardware_test_attempt_record(encoded, parsed).ok());
    CHECK(parsed.content_sha256 == successful_attempt.content_sha256);
    CHECK(parsed.attestation.content_sha256 ==
          successful_attempt.attestation.content_sha256);
    auto tampered_record = encoded;
    tampered_record.back() = tampered_record.back() == '0' ? '1' : '0';
    CHECK(emberlights::parse_raw_hardware_test_attempt_record(
              tampered_record, parsed).error ==
          emberlights::RawHardwareTestError::InvalidAuditRecord);

    auto graduated = project;
    CHECK(emberlights::graduate_raw_hardware_test_attempt(
              graduated, successful_attempt).ok());
    CHECK(emberlights::fixture_qualification_project_basis_sha256(graduated) == basis);
    CHECK(emberlights::evaluate_fixture_qualification_gate(graduated).allowed);
    CHECK(std::any_of(
        graduated.unknown_records.begin(), graduated.unknown_records.end(),
        [](const auto& record) {
            return record.starts_with(emberlights::kRawHardwareTestAttemptRecord);
        }));
    CHECK(emberlights::record_raw_hardware_test_attempt(
              graduated, successful_attempt).error ==
          emberlights::RawHardwareTestError::Replay);
    emberlights::ProjectDocument persisted;
    CHECK(emberlights::parse_project(
              emberlights::serialize_project(graduated), persisted));
    CHECK(emberlights::evaluate_fixture_qualification_gate(persisted).allowed);
    const auto persisted_audit = std::find_if(
        persisted.unknown_records.begin(), persisted.unknown_records.end(),
        [](const auto& record) {
            return record.starts_with(emberlights::kRawHardwareTestAttemptRecord);
        });
    CHECK(persisted_audit != persisted.unknown_records.end());
    if (persisted_audit != persisted.unknown_records.end()) {
        CHECK(emberlights::parse_raw_hardware_test_attempt_record(
                  *persisted_audit, parsed).ok());
    }
    auto tampered_audit_project = graduated;
    const auto audit_record = std::find_if(
        tampered_audit_project.unknown_records.begin(),
        tampered_audit_project.unknown_records.end(),
        [](const auto& record) {
            return record.starts_with(emberlights::kRawHardwareTestAttemptRecord);
        });
    CHECK(audit_record != tampered_audit_project.unknown_records.end());
    if (audit_record != tampered_audit_project.unknown_records.end()) {
        audit_record->back() = audit_record->back() == '0' ? '1' : '0';
    }
    CHECK(!emberlights::evaluate_fixture_qualification_gate(
              tampered_audit_project).allowed);

    auto cross_project = project;
    cross_project.name += " changed";
    CHECK(emberlights::validate_raw_hardware_test_attempt(
              cross_project, successful_attempt).error ==
          emberlights::RawHardwareTestError::StaleProject);
    auto tampered_profile = project;
    tampered_profile.fixture_profiles.front().source_revision += "-changed";
    CHECK(emberlights::validate_raw_hardware_test_attempt(
              tampered_profile, successful_attempt).error ==
          emberlights::RawHardwareTestError::StaleProject);
    const auto unit_b = make_plan(project, "IR-4 physical unit B");
    CHECK(unit_b.candidate_binding_sha256 != plan.candidate_binding_sha256);

    {
        FakeTransport transport;
        transport.open_succeeds = false;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:00:00Z"},
                  transport,
                  epoch).error == emberlights::RawHardwareTestError::OpenFailed);
        CHECK(session.snapshot().phase == emberlights::RawHardwareTestPhase::Failed);
        CHECK(transport.send_calls == 0U);
        emberlights::RawHardwareTestAttempt attempt;
        CHECK(session.make_attempt(
                  project, "2026-08-12T17:00:01Z", attempt).ok());
        auto audited = project;
        CHECK(emberlights::record_raw_hardware_test_attempt(audited, attempt).ok());
        CHECK(emberlights::fixture_qualification_project_basis_sha256(audited) == basis);
        CHECK(!emberlights::evaluate_fixture_qualification_gate(audited).allowed);
        CHECK(emberlights::graduate_raw_hardware_test_attempt(
                  audited, attempt).error ==
              emberlights::RawHardwareTestError::AttestationRejected);
    }

    {
        FakeTransport transport;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:10:00Z"},
                  transport,
                  epoch).ok());
        CHECK(session.poll(epoch + std::chrono::milliseconds{1000}).error ==
              emberlights::RawHardwareTestError::TimedOut);
        CHECK(session.snapshot().shutdown_blackout_attempted);
        CHECK(session.snapshot().shutdown_blackout_succeeded);
        CHECK(session.observations().back().timed_out);
        CHECK(session.observations().back().blackout_after);
        check_only_bounded_frames(transport.attempted_frames, 1U, 6U);
    }

    {
        FakeTransport transport;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:20:00Z"},
                  transport,
                  epoch).ok());
        transport.connected_value = false;
        CHECK(session.poll(epoch + std::chrono::milliseconds{1}).error ==
              emberlights::RawHardwareTestError::DeviceLost);
        CHECK(session.observations().back().device_lost);
        CHECK(!session.snapshot().shutdown_blackout_succeeded);
        CHECK(transport.close_calls == 1U);
    }

    {
        FakeTransport transport;
        transport.fail_send_call = 1U;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:30:00Z"},
                  transport,
                  epoch).error == emberlights::RawHardwareTestError::BlackoutFailed);
        CHECK(session.snapshot().shutdown_blackout_attempted);
        check_only_bounded_frames(transport.attempted_frames, 1U, 6U);
    }

    {
        FakeTransport transport;
        transport.fail_send_call = 3U;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:40:00Z"},
                  transport,
                  epoch).error == emberlights::RawHardwareTestError::FrameWriteFailed);
        CHECK(session.observations().back().blackout_before);
        CHECK(session.observations().back().blackout_after);
        check_only_bounded_frames(transport.attempted_frames, 1U, 6U);
    }

    {
        FakeTransport transport;
        transport.fail_send_call = 4U;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T17:50:00Z"},
                  transport,
                  epoch).ok());
        CHECK(session.submit_observation(
                  {"dark as expected", true, true},
                  epoch + std::chrono::milliseconds{1}).error ==
              emberlights::RawHardwareTestError::BlackoutFailed);
        CHECK(!session.observations().back().blackout_after);
        check_only_bounded_frames(transport.attempted_frames, 1U, 6U);
    }

    {
        FakeTransport transport;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T18:00:00Z"},
                  transport,
                  epoch).ok());
        CHECK(session.submit_observation(
                  {"neighbor responded", true, false},
                  epoch + std::chrono::milliseconds{1}).error ==
              emberlights::RawHardwareTestError::ObservationRejected);
        CHECK(session.observations().back().failure ==
              "spill-or-neighbor-response-observed");
    }

    {
        FakeTransport transport;
        emberlights::RawHardwareTestSession session;
        CHECK(session.begin(
                  make_plan(project),
                  {"operator", "2026-08-12T18:10:00Z"},
                  transport,
                  epoch).ok());
        CHECK(session.cancel(
                  "operator pressed emergency stop",
                  epoch + std::chrono::milliseconds{1}).error ==
              emberlights::RawHardwareTestError::Cancelled);
        CHECK(session.snapshot().phase == emberlights::RawHardwareTestPhase::Cancelled);
        CHECK(session.snapshot().shutdown_blackout_succeeded);
    }

    {
        emberlights::SoundSwitchMicroRawHardwareTestTransport transport;
        auto wrong_binding = plan.binding;
        wrong_binding.output_backend = "artnet:u1";
        CHECK(!transport.open(wrong_binding));
        transport.close();
    }

    if (failures == 0) {
        std::cout << "Raw Hardware Test workflow tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}

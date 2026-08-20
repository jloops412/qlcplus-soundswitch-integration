#include "emberlights/file_identity.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"
#include "emberlights/hardware_qualification.hpp"
#include "emberlights/soundswitch_2026_autoloop.hpp"
#include "emberlights/soundswitch_import.hpp"
#include "emberlights/soundswitch_migration_ir.hpp"
#include "emberlights/soundswitch_source_binding.hpp"
#include "emberlights/soundswitch_v1.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/version.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

void print_help() {
    std::cout
        << "EmberLights SoundSwitch migration evidence tool " << emberlights::kVersion << "\n\n"
        << "Usage:\n"
        << "  emberlights_migrate inspect <SoundSwitch project or extracted application-data directory> [--report <file>] [--force]\n"
        << "  emberlights_migrate compare <before export> <after export> [--report <file>] [--force]\n"
        << "  emberlights_migrate bundle <SoundSwitch project or extracted application-data directory> <new bundle directory>\n"
        << "  emberlights_migrate verify-source-binding <project.emberlights> <SoundSwitch directory> [--archive <SoundSwitch.zip>] [--report <file>] [--force]\n"
        << "  emberlights_migrate corpus-manifest <SoundSwitch project directory> --report <file> [--source-version <version>] [--scripted-tracks] [--force]\n"
        << "  emberlights_migrate convert-2026-red-smooth <SoundSwitch project directory> <output.emberlights> [--report <file>] [--force]\n"
        << "  emberlights_migrate convert-v1 <SoundSwitch project directory> <output.emberlights> [--report <file>] [--force]\n"
        << "  emberlights_migrate upgrade-fixtures <input.emberlights> <new-output.emberlights> [--soundswitch-source <directory>] [--source-archive <SoundSwitch.zip>] [--report <file>] [--source-report <file>] [--force]\n"
        << "  emberlights_migrate template-v1 <output.emberlights> [--force]\n"
        << "  emberlights_migrate template-ir4-6ch-bench <output.emberlights> [--force]\n\n"
        << "inspect reads and hashes the source without modifying it.\n"
        << "bundle copies every regular payload into payload/, verifies each SHA-256, and\n"
        << "publishes inventory.json only after the complete bundle verifies. The destination\n"
        << "must not exist. No undocumented payload is interpreted or discarded.\n"
        << "compare performs two read-only inspections and reports only changed paths, hashes,\n"
        << "and bounded byte ranges. It never exports payload bytes.\n"
        << "verify-source-binding compares a project's recorded Venue/Autoloop hashes with a\n"
        << "complete read-only source inventory. Its review summary separates approximated,\n"
        << "source-only, unqualified project, missing, and not-imported areas. A hash match\n"
        << "establishes identity only; it never qualifies semantic coverage.\n"
        << "corpus-manifest evaluates evidence availability and missing dependency classes;\n"
        << "it does not claim semantic import completeness or scan external music libraries.\n"
        << "convert-2026-red-smooth imports the exact reviewed Medium slot 1 source timeline\n"
        << "into the current IR-4/tube patch and keeps every physical output disabled.\n"
        << "convert-v1 recognizes the qualified SoundSwitch 2.10.x color rig, rebuilds the\n"
        << "active 32-look bank as native semantic content, and leaves every DMX output off.\n"
        << "upgrade-fixtures creates a separate reviewed candidate for exact known-bad embedded\n"
        << "profile signatures. It never edits or overwrites the input project.\n"
        << "template-ir4-6ch-bench creates an editable one-fixture Blackout/R/G/B/W/A\n"
        << "project with every physical output disabled until Connections Save & Apply.\n";
}

[[nodiscard]] bool paths_name_same_file(
    const std::filesystem::path& first,
    const std::filesystem::path& second) {
    std::error_code filesystem_error;
    const auto both_exist = std::filesystem::exists(first, filesystem_error) &&
        !filesystem_error && std::filesystem::exists(second, filesystem_error) &&
        !filesystem_error;
    if (both_exist) {
        filesystem_error.clear();
        if (std::filesystem::equivalent(first, second, filesystem_error) &&
            !filesystem_error) {
            return true;
        }
    }
    filesystem_error.clear();
    const auto first_absolute = std::filesystem::absolute(first, filesystem_error);
    if (filesystem_error) {
        return false;
    }
    filesystem_error.clear();
    const auto second_absolute = std::filesystem::absolute(second, filesystem_error);
    return !filesystem_error &&
        first_absolute.lexically_normal() == second_absolute.lexically_normal();
}

bool save_text_atomic(
    const std::filesystem::path& path,
    std::string_view text,
    std::string& error) {
    std::error_code filesystem_error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "Unable to create the report folder.";
            return false;
        }
    }
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output || !output.write(text.data(), static_cast<std::streamsize>(text.size())) ||
            !output.flush()) {
            output.close();
            std::filesystem::remove(temporary, filesystem_error);
            error = "Unable to write the complete migration report.";
            return false;
        }
    }
    std::filesystem::remove(path, filesystem_error);
    filesystem_error.clear();
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary, filesystem_error);
        error = "Unable to publish the migration report.";
        return false;
    }
    return true;
}

int run(const std::vector<std::filesystem::path>& arguments) {
    if (arguments.empty() || arguments[0] == "--help" || arguments[0] == "-h") {
        print_help();
        return arguments.empty() ? 1 : 0;
    }
    const auto command = arguments[0].string();
    if (command == "inspect") {
        if (arguments.size() < 2U) {
            print_help();
            return 1;
        }
        std::filesystem::path report;
        bool force = false;
        for (std::size_t index = 2U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        const auto inspection = emberlights::inspect_soundswitch_project(arguments[1]);
        if (report.empty()) {
            std::cout << emberlights::serialize_soundswitch_inspection(inspection);
        } else {
            std::error_code filesystem_error;
            if (!force && std::filesystem::exists(report, filesystem_error)) {
                std::cerr << "Report already exists; use --force to replace it.\n";
                return 1;
            }
            std::string error;
            if (!emberlights::save_soundswitch_inspection_atomic(report, inspection, error)) {
                std::cerr << error << '\n';
                return 2;
            }
            std::cout << "Inspection report saved to " << report.string() << '\n';
        }
        std::cerr << "Inspected " << inspection.artifacts.size() << " file(s), "
                  << inspection.total_bytes << " byte(s): "
                  << inspection.error_count() << " error(s), "
                  << inspection.warning_count() << " warning(s).\n";
        return inspection.complete() ? 0 : 2;
    }
    if (command == "compare") {
        if (arguments.size() < 3U) {
            print_help();
            return 1;
        }
        std::filesystem::path report;
        bool force = false;
        for (std::size_t index = 3U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        const auto comparison = emberlights::compare_soundswitch_projects(
            arguments[1], arguments[2]);
        if (report.empty()) {
            std::cout << emberlights::serialize_soundswitch_comparison(comparison);
        } else {
            std::error_code filesystem_error;
            if (!force && std::filesystem::exists(report, filesystem_error)) {
                std::cerr << "Report already exists; use --force to replace it.\n";
                return 1;
            }
            std::string error;
            if (!emberlights::save_soundswitch_comparison_atomic(report, comparison, error)) {
                std::cerr << error << '\n';
                return 2;
            }
            std::cout << "Comparison report saved to " << report.string() << '\n';
        }
        std::cerr << "Compared " << comparison.artifacts.size() << " payload path(s): "
                  << comparison.modified_artifacts << " modified, "
                  << comparison.added_artifacts << " added, "
                  << comparison.removed_artifacts << " removed, "
                  << comparison.error_count() << " error(s), "
                  << comparison.warning_count() << " warning(s).\n";
        return comparison.complete() ? 0 : 2;
    }
    if (command == "bundle") {
        if (arguments.size() != 3U) {
            print_help();
            return 1;
        }
        const auto result = emberlights::create_soundswitch_source_bundle(
            arguments[1], arguments[2]);
        if (!result) {
            std::cerr << result.message << '\n';
            for (const auto& issue : result.inspection.issues) {
                if (issue.severity == emberlights::SoundSwitchIssueSeverity::Error) {
                    std::cerr << "ERROR [" << issue.code << "] "
                              << issue.subject << ": " << issue.message << '\n';
                }
            }
            return 2;
        }
        std::cout << "Verified migration source bundle created at "
                  << result.destination.string() << "\n"
                  << result.message << '\n';
        return 0;
    }
    if (command == "verify-source-binding") {
        if (arguments.size() < 3U) {
            print_help();
            return 1;
        }
        std::filesystem::path archive;
        std::filesystem::path report;
        bool force = false;
        for (std::size_t index = 3U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--archive" && index + 1U < arguments.size()) {
                archive = arguments[++index];
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        emberlights::ProjectDocument project;
        const auto loaded = emberlights::load_project(arguments[1], project, false);
        if (!loaded) {
            std::cerr << loaded.message << '\n';
            return 2;
        }
        const auto project_identity = emberlights::identify_file_sha256(arguments[1]);
        if (!project_identity.success) {
            std::cerr << project_identity.message << '\n';
            return 2;
        }
        const auto inspection = emberlights::inspect_soundswitch_project(arguments[2]);
        std::string archive_sha256;
        if (!archive.empty()) {
            const auto archive_identity = emberlights::identify_file_sha256(archive);
            if (!archive_identity.success) {
                std::cerr << archive_identity.message << '\n';
                return 2;
            }
            archive_sha256 = archive_identity.sha256;
        }
        const auto audit = emberlights::audit_soundswitch_source_binding(
            project, inspection);
        const auto serialized = emberlights::serialize_soundswitch_source_binding_audit(
            audit,
            arguments[1].filename().string(),
            project_identity.sha256,
            arguments[2].filename().string(),
            archive_sha256);
        if (report.empty()) {
            std::cout << serialized;
        } else {
            std::error_code filesystem_error;
            if (!force && std::filesystem::exists(report, filesystem_error)) {
                std::cerr << "Report already exists; use --force to replace it.\n";
                return 1;
            }
            std::string report_error;
            if (!save_text_atomic(report, serialized, report_error)) {
                std::cerr << report_error << '\n';
                return 2;
            }
            std::cout << "Source-binding audit saved to " << report.string() << '\n';
        }
        std::cerr << audit.message << "\nMigration review: "
                  << emberlights::soundswitch_migration_review_state_name(
                         audit.review_state)
                  << "; " << audit.review_areas.size()
                  << " area(s), " << audit.review_action_codes.size()
                  << " next action(s).\n";
        return audit.status ==
                emberlights::SoundSwitchSourceBindingStatus::ExactArtifactHashMatch
            ? 0
            : 4;
    }
    if (command == "corpus-manifest") {
        if (arguments.size() < 4U) {
            print_help();
            return 1;
        }
        std::filesystem::path report;
        std::string source_version;
        auto scope = emberlights::SoundSwitchMigrationScope::ProjectOnly;
        bool force = false;
        for (std::size_t index = 2U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--scripted-tracks") {
                scope = emberlights::SoundSwitchMigrationScope::ScriptedTracks;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else if (option == "--source-version" && index + 1U < arguments.size()) {
                source_version = arguments[++index].string();
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        if (report.empty()) {
            std::cerr << "corpus-manifest requires --report <file>.\n";
            return 1;
        }
        std::error_code filesystem_error;
        if (!force && std::filesystem::exists(report, filesystem_error)) {
            std::cerr << "Report already exists; use --force to replace it.\n";
            return 1;
        }
        const auto inspection = emberlights::inspect_soundswitch_project(arguments[1]);
        const auto manifest = emberlights::build_soundswitch_corpus_manifest(
            inspection, std::move(source_version), scope);
        const auto serialized = emberlights::serialize_soundswitch_corpus_manifest(manifest);
        if (serialized.empty()) {
            std::cerr << "The corpus manifest contract could not be validated.\n";
            return 2;
        }
        std::string report_error;
        if (!save_text_atomic(report, serialized, report_error)) {
            std::cerr << report_error << '\n';
            return 2;
        }
        std::cout << "Corpus evidence manifest saved to " << report.string() << '\n';
        std::cerr << "Inventoried " << manifest.artifacts.size()
                  << " verified artifact(s); "
                  << manifest.missing_dependency_codes.size()
                  << " dependency class(es) remain unavailable.\n";
        return inspection.complete() ? 0 : 2;
    }
    if (command == "convert-2026-red-smooth") {
        if (arguments.size() < 3U) {
            print_help();
            return 1;
        }
        std::filesystem::path report = arguments[2];
        report += ".migration.json";
        bool force = false;
        for (std::size_t index = 3U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        std::error_code filesystem_error;
        if (!force && (std::filesystem::exists(arguments[2], filesystem_error) ||
                       std::filesystem::exists(report, filesystem_error))) {
            std::cerr << "Output or report already exists; use --force to replace it.\n";
            return 1;
        }
        const auto migration =
            emberlights::create_soundswitch_2026_red_smooth_project(
                arguments[1]);
        if (!migration) {
            std::cerr << migration.message << '\n';
            return 2;
        }
        const auto saved = emberlights::save_project_atomic(
            arguments[2], migration.project, false);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 2;
        }
        std::string report_error;
        if (!save_text_atomic(
                report,
                emberlights::serialize_soundswitch_migration_report(
                    migration.proposal.migration_report),
                report_error)) {
            std::cerr << report_error << '\n';
            return 2;
        }
        std::cout << migration.message << "\nProject: "
                  << arguments[2].string() << "\nReport: "
                  << report.string() << '\n';
        return 0;
    }
    if (command == "convert-v1") {
        if (arguments.size() < 3U) {
            print_help();
            return 1;
        }
        std::filesystem::path report = arguments[2];
        report += ".migration.json";
        bool force = false;
        for (std::size_t index = 3U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        std::error_code filesystem_error;
        if (!force && (std::filesystem::exists(arguments[2], filesystem_error) ||
                       std::filesystem::exists(report, filesystem_error))) {
            std::cerr << "Output or report already exists; use --force to replace it.\n";
            return 1;
        }
        const auto migration = emberlights::create_soundswitch_v1_project(arguments[1]);
        if (!migration) {
            std::cerr << migration.message << '\n';
            return 2;
        }
        const auto saved = emberlights::save_project_atomic(arguments[2], migration.project, false);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 2;
        }
        std::string report_error;
        if (!save_text_atomic(
                report,
                emberlights::serialize_soundswitch_v1_migration_report(migration),
                report_error)) {
            std::cerr << report_error << '\n';
            return 2;
        }
        std::cout << migration.message << "\nProject: " << arguments[2].string()
                  << "\nReport: " << report.string() << '\n';
        return 0;
    }
    if (command == "upgrade-fixtures") {
        if (arguments.size() < 3U) {
            print_help();
            return 1;
        }
        const auto& input = arguments[1];
        const auto& output = arguments[2];
        if (paths_name_same_file(input, output)) {
            std::cerr << "The upgraded candidate must use a different path from the input project.\n";
            return 1;
        }
        std::filesystem::path report = output;
        report += ".fixture-upgrade.json";
        std::filesystem::path soundswitch_source;
        std::filesystem::path source_archive;
        std::filesystem::path source_report;
        bool force = false;
        for (std::size_t index = 3U; index < arguments.size(); ++index) {
            const auto option = arguments[index].string();
            if (option == "--force") {
                force = true;
            } else if (option == "--report" && index + 1U < arguments.size()) {
                report = arguments[++index];
            } else if (option == "--soundswitch-source" &&
                       index + 1U < arguments.size()) {
                soundswitch_source = arguments[++index];
            } else if (option == "--source-archive" &&
                       index + 1U < arguments.size()) {
                source_archive = arguments[++index];
            } else if (option == "--source-report" &&
                       index + 1U < arguments.size()) {
                source_report = arguments[++index];
            } else {
                std::cerr << "Unknown or incomplete option: " << option << '\n';
                return 1;
            }
        }
        if (soundswitch_source.empty() &&
            (!source_archive.empty() || !source_report.empty())) {
            std::cerr << "--source-archive and --source-report require --soundswitch-source.\n";
            return 1;
        }
        if (!soundswitch_source.empty() && source_report.empty()) {
            source_report = output;
            source_report += ".source-binding.json";
        }
        if (paths_name_same_file(input, report)) {
            std::cerr << "The upgrade report must not replace the input project.\n";
            return 1;
        }
        if (paths_name_same_file(output, report)) {
            std::cerr << "The upgrade report must use a different path from the candidate project.\n";
            return 1;
        }
        if (!source_report.empty() &&
            (paths_name_same_file(input, source_report) ||
             paths_name_same_file(output, source_report) ||
             paths_name_same_file(report, source_report))) {
            std::cerr << "The source-binding report must use its own output path.\n";
            return 1;
        }
        std::error_code filesystem_error;
        if (!force && (std::filesystem::exists(output, filesystem_error) ||
                       std::filesystem::exists(report, filesystem_error) ||
                       (!source_report.empty() &&
                        std::filesystem::exists(source_report, filesystem_error)))) {
            std::cerr << "Output or report already exists; use --force to replace it.\n";
            return 1;
        }

        emberlights::ProjectDocument candidate;
        const auto loaded = emberlights::load_project(input, candidate, false);
        if (!loaded) {
            std::cerr << loaded.message << '\n';
            return 2;
        }
        const auto input_identity = emberlights::identify_file_sha256(input);
        if (!input_identity.success) {
            std::cerr << input_identity.message << '\n';
            return 2;
        }
        emberlights::SoundSwitchSourceBindingAudit source_audit;
        bool source_audit_available = false;
        std::string source_archive_sha256;
        if (!soundswitch_source.empty()) {
            const auto inspection = emberlights::inspect_soundswitch_project(
                soundswitch_source);
            source_audit = emberlights::audit_soundswitch_source_binding(
                candidate, inspection);
            if (!inspection.complete()) {
                std::cerr << source_audit.message << '\n';
                return 2;
            }
            if (!source_archive.empty()) {
                const auto archive_identity = emberlights::identify_file_sha256(
                    source_archive);
                if (!archive_identity.success) {
                    std::cerr << archive_identity.message << '\n';
                    return 2;
                }
                source_archive_sha256 = archive_identity.sha256;
            }
            source_audit_available = true;
        }
        const auto plan = emberlights::plan_known_fixture_profile_upgrades(candidate);
        if (plan.empty()) {
            std::cerr << "No exact known-bad fixture profile signature was found; no output was written.\n";
            return 3;
        }
        auto upgraded = emberlights::apply_fixture_profile_upgrade_plan(candidate, plan);
        if (!upgraded.applied) {
            std::cerr << upgraded.message << '\n';
            return 2;
        }
        candidate.unknown_records.push_back(
            "FIXTURE_PROFILE_UPGRADE_INPUT_SHA256\t" + input_identity.sha256);
        if (source_audit_available) {
            emberlights::record_soundswitch_source_binding_evidence(
                candidate, source_audit, source_archive_sha256);
        }
        const auto saved = emberlights::save_project_atomic(output, candidate, false);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 2;
        }
        const auto output_identity = emberlights::identify_file_sha256(output);
        if (!output_identity.success) {
            std::cerr << output_identity.message << '\n';
            return 2;
        }
        std::string report_error;
        if (!save_text_atomic(
                report,
                emberlights::serialize_fixture_profile_upgrade_report(
                    upgraded,
                    input.filename().string(),
                    output.filename().string(),
                    output_identity.sha256,
                    input_identity.sha256),
                report_error)) {
            std::cerr << report_error << " The verified candidate remains at "
                      << output.string() << ".\n";
            return 2;
        }
        if (source_audit_available &&
            !save_text_atomic(
                source_report,
                emberlights::serialize_soundswitch_source_binding_audit(
                    source_audit,
                    input.filename().string(),
                    input_identity.sha256,
                    soundswitch_source.filename().string(),
                    source_archive_sha256),
                report_error)) {
            std::cerr << report_error << " The verified candidate remains at "
                      << output.string() << ".\n";
            return 2;
        }
        std::cout << upgraded.message
                  << "\nInput SHA-256: " << input_identity.sha256
                  << "\nCandidate: " << output.string()
                  << "\nCandidate SHA-256: " << output_identity.sha256
                  << "\nReport: " << report.string();
        if (source_audit_available) {
            std::cout << "\nSource binding: "
                      << emberlights::soundswitch_source_binding_status_name(
                             source_audit.status)
                      << "\nSource report: " << source_report.string();
        }
        std::cout << '\n';
        return 0;
    }
    if (command == "template-v1") {
        if (arguments.size() < 2U || arguments.size() > 3U ||
            (arguments.size() == 3U && arguments[2] != "--force")) {
            print_help();
            return 1;
        }
        const bool force = arguments.size() == 3U;
        std::error_code filesystem_error;
        if (!force && std::filesystem::exists(arguments[1], filesystem_error)) {
            std::cerr << "Output already exists; use --force to replace it.\n";
            return 1;
        }
        const auto project = emberlights::make_safe_color_rig_v1_template();
        const auto saved = emberlights::save_project_atomic(arguments[1], project, false);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 2;
        }
        std::cout << "Safe color-rig V1 template saved to " << arguments[1].string() << '\n';
        return 0;
    }
    if (command == "template-ir4-6ch-bench") {
        if (arguments.size() < 2U || arguments.size() > 3U ||
            (arguments.size() == 3U && arguments[2] != "--force")) {
            print_help();
            return 1;
        }
        const bool force = arguments.size() == 3U;
        std::error_code filesystem_error;
        if (!force && std::filesystem::exists(arguments[1], filesystem_error)) {
            std::cerr << "Output already exists; use --force to replace it.\n";
            return 1;
        }
        const auto project = emberlights::make_ir4_6ch_operator_bench_project();
        const auto saved = emberlights::save_project_atomic(arguments[1], project, false);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 2;
        }
        std::cout << "Output-disabled editable IR-4 6CH bench saved to "
                  << arguments[1].string() << '\n';
        return 0;
    }
    std::cerr << "Unknown command: " << command << "\n";
    print_help();
    return 1;
}

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::filesystem::path> arguments;
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#else
int main(int argc, char** argv) {
    std::vector<std::filesystem::path> arguments;
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return run(arguments);
}
#endif

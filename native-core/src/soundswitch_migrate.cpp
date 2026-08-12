#include "emberlights/soundswitch_import.hpp"
#include "emberlights/soundswitch_migration_ir.hpp"
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
        << "  emberlights_migrate inspect <SoundSwitch project directory> [--report <file>] [--force]\n"
        << "  emberlights_migrate compare <before export> <after export> [--report <file>] [--force]\n"
        << "  emberlights_migrate bundle <SoundSwitch project directory> <new bundle directory>\n"
        << "  emberlights_migrate corpus-manifest <SoundSwitch project directory> --report <file> [--source-version <version>] [--scripted-tracks] [--force]\n"
        << "  emberlights_migrate convert-v1 <SoundSwitch project directory> <output.emberlights> [--report <file>] [--force]\n"
        << "  emberlights_migrate template-v1 <output.emberlights> [--force]\n\n"
        << "inspect reads and hashes the source without modifying it.\n"
        << "bundle copies every regular payload into payload/, verifies each SHA-256, and\n"
        << "publishes inventory.json only after the complete bundle verifies. The destination\n"
        << "must not exist. No undocumented payload is interpreted or discarded.\n"
        << "compare performs two read-only inspections and reports only changed paths, hashes,\n"
        << "and bounded byte ranges. It never exports payload bytes.\n"
        << "corpus-manifest evaluates evidence availability and missing dependency classes;\n"
        << "it does not claim semantic import completeness or scan external music libraries.\n"
        << "convert-v1 recognizes the qualified SoundSwitch 2.10.x color rig, rebuilds the\n"
        << "active 32-look bank as native semantic content, and leaves every DMX output off.\n";
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

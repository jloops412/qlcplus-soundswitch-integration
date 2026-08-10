#include "emberlights/soundswitch_import.hpp"
#include "emberlights/version.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

void print_help() {
    std::cout
        << "EmberLights SoundSwitch migration evidence tool " << emberlights::kVersion << "\n\n"
        << "Usage:\n"
        << "  emberlights_migrate inspect <SoundSwitch project directory> [--report <file>] [--force]\n"
        << "  emberlights_migrate compare <before export> <after export> [--report <file>] [--force]\n"
        << "  emberlights_migrate bundle <SoundSwitch project directory> <new bundle directory>\n\n"
        << "inspect reads and hashes the source without modifying it.\n"
        << "bundle copies every regular payload into payload/, verifies each SHA-256, and\n"
        << "publishes inventory.json only after the complete bundle verifies. The destination\n"
        << "must not exist. No undocumented payload is interpreted or discarded.\n"
        << "compare performs two read-only inspections and reports only changed paths, hashes,\n"
        << "and bounded byte ranges. It never exports payload bytes.\n";
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

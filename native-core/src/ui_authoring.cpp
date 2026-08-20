#include "emberlights/ui_authoring.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace emberlights {
namespace {

constexpr std::array<UiAuthoringResourceDescriptor, 6U> kDescriptors{{
    {"profile", "profiles", "New Profile",
     "Search profiles, manufacturer, model, mode or ID"},
    {"fixture", "fixtures", "New Fixture",
     "Search fixtures, profile, role, universe, address or ID"},
    {"group", "groups", "New Group",
     "Search groups, member fixture or ID"},
    {"Static Look", "Static Looks", "New Look",
     "Search Looks, target, property or ID"},
    {"Autoloop", "Autoloops", "New Autoloop",
     "Search Autoloops, bank, slot, Look or ID"},
    {"Track Script", "Track Scripts", "New Script",
     "Search Track Scripts, audio, cue or ID"},
}};

[[nodiscard]] constexpr std::size_t descriptor_index(
    UiAuthoringResourceKind kind) noexcept {
    const auto index = static_cast<std::size_t>(kind);
    return index < kDescriptors.size() ? index : 0U;
}

[[nodiscard]] bool ascii_space(unsigned char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
        value == '\f' || value == '\v';
}

[[nodiscard]] char ascii_lower(char value) noexcept {
    const auto byte = static_cast<unsigned char>(value);
    return byte >= static_cast<unsigned char>('A') &&
            byte <= static_cast<unsigned char>('Z')
        ? static_cast<char>(byte + ('a' - 'A'))
        : value;
}

[[nodiscard]] std::string normalize_query(std::string_view query) {
    std::string normalized;
    normalized.reserve(query.size());
    bool pending_space = false;
    for (const auto value : query) {
        if (ascii_space(static_cast<unsigned char>(value))) {
            pending_space = !normalized.empty();
            continue;
        }
        if (pending_space) {
            normalized.push_back(' ');
            pending_space = false;
        }
        normalized.push_back(ascii_lower(value));
    }
    return normalized;
}

[[nodiscard]] std::vector<std::string_view> query_tokens(
    std::string_view normalized) {
    std::vector<std::string_view> tokens;
    std::size_t begin = 0U;
    while (begin < normalized.size()) {
        const auto end = normalized.find(' ', begin);
        tokens.push_back(normalized.substr(
            begin,
            end == std::string_view::npos ? normalized.size() - begin
                                          : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return tokens;
}

void append_search_text(std::string& target, std::string_view value) {
    if (value.empty()) {
        return;
    }
    if (!target.empty()) {
        target.push_back(' ');
    }
    for (const auto byte : value) {
        target.push_back(ascii_lower(byte));
    }
}

[[nodiscard]] bool item_matches(
    const UiAuthoringItem& item,
    std::span<const std::string_view> tokens) {
    if (tokens.empty()) {
        return true;
    }
    std::string searchable;
    std::size_t bytes = item.stable_id.size() + item.primary_text.size() +
        item.secondary_text.size() + 3U;
    for (const auto& term : item.search_terms) {
        bytes += term.size() + 1U;
    }
    searchable.reserve(bytes);
    append_search_text(searchable, item.stable_id);
    append_search_text(searchable, item.primary_text);
    append_search_text(searchable, item.secondary_text);
    for (const auto& term : item.search_terms) {
        append_search_text(searchable, term);
    }
    return std::all_of(
        tokens.begin(), tokens.end(), [&](const auto token) {
            return searchable.find(token) != std::string::npos;
        });
}

[[nodiscard]] std::string selected_suffix(
    const UiAuthoringProjection& projection) {
    if (projection.selection_hidden()) {
        return " • selection hidden by filter";
    }
    return projection.selected_visible_row.has_value() ? " • selected" : "";
}

[[nodiscard]] constexpr std::int32_t clamp_nonnegative(
    std::int32_t value) noexcept {
    return std::max<std::int32_t>(0, value);
}

}  // namespace

UiAuthoringResourceDescriptor authoring_resource_descriptor(
    UiAuthoringResourceKind kind) noexcept {
    return kDescriptors[descriptor_index(kind)];
}

UiAuthoringProjection project_authoring_items(
    std::span<const UiAuthoringItem> items,
    std::string_view query,
    std::string_view selected_stable_id) {
    UiAuthoringProjection result;
    result.total_count = items.size();
    if (query.size() > kUiAuthoringMaximumQueryBytes) {
        result.query_too_long = true;
        result.normalized_query = normalize_query(
            query.substr(0U, kUiAuthoringMaximumQueryBytes));
        return result;
    }
    result.normalized_query = normalize_query(query);
    const auto tokens = query_tokens(result.normalized_query);
    result.source_indices.reserve(items.size());
    for (std::size_t index = 0U; index < items.size(); ++index) {
        if (!selected_stable_id.empty() &&
            items[index].stable_id == selected_stable_id) {
            result.selected_source_index = index;
        }
        if (item_matches(items[index], tokens)) {
            if (!selected_stable_id.empty() &&
                items[index].stable_id == selected_stable_id) {
                result.selected_visible_row = result.source_indices.size();
            }
            result.source_indices.push_back(index);
        }
    }
    return result;
}

std::string authoring_collection_summary(
    UiAuthoringResourceKind kind,
    const UiAuthoringProjection& projection) {
    const auto descriptor = authoring_resource_descriptor(kind);
    if (projection.query_too_long) {
        return "Search is limited to 256 UTF-8 bytes. Shorten the query.";
    }
    if (projection.total_count == 0U) {
        return std::string{"No "} + std::string{descriptor.plural} +
            " yet • " + std::string{descriptor.create_label} + " starts a draft";
    }
    if (projection.query_active() && projection.source_indices.empty()) {
        return std::string{"No matching "} + std::string{descriptor.plural} +
            " • Esc clears search" + selected_suffix(projection);
    }
    std::ostringstream summary;
    if (projection.query_active()) {
        summary << projection.source_indices.size() << " of ";
    }
    summary << projection.total_count << ' ' << descriptor.plural;
    if (projection.query_active()) {
        summary << " shown";
    }
    summary << selected_suffix(projection);
    return summary.str();
}

std::string authoring_inspector_heading(
    const UiAuthoringInspectorStatus& status) {
    const auto descriptor = authoring_resource_descriptor(status.kind);
    std::string heading{"INSPECTOR • "};
    switch (status.mode) {
    case UiAuthoringInspectorMode::Empty:
        heading += "No ";
        heading += descriptor.singular;
        heading += " selected";
        break;
    case UiAuthoringInspectorMode::Creating:
        heading += status.primary_text.empty()
            ? std::string{descriptor.create_label}
            : status.primary_text;
        heading += " • new draft";
        break;
    case UiAuthoringInspectorMode::Editing:
    case UiAuthoringInspectorMode::ReadOnly:
        heading += status.primary_text.empty()
            ? std::string{descriptor.singular}
            : status.primary_text;
        if (status.mode == UiAuthoringInspectorMode::ReadOnly) {
            heading += " • read-only source";
        } else if (status.draft_changed) {
            heading += " • unsaved edits";
        }
        break;
    }
    return heading;
}

UiAuthoringWorkbenchLayout compute_authoring_workbench_layout(
    std::int32_t client_width,
    std::int32_t client_height,
    UiShellDensity density,
    UiAuthoringCollectionEmphasis emphasis) noexcept {
    const auto width = std::max<std::int32_t>(640, client_width);
    const auto height = std::max<std::int32_t>(480, client_height);
    const auto margin = density == UiShellDensity::Compact ? 18 : 24;
    const auto gap = density == UiShellDensity::Wide ? 20 : 16;
    constexpr std::int32_t top = 62;
    const auto bottom = height - 14;
    const auto available = width - margin * 2 - gap;
    const auto standard_numerator = density == UiShellDensity::Wide ? 31 : 34;
    const auto wide_numerator = density == UiShellDensity::Wide ? 52 : 48;
    const auto numerator = emphasis == UiAuthoringCollectionEmphasis::Wide
        ? wide_numerator
        : standard_numerator;
    const auto desired_library = available * numerator / 100;
    const auto minimum_library = emphasis == UiAuthoringCollectionEmphasis::Wide
        ? 380
        : 260;
    const auto maximum_library = emphasis == UiAuthoringCollectionEmphasis::Wide
        ? 760
        : 420;
    const auto library_width = std::clamp(
        desired_library,
        std::min(minimum_library, available / 2),
        std::min(maximum_library, std::max(1, available - 300)));
    const UiRectangle library{margin, top, library_width, bottom - top};
    const UiRectangle inspector{
        library.right() + gap,
        top,
        clamp_nonnegative(width - margin - library.right() - gap),
        bottom - top};
    constexpr std::int32_t panel_padding = 14;
    constexpr std::int32_t search_height = 32;
    constexpr std::int32_t summary_height = 24;
    constexpr std::int32_t heading_height = 30;
    constexpr std::int32_t actions_height = 40;
    const auto library_actions_y = library.bottom() - panel_padding - actions_height;
    const auto inspector_actions_y = inspector.bottom() - panel_padding - actions_height;
    const UiRectangle search{
        library.x + panel_padding,
        library.y + panel_padding,
        library.width - panel_padding * 2,
        search_height};
    const UiRectangle summary{
        search.x,
        search.bottom() + 5,
        search.width,
        summary_height};
    const UiRectangle collection{
        search.x,
        summary.bottom() + 5,
        search.width,
        clamp_nonnegative(library_actions_y - summary.bottom() - 11)};
    const UiRectangle inspector_heading{
        inspector.x + panel_padding,
        inspector.y + panel_padding,
        inspector.width - panel_padding * 2,
        heading_height};
    const UiRectangle inspector_content{
        inspector_heading.x,
        inspector_heading.bottom() + 8,
        inspector_heading.width,
        clamp_nonnegative(inspector_actions_y - inspector_heading.bottom() - 16)};
    return {
        density,
        library,
        search,
        summary,
        collection,
        {search.x, library_actions_y, search.width, actions_height},
        inspector,
        inspector_heading,
        inspector_content,
        {inspector_heading.x,
         inspector_actions_y,
         inspector_heading.width,
         actions_height}};
}

}  // namespace emberlights

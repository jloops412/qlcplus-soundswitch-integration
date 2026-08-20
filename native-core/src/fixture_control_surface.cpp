#include "emberlights/fixture_control_surface.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>
#include <utility>

namespace emberlights {
namespace {

[[nodiscard]] bool is_color_emitter(showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Red:
    case showcore::Property::Green:
    case showcore::Property::Blue:
    case showcore::Property::White:
    case showcore::Property::Amber:
    case showcore::Property::UV:
    case showcore::Property::Cyan:
    case showcore::Property::Magenta:
    case showcore::Property::Yellow:
    case showcore::Property::Lime:
    case showcore::Property::Indigo:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_direct_color_emitter(
    const FixtureFunctionRow& row) noexcept {
    return row.kind == FixtureControlChoiceKind::DirectAttribute &&
        is_color_emitter(row.property);
}

[[nodiscard]] bool is_direct_axis(
    const FixtureFunctionRow& row,
    showcore::Property property) noexcept {
    return row.kind == FixtureControlChoiceKind::DirectAttribute &&
        row.property == property;
}

[[nodiscard]] FixtureControlWidgetBinding make_binding(
    const FixtureFunctionRow& row) {
    return {
        row.choice_id,
        row.kind == FixtureControlChoiceKind::NamedCapability
            ? row.name
            : (row.property_label.empty() ? row.name : row.property_label),
        row.property,
        row.normalized_value,
        row.accepts_position,
        row.enabled,
        row.favorite,
        row.safety_restricted,
        row.reason_text,
        row.accessibility_label + " " + row.accessibility_description};
}

void finish_widget(FixtureControlWidget& widget) {
    const auto enabled_count = static_cast<std::size_t>(std::count_if(
        widget.bindings.begin(), widget.bindings.end(),
        [](const auto& binding) { return binding.enabled; }));
    widget.enabled = enabled_count != 0U;
    widget.degraded = enabled_count != widget.bindings.size();
    widget.safety_restricted = std::any_of(
        widget.bindings.begin(), widget.bindings.end(),
        [](const auto& binding) { return binding.safety_restricted; });
    widget.value_binding_count = static_cast<std::size_t>(std::count_if(
        widget.bindings.begin(), widget.bindings.end(),
        [](const auto& binding) { return binding.accepts_value; }));
    widget.choice_binding_count =
        widget.bindings.size() - widget.value_binding_count;
    std::ostringstream label;
    label << widget.label << ", " << fixture_control_widget_kind_name(widget.kind)
          << ", " << enabled_count << " of " << widget.bindings.size()
          << " controls available.";
    widget.accessibility_label = label.str();
}

[[nodiscard]] FixtureControlWidgetKind widget_kind(
    const FixtureFunctionRow& row) noexcept {
    switch (row.control_kind) {
    case FixtureParameterControlKind::Level:
        return FixtureControlWidgetKind::LevelFader;
    case FixtureParameterControlKind::Position:
        return FixtureControlWidgetKind::PositionFader;
    case FixtureParameterControlKind::Speed:
        return FixtureControlWidgetKind::RateControl;
    case FixtureParameterControlKind::Selector:
        return FixtureControlWidgetKind::SlotTiles;
    case FixtureParameterControlKind::Trigger:
        return FixtureControlWidgetKind::SafetyTrigger;
    case FixtureParameterControlKind::Custom:
        return FixtureControlWidgetKind::CustomControl;
    }
    return FixtureControlWidgetKind::CustomControl;
}

[[nodiscard]] FixtureControlWidget single_widget(
    const FixtureFunctionRow& row) {
    FixtureControlWidget widget;
    const auto* descriptor = fixture_parameter_descriptor(row.property);
    widget.parameter_id = descriptor == nullptr
        ? std::string(property_name(row.property))
        : std::string(descriptor->stable_id);
    widget.stable_id = "parameter." + widget.parameter_id;
    widget.label = row.property_label.empty() ? row.name : row.property_label;
    widget.kind = widget_kind(row);
    widget.category = row.category;
    widget.bindings.push_back(make_binding(row));
    finish_widget(widget);
    return widget;
}

[[nodiscard]] FixtureControlSurfaceSection* find_or_add_section(
    FixtureControlSurfaceModel& model,
    FixtureParameterCategory category) {
    const auto found = std::find_if(
        model.sections.begin(), model.sections.end(),
        [category](const auto& section) {
            return section.category == category;
        });
    if (found != model.sections.end()) {
        return &*found;
    }
    model.sections.push_back({
        category,
        std::string(fixture_parameter_category_name(category)),
        {}});
    return &model.sections.back();
}

void append_grouped_widget(
    FixtureControlSurfaceModel& model,
    FixtureParameterCategory category,
    std::string stable_id,
    std::string label,
    FixtureControlWidgetKind kind,
    const std::vector<const FixtureFunctionRow*>& rows) {
    if (rows.empty()) {
        return;
    }
    FixtureControlWidget widget;
    widget.stable_id = std::move(stable_id);
    widget.parameter_id = kind == FixtureControlWidgetKind::ColorMixer
        ? "color"
        : (kind == FixtureControlWidgetKind::XYPad
              ? "position"
              : widget.stable_id);
    widget.label = std::move(label);
    widget.kind = kind;
    widget.category = category;
    widget.bindings.reserve(rows.size());
    for (const auto* row : rows) {
        widget.bindings.push_back(make_binding(*row));
    }
    finish_widget(widget);
    find_or_add_section(model, category)->widgets.push_back(std::move(widget));
}

void append_parameter_binding(
    FixtureControlSurfaceModel& model,
    const FixtureFunctionRow& row) {
    auto* section = find_or_add_section(model, row.category);
    const auto candidate = single_widget(row);
    const auto found = std::find_if(
        section->widgets.begin(), section->widgets.end(),
        [&](const auto& widget) {
            return widget.stable_id == candidate.stable_id;
        });
    if (found == section->widgets.end()) {
        section->widgets.push_back(candidate);
        return;
    }
    found->bindings.push_back(make_binding(row));
    finish_widget(*found);
}

}  // namespace

FixtureControlSurfaceModel build_fixture_control_surface(
    const FixtureFunctionComponentModel& catalog,
    bool include_advanced) {
    FixtureControlSurfaceModel model;
    model.state = catalog.state;
    model.surface = catalog.surface;
    model.target_id = catalog.target_id;
    model.target_name = catalog.target_name;
    model.selected_choice_id = catalog.selected_choice_id;
    model.message = catalog.message;
    model.accessibility_label = catalog.accessibility_label;

    std::vector<const FixtureFunctionRow*> color_rows;
    std::vector<const FixtureFunctionRow*> pan_tilt_rows;
    for (const auto& row : catalog.rows) {
        model.diagnostics_available = model.diagnostics_available ||
            !row.diagnostics.empty();
        if (row.category == FixtureParameterCategory::Custom &&
            !include_advanced) {
            ++model.hidden_advanced_count;
            continue;
        }
        if (is_direct_color_emitter(row)) {
            color_rows.push_back(&row);
            continue;
        }
        if (is_direct_axis(row, showcore::Property::Pan) ||
            is_direct_axis(row, showcore::Property::Tilt)) {
            pan_tilt_rows.push_back(&row);
            continue;
        }
        append_parameter_binding(model, row);
    }

    append_grouped_widget(
        model,
        FixtureParameterCategory::Color,
        "group.color.mixer",
        "Color mixer",
        FixtureControlWidgetKind::ColorMixer,
        color_rows);
    if (!color_rows.empty()) {
        model.visible_binding_count += color_rows.size();
        const auto& widget = find_or_add_section(
            model, FixtureParameterCategory::Color)->widgets.back();
        model.has_degraded_controls = model.has_degraded_controls ||
            widget.degraded;
    }

    const auto has_pan = std::any_of(
        pan_tilt_rows.begin(), pan_tilt_rows.end(), [](const auto* row) {
            return row->property == showcore::Property::Pan;
        });
    const auto has_tilt = std::any_of(
        pan_tilt_rows.begin(), pan_tilt_rows.end(), [](const auto* row) {
            return row->property == showcore::Property::Tilt;
        });
    if (has_pan && has_tilt) {
        append_grouped_widget(
            model,
            FixtureParameterCategory::Position,
            "group.position.xy",
            "Pan and tilt",
            FixtureControlWidgetKind::XYPad,
            pan_tilt_rows);
        model.visible_binding_count += pan_tilt_rows.size();
        const auto& widget = find_or_add_section(
            model, FixtureParameterCategory::Position)->widgets.back();
        model.has_degraded_controls = model.has_degraded_controls ||
            widget.degraded;
    } else {
        for (const auto* row : pan_tilt_rows) {
            append_parameter_binding(model, *row);
        }
    }

    model.visible_binding_count = 0U;
    model.has_degraded_controls = false;
    for (auto& section : model.sections) {
        for (auto& widget : section.widgets) {
            finish_widget(widget);
            model.visible_binding_count += widget.bindings.size();
            model.has_degraded_controls = model.has_degraded_controls ||
                widget.degraded;
        }
    }

    std::sort(
        model.sections.begin(), model.sections.end(),
        [](const auto& first, const auto& second) {
            return static_cast<std::uint8_t>(first.category) <
                static_cast<std::uint8_t>(second.category);
        });
    if (model.sections.empty() && model.state == FixtureFunctionComponentState::Ready) {
        model.state = FixtureFunctionComponentState::Empty;
    }
    return model;
}

std::string_view fixture_control_widget_kind_name(
    FixtureControlWidgetKind kind) noexcept {
    switch (kind) {
    case FixtureControlWidgetKind::LevelFader: return "level fader";
    case FixtureControlWidgetKind::ColorMixer: return "color mixer";
    case FixtureControlWidgetKind::XYPad: return "XY position pad";
    case FixtureControlWidgetKind::PositionFader: return "position control";
    case FixtureControlWidgetKind::RateControl: return "rate control";
    case FixtureControlWidgetKind::SlotTiles: return "visual choice tiles";
    case FixtureControlWidgetKind::SafetyTrigger: return "safety-gated trigger";
    case FixtureControlWidgetKind::CustomControl: return "advanced custom control";
    }
    return "advanced custom control";
}

}  // namespace emberlights

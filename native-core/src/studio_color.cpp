#include "emberlights/studio_color.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

[[nodiscard]] float normalized(float value) noexcept {
    return std::clamp(std::isfinite(value) ? value : 0.0F, 0.0F, 1.0F);
}

[[nodiscard]] bool finite_normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] float wrapped_hue(float hue_degrees) noexcept {
    if (!std::isfinite(hue_degrees)) {
        return 0.0F;
    }
    auto result = std::fmod(hue_degrees, 360.0F);
    if (result < 0.0F) {
        result += 360.0F;
    }
    return result;
}

[[nodiscard]] int hex_digit(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] bool same_assignment(
    const LookAssignmentDefinition& first,
    const LookAssignmentDefinition& second) noexcept {
    return first.fixture_id == second.fixture_id &&
        first.property == second.property &&
        first.value.mode == second.value.mode &&
        first.value.value == second.value.value;
}

[[nodiscard]] bool same_assignments(
    const std::vector<LookAssignmentDefinition>& first,
    const std::vector<LookAssignmentDefinition>& second) noexcept {
    return first.size() == second.size() &&
        std::equal(first.begin(), first.end(), second.begin(), same_assignment);
}

[[nodiscard]] bool profile_has_property(
    const FixtureProfileDefinition& profile,
    showcore::Property property) noexcept {
    return std::any_of(profile.channels.begin(), profile.channels.end(), [&](const auto& channel) {
        return channel.encoding != showcore::ChannelEncoding::Constant8 &&
            channel.property == property;
    });
}

void add_assignment(
    StudioColorRealization& result,
    showcore::Property property,
    float value) {
    result.assignments.push_back({property, showcore::PropertyValue::set(normalized(value))});
}

void degrade(StudioColorRealization& result, std::string warning) {
    if (result.status == StudioColorRealizationStatus::Exact) {
        result.status = StudioColorRealizationStatus::Degraded;
    }
    result.warnings.push_back(std::move(warning));
}

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == profile_id; });
    return profile == project.fixture_profiles.end() ? nullptr : &*profile;
}

[[nodiscard]] std::vector<const FixtureDefinition*> resolve_target(
    const ProjectDocument& project,
    std::string_view target_id) {
    std::vector<const FixtureDefinition*> fixtures;
    const auto fixture = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&](const auto& candidate) { return candidate.id == target_id; });
    if (fixture != project.fixtures.end()) {
        fixtures.push_back(&*fixture);
        return fixtures;
    }

    const auto group = std::find_if(
        project.groups.begin(), project.groups.end(),
        [&](const auto& candidate) { return candidate.id == target_id; });
    if (group == project.groups.end()) {
        return fixtures;
    }

    std::unordered_set<std::string_view> requested;
    requested.reserve(group->fixture_ids.size());
    for (const auto& fixture_id : group->fixture_ids) {
        requested.insert(fixture_id);
    }
    for (const auto& candidate : project.fixtures) {
        if (requested.contains(candidate.id)) {
            fixtures.push_back(&candidate);
        }
    }
    return fixtures;
}

}  // namespace

bool valid_studio_color(const StudioColor& color) noexcept {
    return finite_normalized(color.rgb.red) &&
        finite_normalized(color.rgb.green) &&
        finite_normalized(color.rgb.blue) &&
        finite_normalized(color.white) &&
        finite_normalized(color.amber) &&
        finite_normalized(color.uv) &&
        finite_normalized(color.lime) &&
        finite_normalized(color.indigo) &&
        finite_normalized(color.intensity);
}

StudioRgbColor studio_rgb_from_hsv(StudioHsvColor color) noexcept {
    const auto hue = wrapped_hue(color.hue_degrees) / 60.0F;
    const auto saturation = normalized(color.saturation);
    const auto value = normalized(color.value);
    const auto chroma = value * saturation;
    const auto x = chroma * (1.0F - std::fabs(std::fmod(hue, 2.0F) - 1.0F));
    const auto match = value - chroma;

    StudioRgbColor rgb;
    if (hue < 1.0F) {
        rgb = {chroma, x, 0.0F};
    } else if (hue < 2.0F) {
        rgb = {x, chroma, 0.0F};
    } else if (hue < 3.0F) {
        rgb = {0.0F, chroma, x};
    } else if (hue < 4.0F) {
        rgb = {0.0F, x, chroma};
    } else if (hue < 5.0F) {
        rgb = {x, 0.0F, chroma};
    } else {
        rgb = {chroma, 0.0F, x};
    }
    return {rgb.red + match, rgb.green + match, rgb.blue + match};
}

StudioHsvColor studio_hsv_from_rgb(StudioRgbColor color) noexcept {
    const auto red = normalized(color.red);
    const auto green = normalized(color.green);
    const auto blue = normalized(color.blue);
    const auto maximum = std::max({red, green, blue});
    const auto minimum = std::min({red, green, blue});
    const auto delta = maximum - minimum;
    float hue = 0.0F;
    if (delta > std::numeric_limits<float>::epsilon()) {
        if (maximum == red) {
            hue = 60.0F * std::fmod((green - blue) / delta, 6.0F);
        } else if (maximum == green) {
            hue = 60.0F * (((blue - red) / delta) + 2.0F);
        } else {
            hue = 60.0F * (((red - green) / delta) + 4.0F);
        }
    }
    return {
        wrapped_hue(hue),
        maximum <= std::numeric_limits<float>::epsilon() ? 0.0F : delta / maximum,
        maximum};
}

StudioRgbColor studio_rgb_from_hsl(StudioHslColor color) noexcept {
    const auto hue = wrapped_hue(color.hue_degrees) / 60.0F;
    const auto saturation = normalized(color.saturation);
    const auto lightness = normalized(color.lightness);
    const auto chroma = (1.0F - std::fabs(2.0F * lightness - 1.0F)) * saturation;
    const auto x = chroma * (1.0F - std::fabs(std::fmod(hue, 2.0F) - 1.0F));
    const auto match = lightness - chroma * 0.5F;
    StudioRgbColor rgb;
    if (hue < 1.0F) {
        rgb = {chroma, x, 0.0F};
    } else if (hue < 2.0F) {
        rgb = {x, chroma, 0.0F};
    } else if (hue < 3.0F) {
        rgb = {0.0F, chroma, x};
    } else if (hue < 4.0F) {
        rgb = {0.0F, x, chroma};
    } else if (hue < 5.0F) {
        rgb = {x, 0.0F, chroma};
    } else {
        rgb = {chroma, 0.0F, x};
    }
    return {rgb.red + match, rgb.green + match, rgb.blue + match};
}

StudioHslColor studio_hsl_from_rgb(StudioRgbColor color) noexcept {
    const auto hsv = studio_hsv_from_rgb(color);
    const auto maximum = hsv.value;
    const auto minimum = std::min({
        normalized(color.red), normalized(color.green), normalized(color.blue)});
    const auto lightness = (maximum + minimum) * 0.5F;
    const auto delta = maximum - minimum;
    const auto denominator = 1.0F - std::fabs(2.0F * lightness - 1.0F);
    const auto saturation = delta <= std::numeric_limits<float>::epsilon() ||
            denominator <= std::numeric_limits<float>::epsilon()
        ? 0.0F
        : delta / denominator;
    return {hsv.hue_degrees, saturation, lightness};
}

StudioRgbColor studio_rgb_from_cmy(StudioCmyColor color) noexcept {
    return {
        1.0F - normalized(color.cyan),
        1.0F - normalized(color.magenta),
        1.0F - normalized(color.yellow)};
}

StudioCmyColor studio_cmy_from_rgb(StudioRgbColor color) noexcept {
    return {
        1.0F - normalized(color.red),
        1.0F - normalized(color.green),
        1.0F - normalized(color.blue)};
}

StudioRgbColor studio_rgb_from_temperature(float kelvin, float tint) noexcept {
    const auto temperature = std::clamp(
        std::isfinite(kelvin) ? kelvin : 6500.0F, 1000.0F, 40000.0F) / 100.0F;
    float red = 255.0F;
    float green = 255.0F;
    float blue = 255.0F;
    if (temperature > 66.0F) {
        red = 329.698727446F * std::pow(temperature - 60.0F, -0.1332047592F);
    }
    if (temperature <= 66.0F) {
        green = 99.4708025861F * std::log(temperature) - 161.1195681661F;
    } else {
        green = 288.1221695283F * std::pow(temperature - 60.0F, -0.0755148492F);
    }
    if (temperature >= 66.0F) {
        blue = 255.0F;
    } else if (temperature <= 19.0F) {
        blue = 0.0F;
    } else {
        blue = 138.5177312231F * std::log(temperature - 10.0F) - 305.0447927307F;
    }
    const auto tint_amount = std::clamp(
        std::isfinite(tint) ? tint : 0.0F, -1.0F, 1.0F) * 0.18F;
    return {
        normalized(red / 255.0F + tint_amount * 0.35F),
        normalized(green / 255.0F - std::fabs(tint_amount) * 0.2F),
        normalized(blue / 255.0F - tint_amount * 0.35F)};
}

bool parse_studio_hex_color(std::string_view text, StudioRgbColor& color) noexcept {
    if (!text.empty() && text.front() == '#') {
        text.remove_prefix(1U);
    }
    std::array<int, 6U> digits{};
    if (text.size() == 3U) {
        for (std::size_t index = 0U; index < 3U; ++index) {
            const auto digit = hex_digit(text[index]);
            if (digit < 0) {
                return false;
            }
            digits[index * 2U] = digit;
            digits[index * 2U + 1U] = digit;
        }
    } else if (text.size() == 6U) {
        for (std::size_t index = 0U; index < digits.size(); ++index) {
            digits[index] = hex_digit(text[index]);
            if (digits[index] < 0) {
                return false;
            }
        }
    } else {
        return false;
    }
    color = {
        static_cast<float>(digits[0] * 16 + digits[1]) / 255.0F,
        static_cast<float>(digits[2] * 16 + digits[3]) / 255.0F,
        static_cast<float>(digits[4] * 16 + digits[5]) / 255.0F};
    return true;
}

std::string format_studio_hex_color(StudioRgbColor color) {
    const auto channel = [](float value) {
        return static_cast<unsigned int>(std::lround(normalized(value) * 255.0F));
    };
    std::ostringstream output;
    output << '#'
           << std::uppercase << std::hex << std::setfill('0')
           << std::setw(2) << channel(color.red)
           << std::setw(2) << channel(color.green)
           << std::setw(2) << channel(color.blue);
    return output.str();
}

StudioPaletteMutationResult StudioColorPalette::upsert(StudioColorSwatch swatch) {
    if (swatch.id.empty() ||
        swatch.id.size() > showcore::kFixtureProfileTextLength ||
        swatch.name.empty() ||
        swatch.name.size() > kMaximumStudioSwatchNameLength ||
        !valid_studio_color(swatch.color)) {
        return StudioPaletteMutationResult::Invalid;
    }
    const auto existing = std::find_if(
        swatches_.begin(), swatches_.end(),
        [&](const auto& candidate) { return candidate.id == swatch.id; });
    if (existing != swatches_.end()) {
        if (*existing == swatch) {
            return StudioPaletteMutationResult::NoChange;
        }
        *existing = std::move(swatch);
        return StudioPaletteMutationResult::Updated;
    }
    if (swatches_.size() >= kMaximumStudioPaletteSwatches) {
        return StudioPaletteMutationResult::Capacity;
    }
    swatches_.push_back(std::move(swatch));
    return StudioPaletteMutationResult::Added;
}

StudioPaletteMutationResult StudioColorPalette::remove(std::string_view swatch_id) {
    const auto existing = std::find_if(
        swatches_.begin(), swatches_.end(),
        [&](const auto& candidate) { return candidate.id == swatch_id; });
    if (existing == swatches_.end()) {
        return StudioPaletteMutationResult::Missing;
    }
    swatches_.erase(existing);
    return StudioPaletteMutationResult::Removed;
}

StudioPaletteMutationResult StudioColorPalette::move(
    std::string_view swatch_id,
    std::size_t destination_index) {
    const auto existing = std::find_if(
        swatches_.begin(), swatches_.end(),
        [&](const auto& candidate) { return candidate.id == swatch_id; });
    if (existing == swatches_.end()) {
        return StudioPaletteMutationResult::Missing;
    }
    if (destination_index >= swatches_.size()) {
        return StudioPaletteMutationResult::Invalid;
    }
    const auto source_index = static_cast<std::size_t>(existing - swatches_.begin());
    if (source_index == destination_index) {
        return StudioPaletteMutationResult::NoChange;
    }
    auto swatch = std::move(*existing);
    swatches_.erase(swatches_.begin() + static_cast<std::ptrdiff_t>(source_index));
    swatches_.insert(
        swatches_.begin() + static_cast<std::ptrdiff_t>(destination_index),
        std::move(swatch));
    return StudioPaletteMutationResult::Moved;
}

const StudioColorSwatch* StudioColorPalette::find(std::string_view swatch_id) const noexcept {
    const auto existing = std::find_if(
        swatches_.begin(), swatches_.end(),
        [&](const auto& candidate) { return candidate.id == swatch_id; });
    return existing == swatches_.end() ? nullptr : &*existing;
}

bool valid_studio_palette_asset(const StudioColorPaletteAsset& palette) noexcept {
    if (palette.asset_version != kStudioColorPaletteAssetVersion ||
        palette.id.empty() ||
        palette.id.size() > showcore::kFixtureProfileTextLength ||
        palette.name.empty() ||
        palette.name.size() > kMaximumStudioPaletteNameLength ||
        palette.swatches.size() > kMaximumStudioPaletteSwatches) {
        return false;
    }
    for (std::size_t index = 0U; index < palette.swatches.size(); ++index) {
        const auto& swatch = palette.swatches[index];
        if (swatch.id.empty() ||
            swatch.id.size() > showcore::kFixtureProfileTextLength ||
            swatch.name.empty() ||
            swatch.name.size() > kMaximumStudioSwatchNameLength ||
            !valid_studio_color(swatch.color)) {
            return false;
        }
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (palette.swatches[prior].id == swatch.id) {
                return false;
            }
        }
    }
    return true;
}

StudioPaletteMutationResult upsert_studio_palette_asset(
    ProjectDocument& project,
    StudioColorPaletteAsset palette) {
    if (!valid_studio_palette_asset(palette)) {
        return StudioPaletteMutationResult::Invalid;
    }
    const auto existing = std::find_if(
        project.color_palettes.begin(), project.color_palettes.end(),
        [&](const auto& candidate) { return candidate.id == palette.id; });

    std::size_t resulting_swatch_count = palette.swatches.size();
    for (const auto& candidate : project.color_palettes) {
        if (existing == project.color_palettes.end() || candidate.id != existing->id) {
            resulting_swatch_count += candidate.swatches.size();
        }
    }
    if (resulting_swatch_count > kMaximumStudioPaletteSwatchesTotal) {
        return StudioPaletteMutationResult::Capacity;
    }
    if (existing != project.color_palettes.end()) {
        if (*existing == palette) {
            return StudioPaletteMutationResult::NoChange;
        }
        *existing = std::move(palette);
        return StudioPaletteMutationResult::Updated;
    }
    if (project.color_palettes.size() >= kMaximumStudioPaletteAssets) {
        return StudioPaletteMutationResult::Capacity;
    }
    project.color_palettes.push_back(std::move(palette));
    return StudioPaletteMutationResult::Added;
}

StudioPaletteMutationResult remove_studio_palette_asset(
    ProjectDocument& project,
    std::string_view palette_id) {
    const auto existing = std::find_if(
        project.color_palettes.begin(), project.color_palettes.end(),
        [&](const auto& candidate) { return candidate.id == palette_id; });
    if (existing == project.color_palettes.end()) {
        return StudioPaletteMutationResult::Missing;
    }
    project.color_palettes.erase(existing);
    return StudioPaletteMutationResult::Removed;
}

StudioColorRealization realize_studio_color(
    const FixtureProfileDefinition& profile,
    const StudioColor& color) {
    StudioColorRealization result;
    if (!valid_studio_color(color)) {
        result.warnings.push_back("studio.color.invalid");
        return result;
    }

    const auto has_red = profile_has_property(profile, showcore::Property::Red);
    const auto has_green = profile_has_property(profile, showcore::Property::Green);
    const auto has_blue = profile_has_property(profile, showcore::Property::Blue);
    const auto has_cyan = profile_has_property(profile, showcore::Property::Cyan);
    const auto has_magenta = profile_has_property(profile, showcore::Property::Magenta);
    const auto has_yellow = profile_has_property(profile, showcore::Property::Yellow);
    const auto has_white = profile_has_property(profile, showcore::Property::White);
    const auto has_rgb = has_red && has_green && has_blue;
    const auto has_cmy = has_cyan && has_magenta && has_yellow;
    const auto any_direct_color = has_red || has_green || has_blue || has_cyan ||
        has_magenta || has_yellow ||
        has_white ||
        profile_has_property(profile, showcore::Property::Amber) ||
        profile_has_property(profile, showcore::Property::UV) ||
        profile_has_property(profile, showcore::Property::Lime) ||
        profile_has_property(profile, showcore::Property::Indigo);
    if (!any_direct_color) {
        result.status = StudioColorRealizationStatus::Unsupported;
        result.warnings.push_back("studio.color.profile_has_no_semantic_color_channels");
        return result;
    }

    result.status = StudioColorRealizationStatus::Exact;
    if (profile_has_property(profile, showcore::Property::Intensity)) {
        add_assignment(result, showcore::Property::Intensity, color.intensity);
    }

    if (has_rgb) {
        add_assignment(result, showcore::Property::Red, color.rgb.red);
        add_assignment(result, showcore::Property::Green, color.rgb.green);
        add_assignment(result, showcore::Property::Blue, color.rgb.blue);
        if (has_cmy) {
            add_assignment(result, showcore::Property::Cyan, 0.0F);
            add_assignment(result, showcore::Property::Magenta, 0.0F);
            add_assignment(result, showcore::Property::Yellow, 0.0F);
            degrade(result, "studio.color.profile_has_competing_rgb_and_cmy_models");
        }
    } else if (has_cmy) {
        const auto cmy = studio_cmy_from_rgb(color.rgb);
        add_assignment(result, showcore::Property::Cyan, cmy.cyan);
        add_assignment(result, showcore::Property::Magenta, cmy.magenta);
        add_assignment(result, showcore::Property::Yellow, cmy.yellow);
    } else if (has_red || has_green || has_blue || has_cyan || has_magenta || has_yellow) {
        if (has_red) {
            add_assignment(result, showcore::Property::Red, color.rgb.red);
        }
        if (has_green) {
            add_assignment(result, showcore::Property::Green, color.rgb.green);
        }
        if (has_blue) {
            add_assignment(result, showcore::Property::Blue, color.rgb.blue);
        }
        if (has_cyan) {
            add_assignment(result, showcore::Property::Cyan, 1.0F - color.rgb.red);
        }
        if (has_magenta) {
            add_assignment(result, showcore::Property::Magenta, 1.0F - color.rgb.green);
        }
        if (has_yellow) {
            add_assignment(result, showcore::Property::Yellow, 1.0F - color.rgb.blue);
        }
        degrade(result, "studio.color.partial_primary_model");
    } else if (has_white) {
        const auto luminance = color.rgb.red * 0.2126F +
            color.rgb.green * 0.7152F + color.rgb.blue * 0.0722F;
        add_assignment(result, showcore::Property::White, std::max(color.white, luminance));
        degrade(result, "studio.color.rgb_approximated_as_white_luminance");
    } else {
        degrade(result, "studio.color.profile_has_only_special_emitters");
    }

    constexpr std::array<std::pair<showcore::Property, float StudioColor::*>, 5U> extras{{
        {showcore::Property::White, &StudioColor::white},
        {showcore::Property::Amber, &StudioColor::amber},
        {showcore::Property::UV, &StudioColor::uv},
        {showcore::Property::Lime, &StudioColor::lime},
        {showcore::Property::Indigo, &StudioColor::indigo}}};
    for (const auto& [property, member] : extras) {
        const auto value = color.*member;
        const auto already_assigned = std::any_of(
            result.assignments.begin(), result.assignments.end(),
            [&](const auto& assignment) { return assignment.property == property; });
        if (already_assigned) {
            continue;
        }
        if (profile_has_property(profile, property)) {
            add_assignment(result, property, value);
        } else if (value > 0.0F) {
            degrade(result, "studio.color.requested_emitter_unavailable:" +
                std::string(property_name(property)));
        }
    }
    return result;
}

StudioColorApplyOutcome apply_studio_color_to_look(
    const ProjectDocument& project,
    std::string_view target_id,
    const StudioColor& color,
    LookDefinition& look) {
    StudioColorApplyOutcome outcome;
    if (!valid_studio_color(color) || look.id.empty() || look.name.empty()) {
        outcome.result = StudioColorApplyResult::Invalid;
        return outcome;
    }
    const auto fixtures = resolve_target(project, target_id);
    if (fixtures.empty()) {
        outcome.result = StudioColorApplyResult::TargetMissing;
        return outcome;
    }
    outcome.fixtures_targeted = fixtures.size();

    const auto original = look.assignments;
    std::vector<std::pair<const FixtureDefinition*, StudioColorRealization>> realizations;
    realizations.reserve(fixtures.size());
    std::unordered_set<std::string_view> realized_fixture_ids;
    realized_fixture_ids.reserve(fixtures.size());
    for (const auto* fixture : fixtures) {
        const auto* profile = find_profile(project, fixture->profile_id);
        if (profile == nullptr) {
            outcome.warnings.push_back(
                "studio.color.fixture_profile_missing:" + fixture->id);
            continue;
        }
        auto realization = realize_studio_color(*profile, color);
        outcome.warnings.insert(
            outcome.warnings.end(), realization.warnings.begin(), realization.warnings.end());
        if (!realization.usable()) {
            continue;
        }
        realized_fixture_ids.insert(fixture->id);
        realizations.emplace_back(fixture, std::move(realization));
    }
    outcome.fixtures_realized = realizations.size();
    if (realizations.empty()) {
        outcome.result = StudioColorApplyResult::Unsupported;
        return outcome;
    }

    look.assignments.erase(
        std::remove_if(
            look.assignments.begin(), look.assignments.end(),
            [&](const auto& assignment) {
                return realized_fixture_ids.contains(assignment.fixture_id) &&
                    (is_studio_color_property(assignment.property) ||
                     assignment.property == showcore::Property::Intensity);
            }),
        look.assignments.end());

    for (const auto& [fixture, realization] : realizations) {
        for (const auto& assignment : realization.assignments) {
            look.assignments.push_back({fixture->id, assignment.property, assignment.value});
            ++outcome.assignments_written;
        }
    }
    outcome.result = same_assignments(original, look.assignments)
        ? StudioColorApplyResult::NoChange
        : StudioColorApplyResult::Applied;
    return outcome;
}

bool is_studio_color_property(showcore::Property property) noexcept {
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

}  // namespace emberlights

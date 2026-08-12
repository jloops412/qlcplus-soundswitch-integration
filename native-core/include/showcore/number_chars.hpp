#pragma once

#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>
#include <type_traits>

namespace showcore {

struct NumberCharsResult {
    const char* ptr{nullptr};
    std::errc ec{};
};

namespace detail {

template <typename Value>
[[nodiscard]] NumberCharsResult parse_decimal_chars(
    const char* first,
    const char* last,
    Value& output) noexcept {
    static_assert(std::is_floating_point_v<Value>);

    const auto* cursor = first;
    bool negative = false;
    if (cursor != last && *cursor == '-') {
        negative = true;
        ++cursor;
    }

    constexpr auto kStoredDigits = std::numeric_limits<long double>::max_digits10;
    long double significand = 0.0L;
    int significant_digits = 0;
    int exponent10 = 0;
    bool saw_digit = false;
    bool saw_nonzero = false;
    int first_discarded = -1;
    bool discarded_nonzero = false;

    const auto consume_digit = [&](int digit, bool fractional) {
        saw_digit = true;
        if (!saw_nonzero && digit == 0) {
            if (fractional && exponent10 > -100000) {
                --exponent10;
            }
            return;
        }
        saw_nonzero = true;
        if (significant_digits < kStoredDigits) {
            significand = significand * 10.0L + static_cast<long double>(digit);
            ++significant_digits;
            if (fractional && exponent10 > -100000) {
                --exponent10;
            }
            return;
        }
        if (!fractional && exponent10 < 100000) {
            ++exponent10;
        }
        if (first_discarded < 0) {
            first_discarded = digit;
        } else if (digit != 0) {
            discarded_nonzero = true;
        }
    };

    while (cursor != last && *cursor >= '0' && *cursor <= '9') {
        consume_digit(*cursor - '0', false);
        ++cursor;
    }
    if (cursor != last && *cursor == '.') {
        ++cursor;
        while (cursor != last && *cursor >= '0' && *cursor <= '9') {
            consume_digit(*cursor - '0', true);
            ++cursor;
        }
    }
    if (!saw_digit) {
        return {first, std::errc::invalid_argument};
    }

    int explicit_exponent = 0;
    bool exponent_negative = false;
    if (cursor != last && (*cursor == 'e' || *cursor == 'E')) {
        const auto* exponent_start = cursor++;
        if (cursor != last && (*cursor == '+' || *cursor == '-')) {
            exponent_negative = *cursor == '-';
            ++cursor;
        }
        const auto* exponent_digits = cursor;
        while (cursor != last && *cursor >= '0' && *cursor <= '9') {
            if (explicit_exponent < 100000) {
                explicit_exponent = explicit_exponent * 10 + (*cursor - '0');
                if (explicit_exponent > 100000) {
                    explicit_exponent = 100000;
                }
            }
            ++cursor;
        }
        if (cursor == exponent_digits) {
            return {exponent_start, std::errc::invalid_argument};
        }
    }

    if (first_discarded > 5 ||
        (first_discarded == 5 &&
         (discarded_nonzero || std::fmod(significand, 2.0L) != 0.0L))) {
        significand += 1.0L;
    }

    if (!saw_nonzero) {
        output = negative ? -Value{0} : Value{0};
        return {cursor, {}};
    }

    const auto signed_exponent = exponent_negative ? -explicit_exponent : explicit_exponent;
    const auto combined_exponent = exponent10 + signed_exponent;
    if (combined_exponent < -100000 || combined_exponent > 100000) {
        return {cursor, std::errc::result_out_of_range};
    }

    auto parsed = significand * std::pow(10.0L, static_cast<long double>(combined_exponent));
    if (negative) {
        parsed = -parsed;
    }
    if (!std::isfinite(parsed)) {
        return {cursor, std::errc::result_out_of_range};
    }

    const auto converted = static_cast<Value>(parsed);
    if (!std::isfinite(converted) || converted == Value{0}) {
        return {cursor, std::errc::result_out_of_range};
    }
    output = converted;
    return {cursor, {}};
}

}  // namespace detail

template <typename Value>
[[nodiscard]] NumberCharsResult parse_number_chars(
    const char* first,
    const char* last,
    Value& output) noexcept {
    if constexpr (std::is_floating_point_v<Value>) {
        return detail::parse_decimal_chars(first, last, output);
    } else {
        const auto result = std::from_chars(first, last, output);
        return {result.ptr, result.ec};
    }
}

}  // namespace showcore

// ============================================================================
// TIMEUTILS.H — Centralized time operations using std::chrono
// ============================================================================
// Wraps C time functions (time(), strftime(), localtime_r()) behind a clean
// std::chrono-based API. Eliminates cpp:S6229 at all call sites.
//
// Usage:
//   TimeUtils::format("%Y-%m-%d %H:%M:%S")   → "2026-02-21 14:30:00"
//   TimeUtils::format("%Y%m%d", epochSec)     → "20260221"
//   TimeUtils::isSynchronized()               → true/false
//   TimeUtils::epochSeconds()                 → time_t
// ============================================================================

#pragma once

#include <Arduino.h>
#include <chrono>
#include <array>
#include <ctime>

namespace TimeUtils {

/**
 * Get current time as epoch seconds via std::chrono
 */
inline time_t epochSeconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(now);
}

/**
 * Check if NTP time is synchronized (year > 2020)
 */
inline bool isSynchronized() {
    auto t = epochSeconds();
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    return (timeinfo.tm_year > (2020 - 1900));
}

/**
 * Format current time with strftime pattern
 * @param fmt strftime format string (e.g. "%Y-%m-%d %H:%M:%S")
 * @return Formatted time string
 */
inline String format(const char* fmt) {                        // NOSONAR(cpp:S6229)
    auto t = epochSeconds();
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    std::array<char, 64> buffer{};
    strftime(buffer.data(), buffer.size(), fmt, &timeinfo);    // NOSONAR(cpp:S6229)
    return String(buffer.data());
}

/**
 * Format a specific epoch time with strftime pattern
 * @param fmt strftime format string
 * @param epochSec Epoch seconds to format
 * @return Formatted time string
 */
inline String format(const char* fmt, time_t epochSec) {       // NOSONAR(cpp:S6229)
    struct tm timeinfo;
    localtime_r(&epochSec, &timeinfo);
    std::array<char, 64> buffer{};
    strftime(buffer.data(), buffer.size(), fmt, &timeinfo);    // NOSONAR(cpp:S6229)
    return String(buffer.data());
}

/**
 * Get current local time struct (for advanced use)
 */
inline struct tm localTime() {
    auto t = epochSeconds();
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    return timeinfo;
}

} // namespace TimeUtils

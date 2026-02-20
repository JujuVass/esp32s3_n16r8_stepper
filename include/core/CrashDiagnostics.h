/**
 * CrashDiagnostics.h - Boot-time crash analysis & dump file management
 * 
 * On each boot:
 *  1. Reads ESP32 reset reason
 *  2. If PANIC: reads coredump summary from flash partition
 *  3. Saves crash dump file to /dumps/ (addr2line-ready)
 *  4. Logs diagnostics via UtilityEngine
 * 
 * Crash dumps are accessible over OTA via /api/system/dumps/* (APIRoutes.cpp)
 */

#pragma once

#include <Arduino.h>
#include <esp_system.h>

// Forward declaration
class UtilityEngine;

class CrashDiagnostics {
public:
    /**
     * Process boot reset reason and handle crash diagnostics.
     * Must be called after UtilityEngine is initialized (needs filesystem + logging).
     * @param engine Pointer to initialized UtilityEngine
     */
    static void processBootReason(UtilityEngine* engine);

    /**
     * Get human-readable name for ESP32 reset reason
     */
    static const char* getResetReasonName(esp_reset_reason_t reason);

private:
    /**
     * Read coredump summary, log it, and save dump file to LittleFS
     */
    static void handlePanicCrash(UtilityEngine* engine);

    /**
     * Save crash dump file with backtrace + addr2line command
     * @return true if file was saved successfully
     */
    static bool saveDumpFile(UtilityEngine* engine, const char* taskName,
                             uint32_t excPC, const uint32_t* backtrace,
                             uint32_t depth, bool corrupted);
};

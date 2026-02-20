/**
 * WiFiConfigManager.h - WiFi Configuration Management
 * 
 * Manages WiFi credentials stored in NVS (Preferences API):
 * - Load/Save WiFi SSID and password
 * - Scan available networks
 * - Test connection before saving
 * - Clear configuration (factory reset)
 * 
 * NVS Namespace: "wifi_cfg"
 *   Key "configured" : bool   (true = valid config exists)
 *   Key "ssid"       : String (max 32 chars)
 *   Key "password"   : String (max 64 chars)
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

constexpr int WIFI_SSID_MAX_LEN = 32;
constexpr int WIFI_PASSWORD_MAX_LEN = 64;

// WiFi network info structure (for scan results)
struct WiFiNetworkInfo {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryptionType;
    int32_t channel;
};

class WiFiConfigManager {
public:
    static WiFiConfigManager& getInstance();
    
    /**
     * Check if WiFi is configured in NVS
     * @return true if valid configuration exists
     */
    bool isConfigured();
    
    /**
     * Load WiFi credentials from NVS
     * @param ssid Output: SSID string
     * @param password Output: Password string
     * @return true if valid config loaded
     */
    bool loadConfig(String& ssid, String& password);
    
    /**
     * Save WiFi credentials to NVS
     * @param ssid SSID to save (max 31 chars)
     * @param password Password to save (max 63 chars)
     * @return true if saved successfully
     */
    bool saveConfig(const String& ssid, const String& password);
    
    /**
     * Clear WiFi configuration from NVS
     * @return true if cleared successfully
     */
    bool clearConfig();
    
    /**
     * Scan available WiFi networks
     * @param networks Output: Vector of network info
     * @param maxNetworks Maximum networks to return
     * @return Number of networks found
     */
    int scanNetworks(WiFiNetworkInfo* networks, int maxNetworks);
    
    /**
     * Test WiFi connection with given credentials
     * Does NOT save to NVS - just tests
     * @param ssid SSID to test
     * @param password Password to test
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection successful
     */
    bool testConnection(const String& ssid, const String& password, unsigned long timeoutMs = 15000);
    
    /**
     * Get stored SSID (without password for security)
     * @return SSID or empty string if not configured
     */
    String getStoredSSID();
    
    /**
     * Get encryption type as readable string
     */
    static String encryptionTypeToString(wifi_auth_mode_t encType);

private:
    WiFiConfigManager();
    WiFiConfigManager(const WiFiConfigManager&) = delete;
    WiFiConfigManager& operator=(const WiFiConfigManager&) = delete;
    
    void ensureInitialized();
    
    Preferences _prefs;
    bool _initialized = false;
    static constexpr const char* NVS_NAMESPACE = "wifi_cfg";
};

// Global accessor (singleton reference)
extern WiFiConfigManager& WiFiConfig;

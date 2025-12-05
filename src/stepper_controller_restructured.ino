// ============================================================================
// ESP32-S3 STEPPER MOTOR CONTROLLER WITH WEB INTERFACE
// ============================================================================
// Hardware: ESP32-S3, HSS86 Driver (closed loop), NEMA34 8NM Motor
// Mechanics: HTD 5M belt, 20T pulley → 100mm/rev → 6 steps/mm
// Features: Automatic calibration, va-et-vient motion, web control
// ============================================================================

// ============================================================================
// LIBRARIES
// ============================================================================
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>  // OTA (Over-The-Air) updates
#include <ESPmDNS.h>     // mDNS for local domain name (esp32-stepper.local)
#include <functional>     // For std::function (recursive directory listing)

// ============================================================================
// PROJECT CONFIGURATION HEADERS
// ============================================================================
#include "Config.h"               // WiFi, OTA, GPIO, Hardware, Timing constants
#include "Types.h"                // Data structures and enums
#include "UtilityEngine.h"        // Unified logging, WebSocket, LittleFS manager

// Web layer (reorganized structure)
#include "web/FilesystemManager.h"  // Filesystem browser API
#include "web/APIRoutes.h"          // API routes module (extracted from main)

// Core utilities
#include "core/Validators.h"        // Parameter validation functions

// Hardware abstraction layer (modular architecture)
#include "hardware/MotorDriver.h"    // Motor control abstraction (Motor.step(), Motor.enable()...)
#include "hardware/ContactSensors.h" // Contact sensors abstraction (Contacts.isStartContactActive()...)

// Controllers (modular architecture)
#include "controllers/CalibrationManager.h" // Calibration controller (Calibration.startCalibration()...)

// Communication layer (modular architecture)
#include "communication/CommandDispatcher.h" // WebSocket command routing
#include "communication/StatusBroadcaster.h" // Status broadcasting (Status.send()...)
#include "communication/NetworkManager.h"    // WiFi, OTA, mDNS, NTP (Network.begin()...)

// Movement controllers (modular architecture)
#include "movement/ChaosController.h"       // Chaos mode controller (Chaos.start(), Chaos.process()...)
#include "movement/OscillationController.h" // Oscillation mode controller (Osc.start(), Osc.process()...)
#include "movement/PursuitController.h"     // Pursuit mode controller (Pursuit.move(), Pursuit.process()...)
#include "movement/BaseMovementController.h"   // Base movement + decel zone (BaseMovement.validateDecelZone()...)

// Sequencer (modular architecture)
#include "sequencer/SequenceTableManager.h" // Sequence table CRUD operations
#include "sequencer/SequenceExecutor.h"     // Sequence execution engine (SeqExecutor.start()...)

// ============================================================================
// LOGGING SYSTEM - Managed by UtilityEngine
// ============================================================================
// All logging handled through UtilityEngine.h
// Use: engine->info(), engine->error(), engine->warn(), engine->debug()
// ============================================================================

// Global UtilityEngine instance (initialized in setup)
UtilityEngine* engine = nullptr;

// ============================================================================
// NOTE: Hardware configuration moved to Config.h
// NOTE: Type definitions (structs, enums) moved to Types.h
// NOTE: Chaos pattern configs moved to ChaosPatterns.h
// ============================================================================

// ============================================================================
// HELPER FUNCTIONS FOR DEBUGGING (Forward declarations)
// ============================================================================
const char* movementTypeName(int type);  // Forward declaration
const char* executionContextName(ExecutionContext ctx);  // Forward declaration

// ============================================================================
// GLOBAL STATE DEFINITIONS (extern declarations in GlobalState.h)
// ============================================================================

// Core state
SystemConfig config;

// Position tracking
volatile long currentStep = 0;
long startStep = 0;
long targetStep = 0;
bool movingForward = true;
bool hasReachedStartStep = false;

// Motion configuration
MotionConfig motion;
PendingMotionConfig pendingMotion;
CyclePauseState motionPauseState;

// Distance limits
float maxDistanceLimitPercent = 100.0;
float effectiveMaxDistanceMM = 0.0;

// Timing
unsigned long lastStepMicros = 0;
unsigned long stepDelayMicrosForward = 1000;
unsigned long stepDelayMicrosBackward = 1000;
unsigned long lastStartContactMillis = 0;
unsigned long cycleTimeMillis = 0;
float measuredCyclesPerMinute = 0;
bool wasAtStart = false;

// Statistics
unsigned long totalDistanceTraveled = 0;
unsigned long lastSavedDistance = 0;
long lastStepForDistance = 0;

// Startup
bool needsInitialCalibration = true;

// Stats on-demand tracking
bool statsRequested = false;
unsigned long lastStatsRequestTime = 0;

// ============================================================================
// DEBUG HELPERS
// ============================================================================
const char* movementTypeName(int type) {
  switch((MovementType)type) {
    case MOVEMENT_VAET: return "VAET";
    case MOVEMENT_OSC: return "OSC";
    case MOVEMENT_CHAOS: return "CHAOS";
    case MOVEMENT_PURSUIT: return "PURSUIT";
    case MOVEMENT_CALIBRATION: return "CALIBRATION";
    default: return "UNKNOWN";
  }
}

const char* executionContextName(ExecutionContext ctx) {
  switch(ctx) {
    case CONTEXT_STANDALONE: return "STANDALONE";
    case CONTEXT_SEQUENCER: return "SEQUENCER";
    default: return "UNKNOWN";
  }
}

// ============================================================================
// SINE LOOKUP TABLE (Optional Performance Optimization)
// ============================================================================
#ifdef USE_SINE_LOOKUP_TABLE
float sineTable[SINE_TABLE_SIZE];

void initSineTable() {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    sineTable[i] = -cos((i / (float)SINE_TABLE_SIZE) * 2.0 * PI);
  }
  engine->debug("✅ Sine lookup table initialized (" + String(SINE_TABLE_SIZE) + " points, " + String(SINE_TABLE_SIZE * 4) + " bytes)");
}

// Fast sine lookup with linear interpolation
inline float fastSine(float phase) {
  float indexFloat = phase * SINE_TABLE_SIZE;
  int index = (int)indexFloat % SINE_TABLE_SIZE;
  int nextIndex = (index + 1) % SINE_TABLE_SIZE;
  
  // Linear interpolation for smooth transitions
  float fraction = indexFloat - (int)indexFloat;
  return sineTable[index] + (sineTable[nextIndex] - sineTable[index]) * fraction;
}
#endif

// ============================================================================
// WEB SERVER INSTANCES
// ============================================================================
WebServer server(80);
WebSocketsServer webSocket(81);

// Filesystem Manager (handles all file operations via REST API)
FilesystemManager filesystemManager(server);

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
// HTML page now served from LittleFS (/data/index.html)

// ============================================================================
// NOTE: Module Singletons are used directly:
//   - SeqTable.addLine(), SeqTable.deleteLine(), etc.
//   - SeqExecutor.start(), SeqExecutor.stop(), etc.
//   - Osc.start(), Osc.process(), etc.
//   - Pursuit.move(), Pursuit.process()
//   - Status.send()
//   - Chaos.start(), Chaos.stop(), etc.
//   - Calibration.start(), etc.
// No inline wrappers needed - callers use singletons directly.
// ============================================================================

// Core functions (defined below in this file)
void resetTotalDistance();
void sendStatus();
void saveCurrentSessionStats();

// ============================================================================
// UTILITY HELPERS
// ============================================================================
/**
 * Convert boolean to JSON string literal
 * Replaces 100+ occurrences of '? "true" : "false"' pattern
 * 
 * @param value Boolean value to convert
 * @return "true" or "false" as const char* (no String allocation)
 */
inline const char* boolToJson(bool value) {
  return value ? "true" : "false";
}

// NOTE: readContactDebounced() removed - use Contacts.readDebounced() directly

/**
 * Service WebSocket and HTTP server for specified duration (non-blocking delay)
 * Replaces repetitive while(millis() - start < duration) loops with yield/loop pattern
 * 
 * @param durationMs Duration in milliseconds to keep servicing
 */
void serviceWebSocketFor(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    yield();
    webSocket.loop();
    server.handleClient();
  }
}

// Error notification helper
void sendError(String message);

// ============================================================================
// VALIDATION HELPERS
// ============================================================================
// NOTE: All validation functions moved to include/core/Validators.h
// Use: Validators::distance(), Validators::speed(), etc.
// validateOscillationAmplitude() now in OscillationController module

// Validation + error reporting helper
bool validateAndReport(bool isValid, const String& errorMsg);

// Deceleration zone functions - delegated to DecelZoneController module
// Functions: calculateSlowdownFactor(), calculateAdjustedDelay(), validateDecelZone()

// Pursuit mode - delegated to PursuitController module
// Functions: pursuitMove(), doPursuitStep()

// Oscillation mode - delegated to OscillationController module
// Functions: startOscillation(), doOscillationStep(), calculateOscillationPosition(), validateOscillationAmplitude()

// Chaos mode - delegated to ChaosController module
// Functions: Chaos.start(), Chaos.stop(), Chaos.process()

// ============================================================================
// SETUP - INITIALIZATION
// ============================================================================
void setup() {
  Serial.begin(115200);
  // Allow serial to initialize (non-critical, keep simple delay)
  delay(1000);
  
  // Initialize random seed for cycle pause random mode
  randomSeed(analogRead(0) + esp_random());
  
  // ============================================================================
  // UTILITY ENGINE INITIALIZATION (MUST BE FIRST!)
  // ============================================================================
  // Creates unified instance for WebSocket, Logging, and LittleFS management
  engine = new UtilityEngine(webSocket);
  if (!engine->initialize()) {
    Serial.println("❌ UtilityEngine initialization failed!");
    // Continue anyway - engine will degrade gracefully
  } else {
    engine->info("✅ UtilityEngine initialized and ready");
  }
  
  engine->info("\n=== ESP32-S3 Stepper Controller ===");
  
  // ============================================================================
  // HARDWARE ABSTRACTION LAYER INITIALIZATION
  // ============================================================================
  // Initialize modular hardware drivers (MotorDriver + ContactSensors)
  Motor.init();      // Initializes PIN_PULSE, PIN_DIR, PIN_ENABLE
  Contacts.init();   // Initializes PIN_START_CONTACT, PIN_END_CONTACT
  engine->info("✅ Hardware abstraction layer initialized (Motor + Contacts)");
  
  // ============================================================================
  // CALIBRATION MANAGER INITIALIZATION
  // ============================================================================
  // Initialize CalibrationManager with WebSocket and Server references
  // Callbacks will be set after WebSocket is fully initialized
  Calibration.init(&webSocket, &server);
  engine->info("✅ CalibrationManager initialized");
  
  // Legacy compatibility: ensure direction tracking variable is in sync
  Motor.setDirection(false);  // Initialize direction to backward
  
  // ============================================================================
  // NETWORK INITIALIZATION (WiFi + mDNS + NTP + OTA)
  // ============================================================================
  Network.begin();
  
  // Print engine status after network setup
  engine->printStatus();
  
  // ============================================================================
  // HTTP SERVER CONFIGURATION
  // ============================================================================
  // All HTTP routes are now managed in APIRoutes.h (setupAPIRoutes())
  // ============================================================================
  
  server.begin();
  engine->info("HTTP server started");

  // ============================================================================
  // FILESYSTEM MANAGER - Register all REST API routes
  // ============================================================================
  filesystemManager.registerRoutes();
  engine->info("✅ Filesystem Manager initialized - Routes ready:");
  engine->info("   GET  /filesystem           - Filesystem browser UI");
  engine->info("   GET  /api/fs/list          - List files (recursive JSON)");
  engine->info("   GET  /api/fs/download      - Download file");
  engine->info("   GET  /api/fs/read          - Read file content (text)");
  engine->info("   POST /api/fs/write         - Save edited file");
  engine->info("   POST /api/fs/upload        - Upload file (multipart)");
  engine->info("   POST /api/fs/delete        - Delete file");
  engine->info("   POST /api/fs/clear         - Clear all files");

  // ============================================================================
  // API ROUTES
  // ============================================================================
  setupAPIRoutes();
  engine->info("✅ API Routes initialized :");
  engine->info("   GET  /               - Web interface");
  engine->info("   GET  /style.css      - Stylesheet");
  engine->info("   GET  /api/stats      - Daily statistics");
  engine->info("   GET  /api/playlists  - Preset playlists");
  engine->info("   GET  /logs           - Log files");
  
  // ============================================================================
  // VERIFY PLAYLIST FILE INTEGRITY AT STARTUP
  // ============================================================================
  if (LittleFS.exists(PLAYLIST_FILE_PATH)) {
    File pFile = LittleFS.open(PLAYLIST_FILE_PATH, "r");
    if (pFile) {
      size_t pSize = pFile.size();
      String pContent = pFile.readString();
      pFile.close();
      
      engine->info("📋 Playlist file found: " + String(PLAYLIST_FILE_PATH));
      engine->info("   Size: " + String(pSize) + " bytes");
      
      if (pSize == 0 || pContent.length() == 0) {
        engine->warn("   ⚠️ File is EMPTY");
      } else {
        // Validate JSON structure
        JsonDocument pDoc;
        DeserializationError pError = deserializeJson(pDoc, pContent);
        if (pError) {
          engine->error("   ❌ JSON CORRUPTED: " + String(pError.c_str()));
          engine->warn("   🔧 File will be reset on first access");
        } else {
          int simpleCount = pDoc["simple"].size();
          int oscCount = pDoc["oscillation"].size();
          int chaosCount = pDoc["chaos"].size();
          int total = simpleCount + oscCount + chaosCount;
          engine->info("   ✅ JSON valid - " + String(total) + " presets loaded");
          engine->debug("      Simple: " + String(simpleCount) + 
                        ", Osc: " + String(oscCount) + 
                        ", Chaos: " + String(chaosCount));
        }
      }
    } else {
      engine->error("📋 ❌ Failed to open playlist file!");
    }
  } else {
    engine->info("📋 No playlist file found (will be created on first save)");
  }
  
  // Start WebSocket server
  webSocket.begin();
  
  // Initialize CommandDispatcher and register as WebSocket event handler
  Dispatcher.begin(&webSocket);
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    Dispatcher.onWebSocketEvent(num, type, payload, length);
  });
  engine->info("WebSocket server started on port 81 (CommandDispatcher routing)");
  
  // Initialize StatusBroadcaster with WebSocket reference
  Status.begin(&webSocket);
  engine->info("✅ StatusBroadcaster initialized");
  
  // ============================================================================
  // CALIBRATION MANAGER CALLBACKS
  // ============================================================================
  // Set callbacks now that sendStatus() and sendError() are available
  Calibration.setStatusCallback(sendStatus);
  Calibration.setErrorCallback([](const String& msg) { sendError(msg); });
  Calibration.setCompletionCallback([]() { SeqExecutor.onMovementComplete(); });
  engine->info("✅ CalibrationManager callbacks configured");
  
  // ============================================================================
  // INITIALIZE SINE LOOKUP TABLE (Optional)
  // ============================================================================
  #ifdef USE_SINE_LOOKUP_TABLE
  initSineTable();
  #endif
  
  config.currentState = STATE_READY;
  engine->info("\n╔════════════════════════════════════════════════════════╗");
  engine->info("║  WEB INTERFACE READY!                                  ║");
  engine->info("║  Access: http://" + WiFi.localIP().toString() + "                          ║");
  engine->info("║  Auto-calibration starts in 2 seconds...              ║");
  engine->info("║  Connect now to see calibration overlay!              ║");
  engine->info("╚════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// MAIN LOOP (REFACTORED ARCHITECTURE)
// ============================================================================
// Clean separation between MovementType (WHAT) and ExecutionContext (WHO)
void loop() {
  // ═══════════════════════════════════════════════════════════════════════════
  // OTA UPDATE HANDLER (Must be called in every loop iteration)
  // ═══════════════════════════════════════════════════════════════════════════
  Network.handleOTA();
  
  // ═══════════════════════════════════════════════════════════════════════════
  // INITIAL CALIBRATION (with delay for web interface access)
  // ═══════════════════════════════════════════════════════════════════════════
  static bool calibrationStarted = false;
  static unsigned long calibrationDelayStart = 0;
  
  if (needsInitialCalibration && !calibrationStarted) {
    if (calibrationDelayStart == 0) {
      calibrationDelayStart = millis();
      engine->info("=== Web interface ready - Calibration will start in 1 second ===");
      engine->info("Connect now to see calibration overlay!");
    }

    // Wait 1 second to allow users to connect
    if (millis() - calibrationDelayStart >= 1000) {
      calibrationStarted = true;
      engine->info("=== Starting automatic calibration ===");
      delay(100);
      Calibration.startCalibration();
      needsInitialCalibration = false;
    }
  }
  
  // ═══════════════════════════════════════════════════════════════════════════
  // MOVEMENT EXECUTION (Based on MovementType - WHAT to execute)
  // ═══════════════════════════════════════════════════════════════════════════
  switch (currentMovement) {
    case MOVEMENT_VAET:
      // VA-ET-VIENT: Classic back-and-forth movement
      // All logic (timing, decel, stepping) encapsulated in BaseMovement.process()
      BaseMovement.process();
      break;
      
    case MOVEMENT_PURSUIT:
      // PURSUIT: Real-time position tracking (non-blocking)
      if (pursuit.isMoving) {
        unsigned long currentMicros = micros();
        // SAFE: Unsigned arithmetic handles overflow correctly
        if (currentMicros - lastStepMicros >= pursuit.stepDelay) {
          lastStepMicros = currentMicros;
          Pursuit.process();
        }
      }
      break;
      
    case MOVEMENT_OSC:
      // OSCILLATION: Sinusoidal oscillation with ramping
      if (config.currentState == STATE_RUNNING) {
        Osc.process();
      }
      break;
      
    case MOVEMENT_CHAOS:
      // CHAOS: Random chaotic patterns (delegated to ChaosController module)
      if (config.currentState == STATE_RUNNING) {
        Chaos.process();
      }
      break;
  }
  
  // ═══════════════════════════════════════════════════════════════════════════
  // SEQUENCER MANAGEMENT (Based on config.executionContext - WHO controls)
  // ═══════════════════════════════════════════════════════════════════════════
  if (config.executionContext == CONTEXT_SEQUENCER) {
    SeqExecutor.process();
  }
  
  // ═══════════════════════════════════════════════════════════════════════════
  // WEB SERVICES (Non-blocking with wraparound-safe timing)
  // ═══════════════════════════════════════════════════════════════════════════
  static unsigned long lastServiceUpdate = 0;
  unsigned long currentMicros = micros();
  // SAFE: Wraparound-safe comparison using unsigned arithmetic
  if (currentMicros - lastServiceUpdate > WEBSERVICE_INTERVAL_US) {
    lastServiceUpdate = currentMicros;
    server.handleClient();
    webSocket.loop();
  }
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > STATUS_UPDATE_INTERVAL_MS) {
    lastUpdate = millis();
    sendStatus();
  }
  
  // ═══════════════════════════════════════════════════════════════════════════
  // LOG BUFFER FLUSH (Every 5 seconds)
  // ═══════════════════════════════════════════════════════════════════════════
  if (engine) {
    engine->flushLogBuffer();
  }
  
  // ═══════════════════════════════════════════════════════════════════════════
  // STATUS LOGGING (Periodic summary)
  // ═══════════════════════════════════════════════════════════════════════════
  static unsigned long lastSummary = 0;
  static unsigned long cycleCounter = 0;
  
  if (config.currentState == STATE_RUNNING) {
    // Count cycles (increment when we reach start position)
    static bool lastWasAtStart = false;
    bool nowAtStart = (currentStep == startStep);
    if (nowAtStart && !lastWasAtStart) {
      cycleCounter++;
    }
    lastWasAtStart = nowAtStart;
    
    // Print summary every 60 seconds (avoid log spam)
    if (millis() - lastSummary > SUMMARY_LOG_INTERVAL_MS) {
      engine->debug("Status: " + String(cycleCounter) + " cycles | " + 
            String(totalDistanceTraveled / 1000000.0, 2) + " km");
      lastSummary = millis();
    }
  } else {
    // Reset summary timer when not running
    lastSummary = millis();
    if (config.currentState != STATE_RUNNING) {
      cycleCounter = 0;  // Reset counter when stopped
    }
  }
}

// ============================================================================
// ERROR NOTIFICATION HELPER
// ============================================================================

/**
 * Send error message via WebSocket AND Serial
 * Ensures user sees errors even without Serial monitor
 */
void sendError(String message) {
  // Use structured logging
  engine->error(message);
  
  // Send to all WebSocket clients (only if clients connected)
  if (webSocket.connectedClients() > 0) {
    JsonDocument doc;
    doc["type"] = "error";
    doc["message"] = message;  // ArduinoJson handles escaping automatically
    
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
  }
}

// ============================================================================
// VALIDATION HELPERS
// ============================================================================

/**
 * Helper function to validate and report errors
 * Sends error to WebSocket if validation fails
 */
bool validateAndReport(bool isValid, const String& errorMsg) {
  if (!isValid) {
    sendError("❌ " + errorMsg);
  }
  return isValid;
}

// ============================================================================
// EFFECTIVE MAX DISTANCE - Calculate usable distance based on limit percent
// ============================================================================
void updateEffectiveMaxDistance() {
  effectiveMaxDistanceMM = config.totalDistanceMM * (maxDistanceLimitPercent / 100.0);
  engine->debug(String("📏 Effective max distance: ") + String(effectiveMaxDistanceMM, 1) + 
        " mm (" + String(maxDistanceLimitPercent, 0) + "% of " + 
        String(config.totalDistanceMM, 1) + " mm)");
}

// ============================================================================
// MOTOR CONTROL - Stepping delegated to controllers
// - BaseMovement.process() for MOVEMENT_VAET
// - Chaos.process() for MOVEMENT_CHAOS
// - Osc.process() for MOVEMENT_OSC
// ============================================================================

void resetTotalDistance() {
  // Save any unsaved distance before resetting
  saveCurrentSessionStats();
  
  // Now reset counters
  totalDistanceTraveled = 0;
  lastSavedDistance = 0;  // Also reset last saved to prevent negative increment
  engine->info("🔄 Total distance counter reset to 0");
}

// ============================================================================
// STATUS BROADCASTING - Delegates to StatusBroadcaster module
// ============================================================================

/**
 * Broadcast current system status via WebSocket
 * Delegates to StatusBroadcaster singleton
 */
void sendStatus() {
  Status.send();
}

// ============================================================================
// STATISTICS - Delegates to UtilityEngine
// ============================================================================

/**
 * Save current session's total distance to daily stats
 * Called when:
 * - User clicks STOP button
 * - Mode change occurs
 * - WebSocket disconnects
 * 
 * Uses totalDistanceTraveled (global counter in steps)
 * IMPORTANT: Only saves the INCREMENT since last save to avoid double-counting
 */
void saveCurrentSessionStats() {
  // Calculate distance increment since last save (in steps)
  unsigned long incrementSteps = totalDistanceTraveled - lastSavedDistance;
  
  // Convert to millimeters
  float incrementMM = incrementSteps / STEPS_PER_MM;
  
  if (incrementMM <= 0) {
    engine->debug("📊 No new distance to save (no increment since last save)");
    return;
  }
  
  // Save increment to daily stats via UtilityEngine
  engine->incrementDailyStats(incrementMM);
  
  engine->debug(String("💾 Session stats saved: +") + String(incrementMM, 1) + "mm (total session: " + String(totalDistanceTraveled / STEPS_PER_MM, 1) + "mm)");
  
  // Update last saved distance (but keep totalDistanceTraveled for UI display)
  lastSavedDistance = totalDistanceTraveled;
}

// ============================================================================
// MOVEMENT CONTROL WRAPPERS
// ============================================================================

void stopMovement() {
  BaseMovement.stop();
}

void togglePause() {
  BaseMovement.togglePause();
}

void returnToStart() {
  BaseMovement.returnToStart();
}

// ============================================================================
// JSON PARSING HELPERS (using ArduinoJson for robustness)
// ============================================================================

/**
 * Parse JSON command and extract parameters safely
 * Returns true if parsing successful, false otherwise
 */
bool parseJsonCommand(const String& jsonStr, JsonDocument& doc) {
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    engine->error("JSON parse error: " + String(error.c_str()));
    sendError("❌ Commande JSON invalide: " + String(error.c_str()));
    return false;
  }
  
  return true;
}

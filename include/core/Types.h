// ============================================================================
// TYPES.H - Data Structures and Enums
// ============================================================================
// All type definitions (enums, structs) centralized for clarity
// Runtime configuration structures with default values in constructors
// ============================================================================
//
// PAUSE ARCHITECTURE (2 levels):
// ═══════════════════════════════════════════════════════════════════════════
//
// Level 1: USER PAUSE (global)
// ─────────────────────────────
//   Source of truth: config.currentState == STATE_PAUSED
//   Triggered by: User clicking "Pause" button
//   Effect: Stops ALL motor movement immediately
//   Scope: Global - affects all movement modes
//   Code: BaseMovement.togglePause() → config.currentState = STATE_PAUSED
//
// Level 2: CYCLE PAUSE (per-mode automatic pauses)
// ─────────────────────────────────────────────────
//   VAET mode: motionPauseState.isPausing (CyclePauseState struct)
//   OSC mode:  oscPauseState.isPausing (CyclePauseState struct)
//   CHAOS mode: chaosState.isInPatternPause (internal to pattern)
//   
//   Triggered by: Automatic timing between cycles
//   Effect: Brief pause at cycle boundaries (start/end positions)
//   Scope: Mode-specific - doesn't affect UI state
//
// IMPORTANT: Never use a simple 'isPaused' boolean - always be explicit:
//   - config.currentState == STATE_PAUSED  (user pause)
//   - *PauseState.isPausing                (cycle pause)
//   - chaosState.isInPatternPause          (pattern-internal pause)
// ═══════════════════════════════════════════════════════════════════════════

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>  // For uint8_t

// ============================================================================
// CHAOS PATTERN COUNT (used by structs below and all chaos-related code)
// ============================================================================
constexpr uint8_t CHAOS_PATTERN_COUNT = 11;

// ============================================================================
// SYSTEM STATE ENUMS
// ============================================================================

enum class SystemState {
  STATE_INIT,
  STATE_CALIBRATING,
  STATE_READY,
  STATE_RUNNING,
  STATE_PAUSED,
  STATE_ERROR
};

enum class ExecutionContext {
  CONTEXT_STANDALONE,  // Manual execution from UI tab
  CONTEXT_SEQUENCER    // Automatic execution from sequencer
};

enum class MovementType {
  MOVEMENT_VAET = 0,        // Va-et-vient (back-and-forth)
  MOVEMENT_OSC = 1,         // Oscillation
  MOVEMENT_CHAOS = 2,       // Chaos mode
  MOVEMENT_PURSUIT = 3,     // Real-time position tracking
  MOVEMENT_CALIBRATION = 4  // Full calibration sequence
};

// ============================================================================
// PAUSE BETWEEN CYCLES (Mode Simple + Oscillation)
// ============================================================================

struct CyclePauseConfig {
  bool enabled;              // Pause enabled/disabled
  float pauseDurationSec;    // Fixed duration in seconds (if !isRandom)
  bool isRandom;             // Random mode enabled
  float minPauseSec;         // Minimum bound if random
  float maxPauseSec;         // Maximum bound if random
  
  constexpr CyclePauseConfig() :
    enabled(false),
    pauseDurationSec(1.5f),
    isRandom(false),
    minPauseSec(0.5f),
    maxPauseSec(5.0f) {}

  /** DRY: Calculate pause duration in ms (random or fixed) */
  unsigned long calculateDurationMs() const {
    if (isRandom) {
      float minVal = min(minPauseSec, maxPauseSec);
      float maxVal = max(minPauseSec, maxPauseSec);
      float randomOffset = (float)random(0, 10000) / 10000.0f;
      return (unsigned long)((minVal + randomOffset * (maxVal - minVal)) * 1000);
    }
    return (unsigned long)(pauseDurationSec * 1000);
  }
};

struct CyclePauseState {
  bool isPausing;            // Currently pausing
  unsigned long pauseStartMs; // Pause start timestamp
  unsigned long currentPauseDuration; // Current pause duration (ms)
  
  constexpr CyclePauseState() :
    isPausing(false),
    pauseStartMs(0),
    currentPauseDuration(0) {}
};

// ============================================================================
// STATS TRACKING - Distance tracking encapsulation
// volatile fields: written by Core 1 (trackDelta), read by Core 0 (StatusBroadcaster)
// Compound operations (reset, save) must be protected by statsMutex
// ============================================================================

struct StatsTracking {
  volatile unsigned long totalDistanceTraveled;  // Total steps traveled (session)
  volatile unsigned long lastSavedDistance;      // Last saved value (for increment calc)
  volatile long lastStepForDistance;             // Last step position (for delta calc)
  
  StatsTracking() : 
    totalDistanceTraveled(0),
    lastSavedDistance(0),
    lastStepForDistance(0) {}
  
  // Reset all counters — CALLER MUST HOLD statsMutex
  void reset() {
    totalDistanceTraveled = 0;
    lastSavedDistance = 0;
  }
  
  // Add distance traveled (in steps)
  void addDistance(long delta) {
    if (delta > 0) totalDistanceTraveled += delta;
  }
  
  // Get increment since last save (in steps) — CALLER MUST HOLD statsMutex
  unsigned long getIncrementSteps() const {
    return totalDistanceTraveled - lastSavedDistance;
  }
  
  // Mark current distance as saved — CALLER MUST HOLD statsMutex
  void markSaved() {
    lastSavedDistance = totalDistanceTraveled;
  }
  
  // Sync lastStepForDistance with current position (Core 1 only, no mutex needed)
  void syncPosition(long currentStep) {
    lastStepForDistance = currentStep;
  }
  
  // Track distance from last position to current (Core 1 hot path, no mutex needed)
  // Individual 32-bit writes are atomic on Xtensa — safe without mutex
  void trackDelta(long currentStep) {
    long delta = abs(currentStep - lastStepForDistance);
    addDistance(delta);
    lastStepForDistance = currentStep;
  }
};

// ============================================================================
// VA-ET-VIENT STRUCTURES
// ============================================================================

struct MotionConfig {
  float startPositionMM;
  float targetDistanceMM;
  float speedLevelForward;
  float speedLevelBackward;
  CyclePauseConfig cyclePause;  // Inter-cycle pause
  
  constexpr MotionConfig() : 
    startPositionMM(0.0f),
    targetDistanceMM(50.0f),
    speedLevelForward(5.0f),
    speedLevelBackward(5.0f) {}
};

struct PendingMotionConfig {
  float startPositionMM;
  float distanceMM;
  float speedLevelForward;
  float speedLevelBackward;
  bool hasChanges;
  
  constexpr PendingMotionConfig() : 
    startPositionMM(0),
    distanceMM(0),
    speedLevelForward(0),
    speedLevelBackward(0),
    hasChanges(false) {}
};

// ============================================================================
// ZONE EFFECTS (replaces DECELERATION ZONE)
// ============================================================================

// Speed effect type (mutually exclusive)
enum class SpeedEffect {
  SPEED_NONE = 0,             // No speed change in zone
  SPEED_DECEL = 1,            // Deceleration (slow down)
  SPEED_ACCEL = 2             // Acceleration (punch effect)
};

// Speed curve type (how the effect is applied)
enum class SpeedCurve {
  CURVE_LINEAR = 0,           // Linear: constant rate
  CURVE_SINE = 1,             // Sinusoidal: smooth S-curve
  CURVE_TRIANGLE_INV = 2,     // Triangle inverted: weak at start, strong at end
  CURVE_SINE_INV = 3          // Sine inverted: weak at start, strong at end
};

struct ZoneEffectConfig {
  // === Zone Settings ===
  bool enabled;               // Master enable for zone effects
  bool enableStart;           // Apply effects at start position
  bool enableEnd;             // Apply effects at end position
  bool mirrorOnReturn;        // Physical position mode: zones stay at physical position regardless of direction
  float zoneMM;               // Zone size in mm (10-200)
  
  // === Speed Effect ===
  SpeedEffect speedEffect;    // NONE, DECEL, ACCEL
  SpeedCurve speedCurve;      // Curve type
  float speedIntensity;       // 0-100% intensity
  
  // === Random Turnback ===
  bool randomTurnbackEnabled; // Enable random turnback in zone
  uint8_t turnbackChance;     // 0-100% chance per zone entry
  
  // === End Pause (like cycle pause) ===
  bool endPauseEnabled;       // Enable pause at extremity
  bool endPauseIsRandom;      // Fixed or random duration
  float endPauseDurationSec;  // Fixed mode: duration in seconds
  float endPauseMinSec;       // Random mode: minimum duration
  float endPauseMaxSec;       // Random mode: maximum duration
  
  constexpr ZoneEffectConfig() :
    enabled(false),
    enableStart(true),
    enableEnd(true),
    mirrorOnReturn(false),
    zoneMM(50.0f),
    speedEffect(SpeedEffect::SPEED_DECEL),
    speedCurve(SpeedCurve::CURVE_SINE),
    speedIntensity(75.0f),
    randomTurnbackEnabled(false),
    turnbackChance(30),
    endPauseEnabled(false),
    endPauseIsRandom(false),
    endPauseDurationSec(1.0f),
    endPauseMinSec(0.5f),
    endPauseMaxSec(2.0f) {}
};

// Runtime state for zone effects (separated from config for clean copy semantics)
// When SequenceExecutor copies zone config from a line, state is simply reset
struct ZoneEffectState {
  bool hasPendingTurnback;       // Turnback decision made for this pass
  bool hasRolledForTurnback;     // Already rolled dice for this zone entry
  float turnbackPointMM;         // Where to turn back (distance into zone)
  bool isPausing;                // Currently in end pause
  unsigned long pauseStartMs;    // When pause started
  unsigned long pauseDurationMs; // Current pause duration
  
  constexpr ZoneEffectState() :
    hasPendingTurnback(false),
    hasRolledForTurnback(false),
    turnbackPointMM(0.0f),
    isPausing(false),
    pauseStartMs(0),
    pauseDurationMs(0) {}
};

// ============================================================================
// PURSUIT MODE
// ============================================================================

struct PursuitState {
  long targetStep;
  long lastTargetStep;
  float maxSpeedLevel;
  float lastMaxSpeedLevel;
  unsigned long stepDelay;
  bool isMoving;
  bool direction;
  
  constexpr PursuitState() :
    targetStep(0),
    lastTargetStep(0),
    maxSpeedLevel(10.0f),
    lastMaxSpeedLevel(10.0f),
    stepDelay(1000),
    isMoving(false),
    direction(true) {}
};

// ============================================================================
// OSCILLATION MODE
// ============================================================================

enum class OscillationWaveform {
  OSC_SINE = 0,      // Smooth sinusoidal wave
  OSC_TRIANGLE = 1,  // Linear triangle wave
  OSC_SQUARE = 2     // Square wave (instant direction change)
};

enum class RampType {
  RAMP_LINEAR = 0
};

struct OscillationConfig {
  float centerPositionMM;       // Center position for oscillation
  float amplitudeMM;            // Amplitude (±amplitude from center)
  OscillationWaveform waveform; // Waveform type
  float frequencyHz;            // Oscillation frequency (Hz)
  
  // Ramp in/out for smooth transitions
  bool enableRampIn;
  float rampInDurationMs;
  RampType rampInType;
  
  bool enableRampOut;
  float rampOutDurationMs;
  RampType rampOutType;
  
  int cycleCount;              // Number of cycles (0 = infinite)
  bool returnToCenter;         // Return to center after completion
  
  CyclePauseConfig cyclePause; // Inter-cycle pause
  
  constexpr OscillationConfig() :
    centerPositionMM(0),
    amplitudeMM(20.0f),
    waveform(OscillationWaveform::OSC_SINE),
    frequencyHz(0.5f),
    enableRampIn(true),
    rampInDurationMs(2000.0f),
    rampInType(RampType::RAMP_LINEAR),
    enableRampOut(true),
    rampOutDurationMs(2000.0f),
    rampOutType(RampType::RAMP_LINEAR),
    cycleCount(0),
    returnToCenter(true) {}
};

struct OscillationState {
  unsigned long startTimeMs;    // Start time for phase tracking
  unsigned long rampStartMs;    // Ramp start time
  float currentAmplitude;       // Current amplitude (for ramping)
  int completedCycles;          // Completed full cycles
  bool isRampingIn;             // Currently ramping in
  bool isRampingOut;            // Currently ramping out
  bool isReturning;             // Returning to center
  bool isInitialPositioning;    // Moving to starting position (before oscillation starts)
  
  // Frequency transition (smooth frequency changes with phase continuity)
  bool isTransitioning;         // Currently transitioning between frequencies
  unsigned long transitionStartMs;  // Transition start time
  float oldFrequencyHz;         // Previous frequency
  float targetFrequencyHz;      // Target frequency
  float accumulatedPhase;       // Accumulated phase (0.0 to infinity) for smooth transitions
  unsigned long lastPhaseUpdateMs;  // Last time phase was updated
  float lastPhase;              // Last phase value (for cycle counting)
  
  // Center position transition (smooth center changes)
  bool isCenterTransitioning;   // Currently transitioning to new center position
  unsigned long centerTransitionStartMs;  // Center transition start time
  float oldCenterMM;            // Previous center position
  float targetCenterMM;         // Target center position
  
  // Amplitude transition (smooth amplitude changes)
  bool isAmplitudeTransitioning;  // Currently transitioning to new amplitude
  unsigned long amplitudeTransitionStartMs;  // Amplitude transition start time
  float oldAmplitudeMM;         // Previous amplitude
  float targetAmplitudeMM;      // Target amplitude
  
  constexpr OscillationState() :
    startTimeMs(0),
    rampStartMs(0),
    currentAmplitude(0),
    completedCycles(0),
    isRampingIn(false),
    isRampingOut(false),
    isReturning(false),
    isInitialPositioning(false),
    isTransitioning(false),
    transitionStartMs(0),
    oldFrequencyHz(0),
    targetFrequencyHz(0),
    accumulatedPhase(0.0f),
    lastPhaseUpdateMs(0),
    lastPhase(0.0f),
    isCenterTransitioning(false),
    centerTransitionStartMs(0),
    oldCenterMM(0),
    targetCenterMM(0),
    isAmplitudeTransitioning(false),
    amplitudeTransitionStartMs(0),
    oldAmplitudeMM(0),
    targetAmplitudeMM(0) {}
};

// ============================================================================
// CHAOS MODE
// ============================================================================

enum class ChaosPattern {
  CHAOS_ZIGZAG = 0,       // Rapid back-and-forth with random targets (12%)
  CHAOS_SWEEP = 1,        // Smooth sweeps across range (12%)
  CHAOS_PULSE = 2,        // Quick pulses from center (8%)
  CHAOS_DRIFT = 3,        // Slow wandering movements (8%)
  CHAOS_BURST = 4,        // High-speed random jumps (5%)
  CHAOS_WAVE = 5,         // Continuous wave-like motion (15%)
  CHAOS_PENDULUM = 6,     // Regular back-and-forth pendulum (12%)
  CHAOS_SPIRAL = 7,       // Progressive spiral in/out (8%)
  CHAOS_CALM = 8,         // Breathing/heartbeat rhythm (10%)
  CHAOS_BRUTE_FORCE = 9,  // Battering ram: fast in, slow out (10%)
  CHAOS_LIBERATOR = 10    // Extraction: slow in, fast out (10%)
};

struct ChaosRuntimeConfig {
  float centerPositionMM;      // Center position for chaos movements
  float amplitudeMM;           // Maximum deviation from center (±)
  float maxSpeedLevel;         // Maximum speed level (1-MAX_SPEED_LEVEL)
  unsigned long durationSeconds; // Total duration (0 = infinite)
  unsigned long seed;          // Random seed (0 = use micros())
  float crazinessPercent;      // Degree of madness 0-100% (affects speed, duration, jump size)
  bool patternsEnabled[CHAOS_PATTERN_COUNT];    // Enable/disable each pattern (ZIGZAG, SWEEP, PULSE, DRIFT, BURST, WAVE, PENDULUM, SPIRAL, CALM, BRUTE_FORCE, LIBERATOR)
  
  constexpr ChaosRuntimeConfig() : 
    centerPositionMM(110.0f), 
    amplitudeMM(50.0f), 
    maxSpeedLevel(5.0f),
    durationSeconds(0),
    seed(0),
    crazinessPercent(50.0f) {
      // Enable all patterns by default
      for (int i = 0; i < CHAOS_PATTERN_COUNT; i++) {
        patternsEnabled[i] = true;
      }
    }
};

struct ChaosExecutionState {
  bool isRunning;
  ChaosPattern currentPattern;
  unsigned long startTime;           // Chaos mode start time
  unsigned long nextPatternChangeTime; // When to generate next pattern
  float targetPositionMM;            // Current target position
  float currentSpeedLevel;           // Current speed being used
  float minReachedMM;                // Minimum position reached
  float maxReachedMM;                // Maximum position reached
  unsigned int patternsExecuted;     // Count of patterns executed
  
  // Continuous motion state (for WAVE, PENDULUM, SPIRAL)
  bool movingForward;                // Direction for continuous patterns
  float waveAmplitude;               // Current amplitude for wave
  float spiralRadius;                // Current radius for spiral
  unsigned long patternStartTime;    // When current pattern started
  
  // PULSE specific state (2-phase: OUT + RETURN)
  bool pulsePhase;                   // false = OUT phase, true = RETURN phase
  float pulseCenterMM;               // Center position to return to
  
  // WAVE specific state (sinusoidal continuous)
  float waveFrequency;               // Frequency in Hz for sinusoidal wave
  
  // CALM specific state (breathing/heartbeat)
  // NOTE: isInPatternPause is INTERNAL to chaos patterns, NOT user pause
  // User pause is controlled by config.currentState == STATE_PAUSED
  bool isInPatternPause;             // Currently in pattern's internal pause phase
  unsigned long pauseStartTime;      // When pattern pause started
  unsigned long pauseDuration;       // Duration of current pattern pause
  float lastCalmSineValue;           // Last sine value for CALM peak detection
  
  // BRUTE FORCE specific state (3-phase: fast in, slow out, pause)
  uint8_t brutePhase;                // 0=fast forward, 1=slow return, 2=pause
  
  // LIBERATOR specific state (3-phase: slow in, fast out, pause)
  uint8_t liberatorPhase;            // 0=slow forward, 1=fast return, 2=pause
  
  // Timing control for non-blocking stepping
  unsigned long stepDelay;           // Microseconds between steps
  unsigned long lastStepMicros;      // Last step timestamp
  
  constexpr ChaosExecutionState() : 
    isRunning(false),
    currentPattern(ChaosPattern::CHAOS_ZIGZAG),
    startTime(0),
    nextPatternChangeTime(0),
    targetPositionMM(0),
    currentSpeedLevel(0),
    minReachedMM(999999),
    maxReachedMM(0),
    patternsExecuted(0),
    movingForward(true),
    waveAmplitude(0),
    spiralRadius(0),
    patternStartTime(0),
    pulsePhase(false),
    pulseCenterMM(0),
    waveFrequency(0),
    isInPatternPause(false),
    pauseStartTime(0),
    pauseDuration(0),
    lastCalmSineValue(0.0f),
    brutePhase(0),
    liberatorPhase(0),
    stepDelay(1000),
    lastStepMicros(0) {}
};

// ============================================================================
// SEQUENCER
// ============================================================================

struct SequenceLine {
  bool enabled;
  MovementType movementType;  // Type of movement for this line
  
  // VA-ET-VIENT parameters
  float startPositionMM;
  float distanceMM;
  float speedForward;
  float speedBackward;
  
  // VA-ET-VIENT Zone Effects (DRY: uses same struct as standalone mode)
  // Runtime state is managed separately by zoneEffectState global
  ZoneEffectConfig vaetZoneEffect;
  
  // VA-ET-VIENT cycle pause (DRY: uses CyclePauseConfig struct)
  CyclePauseConfig vaetCyclePause;
  
  // OSCILLATION parameters
  float oscCenterPositionMM;
  float oscAmplitudeMM;
  OscillationWaveform oscWaveform;
  float oscFrequencyHz;
  bool oscEnableRampIn;
  bool oscEnableRampOut;
  float oscRampInDurationMs;
  float oscRampOutDurationMs;
  
  // OSCILLATION cycle pause (DRY: uses CyclePauseConfig struct)
  CyclePauseConfig oscCyclePause;
  
  // CHAOS parameters
  float chaosCenterPositionMM;
  float chaosAmplitudeMM;
  float chaosMaxSpeedLevel;
  float chaosCrazinessPercent;
  unsigned long chaosDurationSeconds;
  unsigned long chaosSeed;
  bool chaosPatternsEnabled[CHAOS_PATTERN_COUNT];  // ZIGZAG, SWEEP, PULSE, DRIFT, BURST, WAVE, PENDULUM, SPIRAL, CALM, BRUTE_FORCE, LIBERATOR
  
  // COMMON parameters
  int cycleCount;
  int pauseAfterMs;
  int lineId;
  
  constexpr SequenceLine() :
    enabled(true),
    movementType(MovementType::MOVEMENT_VAET),
    startPositionMM(0),
    distanceMM(100),
    speedForward(5.0f),
    speedBackward(5.0f),
    vaetZoneEffect(),  // Uses ZoneEffectConfig default constructor
    vaetCyclePause(),  // Uses CyclePauseConfig default constructor
    oscCenterPositionMM(100.0f),
    oscAmplitudeMM(50.0f),
    oscWaveform(OscillationWaveform::OSC_SINE),
    oscFrequencyHz(0.5f),
    oscEnableRampIn(false),
    oscEnableRampOut(false),
    oscRampInDurationMs(1000.0f),
    oscRampOutDurationMs(1000.0f),
    oscCyclePause(),  // Uses CyclePauseConfig default constructor
    chaosCenterPositionMM(110.0f),
    chaosAmplitudeMM(50.0f),
    chaosMaxSpeedLevel(10.0f),
    chaosCrazinessPercent(50.0f),
    chaosDurationSeconds(30),
    chaosSeed(0),
    cycleCount(1),
    pauseAfterMs(0),
    lineId(0) {
      // Initialize all chaos patterns to enabled
      for (int i = 0; i < CHAOS_PATTERN_COUNT; i++) {
        chaosPatternsEnabled[i] = true;
      }
    }
};

struct SequenceExecutionState {
  bool isRunning;
  bool isLoopMode;
  int currentLineIndex;
  int currentCycleInLine;
  bool isPaused;
  bool isWaitingPause;
  unsigned long pauseEndTime;
  int loopCount;
  unsigned long sequenceStartTime;
  unsigned long lineStartTime;
  
  constexpr SequenceExecutionState() :
    isRunning(false),
    isLoopMode(false),
    currentLineIndex(0),
    currentCycleInLine(0),
    isPaused(false),
    isWaitingPause(false),
    pauseEndTime(0),
    loopCount(0),
    sequenceStartTime(0),
    lineStartTime(0) {}
};

// ============================================================================
// PLAYLIST STRUCTURES
// ============================================================================

constexpr int MAX_PRESETS_PER_MODE = 20;
constexpr const char* PLAYLIST_FILE_PATH = "/playlists.json";

enum class PlaylistMode {
  PLAYLIST_SIMPLE = 0,
  PLAYLIST_OSCILLATION = 1,
  PLAYLIST_CHAOS = 2
};

struct PlaylistPreset {
  int id;
  String name;
  unsigned long timestamp;  // Creation time (epoch seconds)
  PlaylistMode mode;
  String configJson;  // JSON string of the config (flexible storage)
  
  PlaylistPreset() :
    id(0),
    name(""),
    timestamp(0),
    mode(PlaylistMode::PLAYLIST_SIMPLE),
    configJson("{}") {}
};

#endif // TYPES_H
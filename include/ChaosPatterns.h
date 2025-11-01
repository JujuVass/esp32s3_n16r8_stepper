// ============================================================================
// CHAOSPATTERNS.H - Chaos Mode Pattern Configurations
// ============================================================================
// All chaos pattern configurations centralized for easy tweaking
// Modify pattern configs to adjust behavior without touching main code
// ============================================================================

#ifndef CHAOS_PATTERNS_H
#define CHAOS_PATTERNS_H

// ============================================================================
// CHAOS PATTERN CONFIGURATION STRUCTURES
// ============================================================================

/**
 * Base configuration for all chaos patterns
 * Contains common parameters: speed, duration, amplitude/jump
 */
struct ChaosBaseConfig {
  float speedMin;                    // Minimum speed (0.0-1.0)
  float speedMax;                    // Maximum speed (0.0-1.0)
  float speedCrazinessBoost;         // Speed increase per craziness unit
  unsigned long durationMin;         // Minimum pattern duration (ms)
  unsigned long durationMax;         // Maximum pattern duration (ms)
  unsigned long durationCrazinessReduction; // Duration reduction per craziness unit
  float amplitudeJumpMin;            // Minimum amplitude/jump (0.0-1.0)
  float amplitudeJumpMax;            // Maximum amplitude/jump (0.0-1.0)
};

/**
 * Extension for sinusoidal patterns (WAVE, CALM)
 */
struct ChaosSinusoidalExt {
  float frequencyMin;                // Minimum frequency (Hz)
  float frequencyMax;                // Maximum frequency (Hz)
  int cyclesOverDuration;            // Fixed cycles over duration (0 = use frequencyMin/Max)
};

/**
 * Extension for multi-phase patterns (BRUTE_FORCE, LIBERATOR, PULSE)
 */
struct ChaosMultiPhaseExt {
  float phase2SpeedMin;              // Phase 2 minimum speed (0.0-1.0)
  float phase2SpeedMax;              // Phase 2 maximum speed (0.0-1.0)
  float phase2SpeedCrazinessBoost;   // Phase 2 speed boost
  unsigned long pauseMin;            // Minimum pause duration (ms)
  unsigned long pauseMax;            // Maximum pause duration (ms)
};

/**
 * Extension for patterns with random pauses (CALM)
 */
struct ChaosPauseExt {
  unsigned long pauseMin;            // Minimum pause duration (ms)
  unsigned long pauseMax;            // Maximum pause duration (ms)
  float pauseChancePercent;          // Pause probability (0-100)
  float pauseTriggerThreshold;       // Sine threshold for pause (0.0-1.0)
};

/**
 * Extension for directional patterns (BRUTE_FORCE, LIBERATOR)
 */
struct ChaosDirectionExt {
  int forwardChanceMin;              // Forward probability at 0% craziness (0-100)
  int forwardChanceMax;              // Forward probability at 100% craziness (0-100)
};

// ============================================================================
// PATTERN CONFIGURATION CONSTANTS
// ============================================================================

// ZIGZAG: Rapid random jumps
const ChaosBaseConfig ZIGZAG_CONFIG = {
  0.40, 0.70, 0.30,  // Speed: 40-70% + 30%
  2000, 4000, 600,   // Duration: 2000-4000ms - 600ms
  0.60, 1.00         // Jump: 60-100%
};

// SWEEP: Large edge-to-edge sweeps
const ChaosBaseConfig SWEEP_CONFIG = {
  0.30, 0.60, 0.40,  // Speed: 30-60% + 40%
  3000, 5000, 1400,  // Duration: 3000-5000ms - 1400ms
  0.75, 1.00         // Amplitude: 75-100%
};

// PULSE: Quick out-and-back pulses
const ChaosBaseConfig PULSE_CONFIG = {
  0.50, 0.80, 0.20,  // Speed: 50-80% + 20%
  800, 1500, 400,    // Duration: 800-1500ms - 400ms
  0.40, 1.00         // Jump: 40-100%
};

// DRIFT: Slow wandering
const ChaosBaseConfig DRIFT_CONFIG = {
  0.20, 0.40, 0.30,  // Speed: 20-40% + 30%
  4000, 9000, 1500,  // Duration: 4000-9000ms - 1500ms
  0.25, 0.75         // Drift: 25-75%
};

// BURST: Fast explosive jumps
const ChaosBaseConfig BURST_CONFIG = {
  0.60, 0.90, 0.10,  // Speed: 60-90% + 10%
  600, 1200, 300,    // Duration: 600-1200ms - 300ms
  0.70, 1.00         // Jump: 70-100%
};

// WAVE: Sinusoidal continuous motion
const ChaosBaseConfig WAVE_CONFIG = {
  0.25, 0.50, 0.25,  // Speed: 25-50% + 25%
  6000, 12000, 2000, // Duration: 6000-12000ms - 2000ms
  0.50, 1.00         // Amplitude: 50-100%
};
const ChaosSinusoidalExt WAVE_SIN = {
  0.0, 0.0,          // Frequency calculated from duration
  3                  // 3 cycles over pattern duration
};

// PENDULUM: Regular back-and-forth
const ChaosBaseConfig PENDULUM_CONFIG = {
  0.30, 0.60, 0.30,  // Speed: 30-60% + 30%
  5000, 8000, 1200,  // Duration: 5000-8000ms - 1200ms
  0.60, 1.00         // Amplitude: 60-100%
};

// SPIRAL: Progressive in/out
const ChaosBaseConfig SPIRAL_CONFIG = {
  0.20, 0.45, 0.30,  // Speed: 20-45% + 30%
  5000, 10000, 2500, // Duration: 5000-10000ms - 2500ms
  0.10, 1.00         // Radius: 10% start → 100% progressive
};

// CALM (BREATHING): Slow sinusoidal with pauses
const ChaosBaseConfig CALM_CONFIG = {
  0.05, 0.10, 0.10,  // Speed: 5-10% + 10%
  5000, 8000, 800,   // Duration: 5000-8000ms - 800ms
  0.10, 0.30         // Amplitude: 10-30%
};
const ChaosSinusoidalExt CALM_SIN = {
  0.2, 1.0,          // Frequency: 0.2-1.0 Hz
  0                  // Random frequency (not cycles-based)
};
const ChaosPauseExt CALM_PAUSE = {
  500, 2000,         // Pause: 500-2000ms
  20.0,               // 20% chance
  0.95               // At sine extremes (±0.95)
};

// BRUTE_FORCE: Fast in, slow out, pause
const ChaosBaseConfig BRUTE_FORCE_CONFIG = {
  0.70, 1.00, 0.30,  // Speed fast: 70-100% + 30%
  3000, 5000, 750,   // Duration: 3000-5000ms - 750ms
  0.60, 0.90         // Amplitude: 60-90%
};
const ChaosMultiPhaseExt BRUTE_FORCE_MULTI = {
  0.01, 0.10, 0.09,  // Speed slow: 1-10% + 9%
  500, 2000          // Pause: 500-2000ms
};
const ChaosDirectionExt BRUTE_FORCE_DIR = {
  90, 60             // Direction: 90% → 60% forward
};

// LIBERATOR: Slow in, fast out, pause
const ChaosBaseConfig LIBERATOR_CONFIG = {
  0.05, 0.15, 0.10,  // Speed slow: 5-15% + 10%
  3000, 5000, 750,   // Duration: 3000-5000ms - 750ms
  0.60, 0.90         // Amplitude: 60-90%
};
const ChaosMultiPhaseExt LIBERATOR_MULTI = {
  0.70, 1.00, 0.30,  // Speed fast: 70-100% + 30%
  500, 2000          // Pause: 500-2000ms
};
const ChaosDirectionExt LIBERATOR_DIR = {
  90, 60             // Direction: 90% → 60% forward
};

#endif // CHAOS_PATTERNS_H

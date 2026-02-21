// ============================================================================
// MOVEMENT MATH — Pure, testable math functions for movement controllers
// ============================================================================
// Extracted from BaseMovementController, ChaosController, PursuitController,
// and OscillationController so that unit tests exercise the REAL production
// formulas instead of local mirrors.
//
// All functions are free (namespace-scoped), pure, and depend only on
// Config.h + Types.h + <cmath>.  No hardware, no globals, no side effects.
//
// Architecture: .h declarations + .cpp definitions.
// Native test env compiles MovementMath.cpp via build_src_filter.
// ============================================================================

#pragma once

#include <cmath>
#include <numbers>
#include "Config.h"
#include "Types.h"
#include "movement/ChaosPatterns.h"

constexpr float PI_F = std::numbers::pi_v<float>;

namespace MovementMath {

// ============================================================================
// UNIT CONVERSIONS (inline — trivial one-liners)
// ============================================================================

/** Convert millimeters to steps */
constexpr long mmToSteps(float mm) { return (long)(mm * STEPS_PER_MM); }

/** Convert steps to millimeters */
constexpr float stepsToMM(long steps) { return static_cast<float>(steps) / STEPS_PER_MM; }

// ============================================================================
// SPEED / DELAY
// ============================================================================

/** Convert speed level (0–MAX_SPEED_LEVEL) to cycles per minute. */
float speedLevelToCPM(float speedLevel);

/** Step delay for va-et-vient mode (µs). Fallback 1000 on bad input. */
unsigned long vaetStepDelay(float speedLevel, float distanceMM);

/** Step delay for chaos mode (µs). Clamped to [20, CHAOS_MAX_STEP_DELAY_MICROS]. */
unsigned long chaosStepDelay(float speedLevel);

/** Step delay for pursuit mode (µs). Speed ramps by error distance. */
unsigned long pursuitStepDelay(float errorMM, float maxSpeedLevel);

// ============================================================================
// ZONE EFFECTS
// ============================================================================

/**
 * Zone speed-adjustment factor.
 * @return 1.0 = normal, >1 = slower for DECEL, <1 = faster for ACCEL.
 */
float zoneSpeedFactor(SpeedEffect effect, SpeedCurve curve,
                      float intensity, float zoneProgress);

// ============================================================================
// CHAOS
// ============================================================================

/** Safe [min, max) duration, preventing unsigned underflow. */
void safeDurationCalc(const ChaosBaseConfig& cfg, float craziness, float maxFactor,
                      unsigned long& outMin, unsigned long& outMax);

// ============================================================================
// OSCILLATION
// ============================================================================

/** Waveform value (−1 to +1). SINE uses −cos convention. */
float waveformValue(OscillationWaveform waveform, float phase);

/**
 * Effective frequency capped by hardware speed limit.
 * Pure math — no OscillationController dependency.
 */
float effectiveFrequency(float requestedHz, float amplitudeMM);

} // namespace MovementMath

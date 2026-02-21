// ============================================================================
// MOVEMENT MATH — Implementation
// ============================================================================
// Pure math, no hardware, no globals. See MovementMath.h for declarations.
// ============================================================================

#include <Arduino.h>
#include "core/MovementMath.h"

namespace MovementMath {

// ============================================================================
// SPEED / DELAY
// ============================================================================

float speedLevelToCPM(float speedLevel) {
    float cpm = speedLevel * 10.0f;
    if (cpm < 0) cpm = 0.0f;
    if (cpm > MAX_SPEED_LEVEL * 10.0f) cpm = MAX_SPEED_LEVEL * 10.0f;
    return cpm;
}

unsigned long vaetStepDelay(float speedLevel, float distanceMM) {
    if (distanceMM <= 0 || speedLevel <= 0) return 1000;

    float cpm = speedLevelToCPM(speedLevel);
    if (cpm <= 0.1f) cpm = 0.1f;

    long stepsPerDirection = mmToSteps(distanceMM);
    if (stepsPerDirection <= 0) return 1000;

    float halfCycleMs   = (60000.0f / cpm) / 2.0f;
    float rawDelay      = (halfCycleMs * 1000.0f) / (float)stepsPerDirection;
    float delay         = (rawDelay - static_cast<float>(STEP_EXECUTION_TIME_MICROS)) / SPEED_COMPENSATION_FACTOR;

    if (delay < 20) delay = 20.0f;
    return (unsigned long)delay;
}

unsigned long chaosStepDelay(float speedLevel) {
    float mmPerSecond   = speedLevel * 10.0f;
    float stepsPerSecond = mmPerSecond * STEPS_PER_MM;

    unsigned long delay;
    if (stepsPerSecond > 0) {
        delay = (unsigned long)((1000000.0f / stepsPerSecond) / SPEED_COMPENSATION_FACTOR);
    } else {
        delay = 10000;
    }
    if (delay < 20) delay = 20;
    if (delay > CHAOS_MAX_STEP_DELAY_MICROS) delay = CHAOS_MAX_STEP_DELAY_MICROS;
    return delay;
}

unsigned long pursuitStepDelay(float errorMM, float maxSpeedLevel) {
    float speedLevel;
    if (errorMM > 5.0f) {
        speedLevel = maxSpeedLevel;
    } else if (errorMM > 1.0f) {
        float ratio = (errorMM - 1.0f) / (5.0f - 1.0f);
        speedLevel  = maxSpeedLevel * (0.6f + ratio * 0.4f);
    } else {
        speedLevel  = maxSpeedLevel * 0.6f;
    }

    float mmPerSecond    = speedLevel * 10.0f;
    float stepsPerSecond = mmPerSecond * STEPS_PER_MM;
    if (stepsPerSecond < 30)   stepsPerSecond = 30.0f;
    if (stepsPerSecond > 6000) stepsPerSecond = 6000.0f;

    float delayMicros = ((1000000.0f / stepsPerSecond) - static_cast<float>(STEP_EXECUTION_TIME_MICROS)) / SPEED_COMPENSATION_FACTOR;
    if (delayMicros < 20) delayMicros = 20.0f;
    return (unsigned long)delayMicros;
}

// ============================================================================
// ZONE EFFECTS
// ============================================================================

float zoneSpeedFactor(SpeedEffect effect, SpeedCurve curve,
                      float intensity, float zoneProgress) {
    if (effect == SpeedEffect::SPEED_NONE) return 1.0f;

    float maxIntensity = 1.0f + (intensity / 100.0f) * 9.0f;
    float curveValue;

    using enum SpeedCurve;
    switch (curve) {
        case CURVE_LINEAR:
            curveValue = 1.0f - zoneProgress;
            break;
        case CURVE_SINE: {
            float sp   = (1.0f - cosf(zoneProgress * PI_F)) / 2.0f;
            curveValue = 1.0f - sp;
            break;
        }
        case CURVE_TRIANGLE_INV: {
            float inv  = 1.0f - zoneProgress;
            curveValue = inv * inv;
            break;
        }
        case CURVE_SINE_INV: {
            float inv  = 1.0f - zoneProgress;
            curveValue = sinf(inv * PI_F / 2.0f);
            break;
        }
        default:
            curveValue = 1.0f - zoneProgress;
            break;
    }

    if (effect == SpeedEffect::SPEED_DECEL) {
        return 1.0f + curveValue * (maxIntensity - 1.0f);
    } else {
        float accelCurve = 1.0f - curveValue;
        float minFactor  = 1.0f / maxIntensity;
        return 1.0f - accelCurve * (1.0f - minFactor);
    }
}

// ============================================================================
// CHAOS
// ============================================================================

void safeDurationCalc(const ChaosBaseConfig& cfg, float craziness, float maxFactor,
                      unsigned long& outMin, unsigned long& outMax) {
    long minVal = (long)cfg.durationMin - (long)(static_cast<float>(cfg.durationCrazinessReduction) * craziness);
    long maxVal = (long)cfg.durationMax - (long)(static_cast<float>(cfg.durationMax - cfg.durationMin) * craziness * maxFactor);

    if (minVal < 100) minVal = 100;
    if (maxVal < 100) maxVal = 100;
    if (minVal >= maxVal) maxVal = minVal + 100;

    outMin = (unsigned long)minVal;
    outMax = (unsigned long)maxVal;
}

// ============================================================================
// OSCILLATION
// ============================================================================

float waveformValue(OscillationWaveform waveform, float phase) {
    using enum OscillationWaveform;
    switch (waveform) {
        case OSC_SINE:
            return -cosf(phase * 2.0f * PI_F);

        case OSC_TRIANGLE:
            if (phase < 0.5f) {
                return 1.0f - (phase * 4.0f);   // +1 → −1
            } else {
                return -3.0f + (phase * 4.0f);   // −1 → +1
            }

        case OSC_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;

        default:
            return 0.0f;
    }
}

float effectiveFrequency(float requestedHz, float amplitudeMM) {
    if (amplitudeMM > 0.0f) {
        float maxAllowedFreq = OSC_MAX_SPEED_MM_S / (2.0f * PI_F * amplitudeMM);
        if (requestedHz > maxAllowedFreq) {
            return maxAllowedFreq;
        }
    }
    return requestedHz;
}

} // namespace MovementMath

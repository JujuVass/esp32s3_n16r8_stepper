/**
 * sequencer.js - Sequencer Module for ESP32 Stepper Controller
 * 
 * Contains pure functions for sequence validation and management.
 * These functions have NO side effects (no DOM, no global state mutation).
 * 
 * Pattern: Pure functions are suffixed with "Pure" and can be unit tested.
 */

// ============================================================================
// CONSTANTS
// ============================================================================
const SEQUENCER_LIMITS = {
  // VA-ET-VIENT
  SPEED_MIN: 0,
  SPEED_MAX: 20,
  DECEL_ZONE_MIN: 10,
  DECEL_ZONE_MAX: 200,
  
  // OSCILLATION
  FREQUENCY_MIN: 0.01,
  FREQUENCY_MAX: 10,
  RAMP_DURATION_MIN: 100,
  RAMP_DURATION_MAX: 10000,
  
  // CHAOS
  CHAOS_SPEED_MIN: 1,
  CHAOS_SPEED_MAX: 20,
  CHAOS_CRAZINESS_MIN: 0,
  CHAOS_CRAZINESS_MAX: 100,
  CHAOS_DURATION_MIN: 5,
  CHAOS_DURATION_MAX: 600,
  CHAOS_SEED_MIN: 0,
  CHAOS_SEED_MAX: 9999999,
  
  // COMMON
  CYCLE_COUNT_MIN: 1,
  CYCLE_COUNT_MAX: 1000,
  PAUSE_MIN: 0,
  PAUSE_MAX: 60000
};

// Movement type constants
const MOVEMENT_TYPE = {
  VAET: 0,
  OSCILLATION: 1,
  CHAOS: 2,
  CALIBRATION: 4
};

// ============================================================================
// PURE VALIDATION FUNCTIONS
// ============================================================================

/**
 * Validate a sequencer line configuration (PURE FUNCTION)
 * @param {Object} line - The line configuration object
 * @param {number} movementType - 0=VAET, 1=OSC, 2=CHAOS, 4=CALIBRATION
 * @param {number} effectiveMax - Maximum travel distance in mm
 * @returns {string[]} Array of error messages (empty if valid)
 */
function validateSequencerLinePure(line, movementType, effectiveMax) {
  const errors = [];
  
  if (movementType === MOVEMENT_TYPE.VAET) {
    // VA-ET-VIENT validation
    const endPosition = line.startPositionMM + line.distanceMM;
    
    // Position départ
    if (line.startPositionMM < 0) {
      errors.push('⚠️ Position de départ ne peut pas être négative');
    }
    if (line.startPositionMM > effectiveMax) {
      errors.push(`⚠️ Position de départ (${line.startPositionMM.toFixed(1)}mm) dépasse la course disponible (${effectiveMax.toFixed(1)}mm)`);
    }
    
    // Distance
    if (line.distanceMM <= 0) {
      errors.push('⚠️ Distance doit être positive');
    }
    if (endPosition > effectiveMax) {
      errors.push(`⚠️ Position finale (${endPosition.toFixed(1)}mm) dépasse la course disponible (${effectiveMax.toFixed(1)}mm)`);
    }
    
    // Vitesses
    if (line.speedForward < SEQUENCER_LIMITS.SPEED_MIN || line.speedForward > SEQUENCER_LIMITS.SPEED_MAX) {
      errors.push(`⚠️ Vitesse aller doit être entre ${SEQUENCER_LIMITS.SPEED_MIN} et ${SEQUENCER_LIMITS.SPEED_MAX}`);
    }
    if (line.speedBackward < SEQUENCER_LIMITS.SPEED_MIN || line.speedBackward > SEQUENCER_LIMITS.SPEED_MAX) {
      errors.push(`⚠️ Vitesse retour doit être entre ${SEQUENCER_LIMITS.SPEED_MIN} et ${SEQUENCER_LIMITS.SPEED_MAX}`);
    }
    
    // Zone décélération
    if (line.decelZoneMM < SEQUENCER_LIMITS.DECEL_ZONE_MIN || line.decelZoneMM > SEQUENCER_LIMITS.DECEL_ZONE_MAX) {
      errors.push(`⚠️ Zone de décélération doit être entre ${SEQUENCER_LIMITS.DECEL_ZONE_MIN} et ${SEQUENCER_LIMITS.DECEL_ZONE_MAX} mm`);
    }
    
  } else if (movementType === MOVEMENT_TYPE.OSCILLATION) {
    // OSCILLATION validation
    const minPos = line.oscCenterPositionMM - line.oscAmplitudeMM;
    const maxPos = line.oscCenterPositionMM + line.oscAmplitudeMM;
    
    // Centre & Amplitude
    if (minPos < 0) {
      errors.push(`⚠️ Position min (${minPos.toFixed(1)}mm) est négative`);
    }
    if (maxPos > effectiveMax) {
      errors.push(`⚠️ Position max (${maxPos.toFixed(1)}mm) dépasse la course disponible (${effectiveMax.toFixed(1)}mm)`);
    }
    if (line.oscAmplitudeMM <= 0) {
      errors.push('⚠️ Amplitude doit être positive');
    }
    
    // Fréquence
    if (line.oscFrequencyHz < SEQUENCER_LIMITS.FREQUENCY_MIN || line.oscFrequencyHz > SEQUENCER_LIMITS.FREQUENCY_MAX) {
      errors.push(`⚠️ Fréquence doit être entre ${SEQUENCER_LIMITS.FREQUENCY_MIN} et ${SEQUENCER_LIMITS.FREQUENCY_MAX} Hz`);
    }
    
    // Durée rampes
    if (line.oscRampInDurationMs < SEQUENCER_LIMITS.RAMP_DURATION_MIN || line.oscRampInDurationMs > SEQUENCER_LIMITS.RAMP_DURATION_MAX) {
      errors.push(`⚠️ Durée rampe IN doit être entre ${SEQUENCER_LIMITS.RAMP_DURATION_MIN} et ${SEQUENCER_LIMITS.RAMP_DURATION_MAX} ms`);
    }
    if (line.oscRampOutDurationMs < SEQUENCER_LIMITS.RAMP_DURATION_MIN || line.oscRampOutDurationMs > SEQUENCER_LIMITS.RAMP_DURATION_MAX) {
      errors.push(`⚠️ Durée rampe OUT doit être entre ${SEQUENCER_LIMITS.RAMP_DURATION_MIN} et ${SEQUENCER_LIMITS.RAMP_DURATION_MAX} ms`);
    }
    
  } else if (movementType === MOVEMENT_TYPE.CHAOS) {
    // CHAOS validation
    const minPos = line.chaosCenterPositionMM - line.chaosAmplitudeMM;
    const maxPos = line.chaosCenterPositionMM + line.chaosAmplitudeMM;
    
    // Centre & Amplitude
    if (minPos < 0) {
      errors.push(`⚠️ Position min (${minPos.toFixed(1)}mm) est négative`);
    }
    if (maxPos > effectiveMax) {
      errors.push(`⚠️ Position max (${maxPos.toFixed(1)}mm) dépasse la course disponible (${effectiveMax.toFixed(1)}mm)`);
    }
    if (line.chaosAmplitudeMM <= 0) {
      errors.push('⚠️ Amplitude doit être positive');
    }
    
    // Vitesse max
    if (line.chaosMaxSpeedLevel < SEQUENCER_LIMITS.CHAOS_SPEED_MIN || line.chaosMaxSpeedLevel > SEQUENCER_LIMITS.CHAOS_SPEED_MAX) {
      errors.push(`⚠️ Vitesse max doit être entre ${SEQUENCER_LIMITS.CHAOS_SPEED_MIN} et ${SEQUENCER_LIMITS.CHAOS_SPEED_MAX}`);
    }
    
    // Degré de folie
    if (line.chaosCrazinessPercent < SEQUENCER_LIMITS.CHAOS_CRAZINESS_MIN || line.chaosCrazinessPercent > SEQUENCER_LIMITS.CHAOS_CRAZINESS_MAX) {
      errors.push(`⚠️ Degré de folie doit être entre ${SEQUENCER_LIMITS.CHAOS_CRAZINESS_MIN} et ${SEQUENCER_LIMITS.CHAOS_CRAZINESS_MAX}%`);
    }
    
    // Durée
    if (line.chaosDurationSeconds < SEQUENCER_LIMITS.CHAOS_DURATION_MIN || line.chaosDurationSeconds > SEQUENCER_LIMITS.CHAOS_DURATION_MAX) {
      errors.push(`⚠️ Durée doit être entre ${SEQUENCER_LIMITS.CHAOS_DURATION_MIN} et ${SEQUENCER_LIMITS.CHAOS_DURATION_MAX} secondes`);
    }
    
    // Seed
    if (line.chaosSeed < SEQUENCER_LIMITS.CHAOS_SEED_MIN || line.chaosSeed > SEQUENCER_LIMITS.CHAOS_SEED_MAX) {
      errors.push(`⚠️ Seed doit être entre ${SEQUENCER_LIMITS.CHAOS_SEED_MIN} et ${SEQUENCER_LIMITS.CHAOS_SEED_MAX}`);
    }
    
  } else if (movementType === MOVEMENT_TYPE.CALIBRATION) {
    // CALIBRATION - no validation needed (no parameters)
  }
  
  // Validation commune (Cycles & Pause)
  if (movementType !== MOVEMENT_TYPE.CHAOS && movementType !== MOVEMENT_TYPE.CALIBRATION) {
    // CHAOS uses duration, CALIBRATION forces 1 cycle
    if (line.cycleCount < SEQUENCER_LIMITS.CYCLE_COUNT_MIN || line.cycleCount > SEQUENCER_LIMITS.CYCLE_COUNT_MAX) {
      errors.push(`⚠️ Nombre de cycles doit être entre ${SEQUENCER_LIMITS.CYCLE_COUNT_MIN} et ${SEQUENCER_LIMITS.CYCLE_COUNT_MAX}`);
    }
  }
  
  if (line.pauseAfterMs < SEQUENCER_LIMITS.PAUSE_MIN || line.pauseAfterMs > SEQUENCER_LIMITS.PAUSE_MAX) {
    errors.push(`⚠️ Pause doit être entre ${SEQUENCER_LIMITS.PAUSE_MIN / 1000} et ${SEQUENCER_LIMITS.PAUSE_MAX / 1000} secondes`);
  }
  
  return errors;
}

/**
 * Build default values for a new sequence line (PURE FUNCTION)
 * @param {number} effectiveMax - Maximum travel distance in mm
 * @returns {Object} Default line configuration
 */
function buildSequenceLineDefaultsPure(effectiveMax) {
  const center = effectiveMax / 2;
  
  return {
    enabled: true,
    movementType: MOVEMENT_TYPE.VAET,
    
    // VA-ET-VIENT fields
    startPositionMM: 0,
    distanceMM: Math.min(100, effectiveMax),
    speedForward: 5.0,
    speedBackward: 5.0,
    decelStartEnabled: false,
    decelEndEnabled: true,
    decelZoneMM: 20,
    decelEffectPercent: 50,
    decelMode: 1,
    
    // VA-ET-VIENT cycle pause
    vaetCyclePauseEnabled: false,
    vaetCyclePauseIsRandom: false,
    vaetCyclePauseDurationSec: 0.0,
    vaetCyclePauseMinSec: 0.5,
    vaetCyclePauseMaxSec: 3.0,
    
    // OSCILLATION fields
    oscCenterPositionMM: center,
    oscAmplitudeMM: Math.min(50.0, center),
    oscWaveform: 0,  // SINE
    oscFrequencyHz: 0.5,
    oscEnableRampIn: false,
    oscEnableRampOut: false,
    oscRampInDurationMs: 1000.0,
    oscRampOutDurationMs: 1000.0,
    
    // OSCILLATION cycle pause
    oscCyclePauseEnabled: false,
    oscCyclePauseIsRandom: false,
    oscCyclePauseDurationSec: 0.0,
    oscCyclePauseMinSec: 0.5,
    oscCyclePauseMaxSec: 3.0,
    
    // CHAOS fields
    chaosCenterPositionMM: center,
    chaosAmplitudeMM: Math.min(50.0, center),
    chaosMaxSpeedLevel: 10.0,
    chaosCrazinessPercent: 50.0,
    chaosDurationSeconds: 30,
    chaosSeed: 0,
    chaosPatternsEnabled: [true, true, true, true, true, true, true, true, true, true, true],
    
    // COMMON fields
    cycleCount: 1,
    pauseAfterMs: 0
  };
}

// ============================================================================
// VA-ET-VIENT TOOLTIP HELPER (PURE FUNCTION)
// ============================================================================

const DECEL_MODES = ['Linéaire', 'Sinusoïdal', 'Triangle⁻¹', 'Sine⁻¹'];

/**
 * Generate VA-ET-VIENT tooltip content (PURE FUNCTION)
 * @param {Object} line - Sequence line configuration
 * @returns {string} Formatted tooltip string
 */
function generateVaetTooltipPure(line) {
  const decelInfo = [];
  if (line.decelStartEnabled) decelInfo.push('Départ');
  if (line.decelEndEnabled) decelInfo.push('Fin');
  
  // Cycle pause info
  let cyclePauseInfo = 'Aucune';
  if (line.vaetCyclePauseEnabled) {
    if (line.vaetCyclePauseIsRandom) {
      cyclePauseInfo = `${line.vaetCyclePauseMinSec}s-${line.vaetCyclePauseMaxSec}s (aléatoire)`;
    } else {
      cyclePauseInfo = `${line.vaetCyclePauseDurationSec}s (fixe)`;
    }
  }
  
  return `🔄 VA-ET-VIENT
━━━━━━━━━━━━━━━━━━━━━━
📍 Départ: ${line.startPositionMM}mm
📏 Distance: ${line.distanceMM}mm
➡️ Vitesse aller: ${line.speedForward.toFixed(1)}/20
⬅️ Vitesse retour: ${line.speedBackward.toFixed(1)}/20
${decelInfo.length > 0 ? '🛑 Décel: ' + decelInfo.join(' + ') + ' (' + line.decelZoneMM + 'mm, ' + line.decelEffectPercent + '%, ' + DECEL_MODES[line.decelMode] + ')' : '🛑 Décel: Aucune'}
🔄 Cycles: ${line.cycleCount}
⏸️ Pause/cycle: ${cyclePauseInfo}
⏱️ Pause après: ${line.pauseAfterMs > 0 ? (line.pauseAfterMs / 1000).toFixed(1) + 's' : 'Aucune'}`;
}

/**
 * Generate CALIBRATION tooltip content (PURE FUNCTION)
 * @returns {string} Formatted tooltip string
 */
function generateCalibrationTooltipPure() {
  return `📏 CALIBRATION
━━━━━━━━━━━━━━━━━━━━━━
Recalibration complète du système
Réinitialise la position à 0mm
Détecte la limite physique`;
}

/**
 * Generate tooltip content for any sequence line (PURE FUNCTION)
 * Delegates to mode-specific tooltip generators
 * @param {Object} line - Sequence line configuration
 * @returns {string} Formatted tooltip string
 */
function generateSequenceLineTooltipPure(line) {
  const movementType = line.movementType !== undefined ? line.movementType : MOVEMENT_TYPE.VAET;
  
  if (movementType === MOVEMENT_TYPE.VAET) {
    return generateVaetTooltipPure(line);
  } else if (movementType === MOVEMENT_TYPE.OSCILLATION) {
    // Delegate to oscillation.js if available
    if (typeof generateOscillationTooltipPure === 'function') {
      return generateOscillationTooltipPure(line);
    }
    return '〰️ OSCILLATION (module not loaded)';
  } else if (movementType === MOVEMENT_TYPE.CHAOS) {
    // Delegate to chaos.js if available
    if (typeof generateChaosTooltipPure === 'function') {
      return generateChaosTooltipPure(line);
    }
    return '🌀 CHAOS (module not loaded)';
  } else if (movementType === MOVEMENT_TYPE.CALIBRATION) {
    return generateCalibrationTooltipPure();
  }
  
  return 'Ligne de séquence';
}

// ============================================================================
// EXPORTS (Browser globals)
// ============================================================================
window.SEQUENCER_LIMITS = SEQUENCER_LIMITS;
window.MOVEMENT_TYPE = MOVEMENT_TYPE;
window.DECEL_MODES = DECEL_MODES;
window.validateSequencerLinePure = validateSequencerLinePure;
window.buildSequenceLineDefaultsPure = buildSequenceLineDefaultsPure;
window.generateVaetTooltipPure = generateVaetTooltipPure;
window.generateCalibrationTooltipPure = generateCalibrationTooltipPure;
window.generateSequenceLineTooltipPure = generateSequenceLineTooltipPure;

console.log('✅ sequencer.js loaded');

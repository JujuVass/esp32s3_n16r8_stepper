/**
 * context.js - Dependency Injection Container for ESP32 Stepper Controller
 * 
 * This module provides:
 * 1. A Context object that holds all dependencies (DOM refs, state, services)
 * 2. Generic utility functions (unit conversions)
 * 3. Context-aware wrapper functions for accessing state
 * 
 * NOTE: Mode-specific pure functions have been moved to their dedicated modules:
 * - chaos.js: validateChaosLimitsPure, buildChaosConfigPure, etc.
 * - oscillation.js: validateOscillationLimitsPure, buildOscillationConfigPure, etc.
 * - sequencer.js: validateSequencerLinePure, generateSequenceLineTooltipPure, etc.
 */

// ============================================================================
// DEPENDENCY INJECTION CONTAINER
// ============================================================================
const Context = {
  dom: {
    // Chaos mode elements
    chaosCenterPos: null,
    chaosAmplitude: null,
    chaosMaxSpeed: null,
    // Oscillation mode elements
    oscCenterPos: null,
    oscAmplitude: null,
    oscFrequency: null
  },
  state: null,
  playlist: null,
  ws: null,
  ui: {
    showNotification: null,
    updateStatusDot: null,
    log: null
  },
  SystemState: null,
  milestones: null,
  initialized: false
};

/**
 * Initialize the Context with DOM references and global state
 */
function initContext() {
  console.log("🔧 Initializing Context...");
  
  // Cache DOM elements
  Context.dom.chaosCenterPos = document.getElementById("chaosCenterPos");
  Context.dom.chaosAmplitude = document.getElementById("chaosAmplitude");
  Context.dom.chaosMaxSpeed = document.getElementById("chaosMaxSpeed");
  Context.dom.oscCenterPos = document.getElementById("oscCenter");
  Context.dom.oscAmplitude = document.getElementById("oscAmplitude");
  Context.dom.oscFrequency = document.getElementById("oscFrequency");
  
  // Link to global state objects
  if (typeof AppState !== "undefined") { Context.state = AppState; }
  if (typeof PlaylistState !== "undefined") { Context.playlist = PlaylistState; }
  if (typeof ws !== "undefined") { Context.ws = ws; }
  if (typeof SystemState !== "undefined") { Context.SystemState = SystemState; }
  if (typeof MILESTONES !== "undefined") { Context.milestones = MILESTONES; }
  
  // Link to UI functions
  if (typeof showNotification === "function") { Context.ui.showNotification = showNotification; }
  if (typeof updateStatusDot === "function") { Context.ui.updateStatusDot = updateStatusDot; }
  if (typeof log === "function") { Context.ui.log = log; }
  
  Context.initialized = true;
  console.log("✅ Context initialized");
  return Context;
}

// ============================================================================
// GENERIC UTILITY FUNCTIONS (PURE)
// ============================================================================

/**
 * Convert millimeters to steps (PURE FUNCTION)
 * @param {number} mm - Distance in millimeters
 * @param {number} stepsPerMM - Steps per millimeter (default: 80)
 * @returns {number} Distance in steps
 */
function mmToSteps(mm, stepsPerMM) {
  stepsPerMM = stepsPerMM || 80;
  return Math.round(mm * stepsPerMM);
}

/**
 * Convert steps to millimeters (PURE FUNCTION)
 * @param {number} steps - Distance in steps
 * @param {number} stepsPerMM - Steps per millimeter (default: 80)
 * @returns {number} Distance in millimeters
 */
function stepsToMm(steps, stepsPerMM) {
  stepsPerMM = stepsPerMM || 80;
  return steps / stepsPerMM;
}

/**
 * Get effective max distance from context (CONTEXT-AWARE)
 * @param {Object} ctx - Context object (optional, defaults to global Context)
 * @returns {number} Effective max distance in mm
 */
function getEffectiveMaxDistMM(ctx) {
  ctx = ctx || Context;
  if (ctx.state && ctx.state.pursuit) {
    return ctx.state.pursuit.effectiveMaxDistMM || ctx.state.pursuit.totalDistanceMM || 0;
  }
  return 0;
}

/**
 * Get total distance from context (CONTEXT-AWARE)
 * @param {Object} ctx - Context object (optional, defaults to global Context)
 * @returns {number} Total distance in mm
 */
function getTotalDistMM(ctx) {
  ctx = ctx || Context;
  if (ctx.state && ctx.state.pursuit) {
    return ctx.state.pursuit.totalDistanceMM || 0;
  }
  return 0;
}

// ============================================================================
// CONTEXT-AWARE WRAPPERS (delegate to pure functions in modules)
// ============================================================================

/**
 * Validate chaos limits using Context (CONTEXT-AWARE)
 * Delegates to validateChaosLimitsPure from chaos.js
 */
function validateChaosLimitsCtx(ctx) {
  ctx = ctx || Context;
  const centerPos = parseFloat((ctx.dom.chaosCenterPos || {}).value || 0);
  const amplitude = parseFloat((ctx.dom.chaosAmplitude || {}).value || 0);
  const totalDistMM = getTotalDistMM(ctx);
  
  if (typeof validateChaosLimitsPure === 'function') {
    return validateChaosLimitsPure(centerPos, amplitude, totalDistMM);
  }
  console.warn('validateChaosLimitsPure not available (chaos.js not loaded)');
  return { valid: true, min: 0, max: totalDistMM };
}

/**
 * Validate oscillation limits using Context (CONTEXT-AWARE)
 * Delegates to validateOscillationLimitsPure from oscillation.js
 */
function validateOscillationLimitsCtx(ctx) {
  ctx = ctx || Context;
  const centerPos = parseFloat((ctx.dom.oscCenterPos || {}).value || 0);
  const amplitude = parseFloat((ctx.dom.oscAmplitude || {}).value || 0);
  const totalDistMM = getTotalDistMM(ctx);
  
  if (typeof validateOscillationLimitsPure === 'function') {
    return validateOscillationLimitsPure(centerPos, amplitude, totalDistMM);
  }
  console.warn('validateOscillationLimitsPure not available (oscillation.js not loaded)');
  return { valid: true, min: 0, max: totalDistMM };
}

// ============================================================================
// EXPORTS (Browser globals)
// ============================================================================
window.Context = Context;
window.initContext = initContext;
window.mmToSteps = mmToSteps;
window.stepsToMm = stepsToMm;
window.getEffectiveMaxDistMM = getEffectiveMaxDistMM;
window.getTotalDistMM = getTotalDistMM;
window.validateChaosLimitsCtx = validateChaosLimitsCtx;
window.validateOscillationLimitsCtx = validateOscillationLimitsCtx;

console.log("✅ context.js loaded - DI Container ready");

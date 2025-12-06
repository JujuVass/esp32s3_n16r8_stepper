/**
 * simple-mode.js - Simple Mode Event Listeners & Config
 * 
 * Contains event listeners and configuration functions for Simple Mode.
 * Extracted from main.js v2.10
 * 
 * Dependencies:
 * - sendCommand() from websocket.js
 * - showNotification() from main.js
 * - WS_CMD from websocket.js
 */

// ============================================================================
// CYCLE PAUSE CONFIG (Simple Mode)
// ============================================================================

/**
 * Send cycle pause configuration for Simple mode
 */
function sendSimpleCyclePauseConfig() {
  const enabled = document.getElementById('cyclePauseEnabled')?.checked || false;
  const isRandom = document.getElementById('cyclePauseRandom')?.checked || false;
  const durationSec = parseFloat(document.getElementById('cyclePauseDuration')?.value || 0);
  let minSec = parseFloat(document.getElementById('cyclePauseMin')?.value || 0.5);
  let maxSec = parseFloat(document.getElementById('cyclePauseMax')?.value || 3.0);
  
  // Validation: Min must be ≤ Max (only if random enabled)
  if (isRandom && minSec > maxSec) {
    showNotification('⚠️ Pause Min (' + minSec.toFixed(1) + 's) doit être ≤ Max (' + maxSec.toFixed(1) + 's)', 'warning');
    // Auto-correction: adjust Max = Min
    maxSec = minSec;
    document.getElementById('cyclePauseMax').value = maxSec;
  }
  
  sendCommand(WS_CMD.SET_CYCLE_PAUSE, {
    mode: 'simple',
    enabled: enabled,
    isRandom: isRandom,
    durationSec: durationSec,
    minSec: minSec,
    maxSec: maxSec
  });
}

// ============================================================================
// INITIALIZATION - Setup Event Listeners
// ============================================================================

/**
 * Initialize all Simple Mode event listeners
 * Call this after DOM is ready
 */
function initSimpleModeListeners() {
  // --- Cycle Pause Listeners ---
  if (document.getElementById('cyclePauseEnabled')) {
    document.getElementById('cyclePauseEnabled').addEventListener('change', sendSimpleCyclePauseConfig);
  }
  if (document.getElementById('cyclePauseRandom')) {
    document.getElementById('cyclePauseRandom').addEventListener('change', sendSimpleCyclePauseConfig);
  }
  if (document.getElementById('cyclePauseDuration')) {
    document.getElementById('cyclePauseDuration').addEventListener('change', sendSimpleCyclePauseConfig);
  }
  if (document.getElementById('cyclePauseMin')) {
    document.getElementById('cyclePauseMin').addEventListener('change', sendSimpleCyclePauseConfig);
  }
  if (document.getElementById('cyclePauseMax')) {
    document.getElementById('cyclePauseMax').addEventListener('change', sendSimpleCyclePauseConfig);
  }
  
  // --- Start Position Presets ---
  document.querySelectorAll('[data-start]').forEach(btn => {
    btn.addEventListener('click', function() {
      const startPos = parseFloat(this.getAttribute('data-start'));
      const maxStart = parseFloat(document.getElementById('startPosition').max);
      
      if (startPos <= maxStart) {
        document.getElementById('startPosition').value = startPos;
        sendCommand(WS_CMD.SET_START_POSITION, {startPosition: startPos});
        
        document.querySelectorAll('[data-start]').forEach(b => b.classList.remove('active'));
        this.classList.add('active');
      }
    });
  });
  
  // --- Distance Presets ---
  document.querySelectorAll('[data-distance]').forEach(btn => {
    btn.addEventListener('click', function() {
      const distance = parseFloat(this.getAttribute('data-distance'));
      const maxDist = parseFloat(document.getElementById('distance').max);
      
      if (distance <= maxDist) {
        document.getElementById('distance').value = distance;
        sendCommand(WS_CMD.SET_DISTANCE, {distance: distance});
        
        document.querySelectorAll('[data-distance]').forEach(b => b.classList.remove('active'));
        this.classList.add('active');
      }
    });
  });
  
  // --- Unified Speed Presets ---
  document.querySelectorAll('[data-speed-unified]').forEach(btn => {
    btn.addEventListener('click', function() {
      const speed = parseFloat(this.getAttribute('data-speed-unified'));
      
      // Update visible unified field
      document.getElementById('speedUnified').value = speed;
      
      // Also update hidden separate fields for consistency when switching modes
      document.getElementById('speedForward').value = speed;
      document.getElementById('speedBackward').value = speed;
      
      // Send both commands to ESP32
      sendCommand(WS_CMD.SET_SPEED_FORWARD, {speed: speed});
      sendCommand(WS_CMD.SET_SPEED_BACKWARD, {speed: speed});
      
      // Update preset button highlighting
      document.querySelectorAll('[data-speed-unified]').forEach(b => b.classList.remove('active'));
      this.classList.add('active');
    });
  });
  
  // --- Separate Speed Presets (Forward) ---
  document.querySelectorAll('[data-speed-forward]').forEach(btn => {
    btn.addEventListener('click', function() {
      const speed = parseFloat(this.getAttribute('data-speed-forward'));
      document.getElementById('speedForward').value = speed;
      sendCommand(WS_CMD.SET_SPEED_FORWARD, {speed: speed});
      
      document.querySelectorAll('[data-speed-forward]').forEach(b => b.classList.remove('active'));
      this.classList.add('active');
    });
  });
  
  // --- Separate Speed Presets (Backward) ---
  document.querySelectorAll('[data-speed-backward]').forEach(btn => {
    btn.addEventListener('click', function() {
      const speed = parseFloat(this.getAttribute('data-speed-backward'));
      document.getElementById('speedBackward').value = speed;
      sendCommand(WS_CMD.SET_SPEED_BACKWARD, {speed: speed});
      
      document.querySelectorAll('[data-speed-backward]').forEach(b => b.classList.remove('active'));
      this.classList.add('active');
    });
  });
  
  // --- Speed Mode Toggle (Unified/Separate) ---
  document.querySelectorAll('input[name="speedMode"]').forEach(radio => {
    radio.addEventListener('change', function() {
      const isSeparate = document.getElementById('speedModeSeparate').checked;
      const unifiedGroup = document.getElementById('speedUnifiedGroup');
      const separateGroup = document.getElementById('speedSeparateGroup');
      
      if (isSeparate) {
        // UNIFIED → SEPARATE: Copy unified value to BOTH forward AND backward
        unifiedGroup.style.display = 'none';
        separateGroup.style.display = 'block';
        
        const unifiedSpeed = parseFloat(document.getElementById('speedUnified').value);
        document.getElementById('speedForward').value = unifiedSpeed;
        document.getElementById('speedBackward').value = unifiedSpeed;
        
        // Send both commands to ESP32 to ensure sync
        sendCommand(WS_CMD.SET_SPEED_FORWARD, {speed: unifiedSpeed});
        sendCommand(WS_CMD.SET_SPEED_BACKWARD, {speed: unifiedSpeed});
        
        // Update preset button highlighting
        document.querySelectorAll('[data-speed-forward]').forEach(btn => {
          if (parseFloat(btn.getAttribute('data-speed-forward')) === unifiedSpeed) {
            btn.classList.add('active');
          } else {
            btn.classList.remove('active');
          }
        });
        document.querySelectorAll('[data-speed-backward]').forEach(btn => {
          if (parseFloat(btn.getAttribute('data-speed-backward')) === unifiedSpeed) {
            btn.classList.add('active');
          } else {
            btn.classList.remove('active');
          }
        });
        
        console.log('Switched to SEPARATE mode: both speeds set to ' + unifiedSpeed);
        
      } else {
        // SEPARATE → UNIFIED: Use forward speed value for both
        unifiedGroup.style.display = 'flex';
        separateGroup.style.display = 'none';
        
        const forwardSpeed = parseFloat(document.getElementById('speedForward').value);
        
        // Use forward speed as the unified value
        document.getElementById('speedUnified').value = forwardSpeed;
        
        // Also update backward to match forward
        document.getElementById('speedBackward').value = forwardSpeed;
        
        // Apply forward speed to BOTH directions immediately
        sendCommand(WS_CMD.SET_SPEED_FORWARD, {speed: forwardSpeed});
        sendCommand(WS_CMD.SET_SPEED_BACKWARD, {speed: forwardSpeed});
        
        // Update preset button highlighting
        document.querySelectorAll('[data-speed-unified]').forEach(btn => {
          if (parseFloat(btn.getAttribute('data-speed-unified')) === forwardSpeed) {
            btn.classList.add('active');
          } else {
            btn.classList.remove('active');
          }
        });
        
        console.log('Switched to UNIFIED mode: using forward speed ' + forwardSpeed + ' for both directions');
      }
    });
  });
  
  console.log('✅ simple-mode.js listeners initialized');
}

// ============================================================================
// EXPORTS (Browser globals)
// ============================================================================
window.sendSimpleCyclePauseConfig = sendSimpleCyclePauseConfig;
window.initSimpleModeListeners = initSimpleModeListeners;

console.log('✅ simple-mode.js loaded');

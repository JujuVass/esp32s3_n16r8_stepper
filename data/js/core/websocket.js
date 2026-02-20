/**
 * ============================================================================
 * websocket.js - WebSocket Connection & Message Handlers
 * ============================================================================
 * Manages WebSocket connection to ESP32 and routes incoming messages
 * 
 * Dependencies:
 * - app.js (AppState, WS_CMD)
 * - utils.js (showNotification)
 * - ui.js (updateUI) - loaded after this file
 * ============================================================================
 */

// ============================================================================
// WEBSOCKET CONNECTION
// ============================================================================

/**
 * Check if two IPs are on the same /24 subnet
 * @param {string} ip1 - First IP address
 * @param {string} ip2 - Second IP address
 * @returns {boolean} True if same /24 subnet
 */
function isSameSubnet(ip1, ip2) {
  if (!ip1 || !ip2) return false;
  const parts1 = ip1.split('.');
  const parts2 = ip2.split('.');
  if (parts1.length !== 4 || parts2.length !== 4) return false;
  return parts1[0] === parts2[0] && parts1[1] === parts2[1] && parts1[2] === parts2[2];
}

/**
 * Establish WebSocket connection to ESP32
 * Auto-reconnects on disconnect with 2 second delay
 * Uses IP address when available for faster/more reliable connection
 * Falls back to hostname (mDNS) if IP connection fails
 */
function connectWebSocket(useFallback) {
  // Determine which host to use
  let host;
  if (useFallback) {
    // Fallback mode: use hostname (mDNS), clear cached IP as it may be stale
    host = window.location.hostname;
    AppState.espIpAddress = null;
    console.debug('🔄 Fallback: Using hostname (mDNS):', host);
  } else {
    // Only use cached IP if it's on the same subnet as our current connection
    // This prevents using STA IP (192.168.1.x) when connected via AP (192.168.4.x)
    const currentHost = window.location.hostname;
    if (AppState.espIpAddress && isSameSubnet(AppState.espIpAddress, currentHost)) {
      host = AppState.espIpAddress;
    } else {
      host = currentHost;
      if (AppState.espIpAddress && !isSameSubnet(AppState.espIpAddress, currentHost)) {
        console.debug('⚠️ Cached IP', AppState.espIpAddress, 'not on same subnet as', currentHost, '- using current host');
        AppState.espIpAddress = null;  // Clear stale cross-subnet IP
      }
    }
  }
  
  const wsUrl = 'ws://' + host + ':81';
  console.debug('🔌 Connecting to WebSocket:', wsUrl);
  
  // Track connection attempt for fallback logic
  AppState._wsConnectStartTime = Date.now();
  AppState._wsUsingCachedIp = (host === AppState.espIpAddress);
  
  AppState.ws = new WebSocket(wsUrl);
  
  // ========================================================================
  // ON OPEN - Connection established
  // ========================================================================
  AppState.ws.onopen = function() {
    console.debug('✅ WebSocket connected');
    AppState._wsRetryCount = 0;  // Reset retry counter on success
    
    // Initialize UI elements that depend on connection
    if (typeof updatePatternToggleButton === 'function') {
      updatePatternToggleButton();
    }
    
    // Sync time then request initial status immediately
    sendCommand('syncTime', { time: Date.now() });
    sendCommand(WS_CMD.GET_STATUS, {});
  };
  
  // ========================================================================
  // ON MESSAGE - Handle incoming data
  // ========================================================================
  AppState.ws.onmessage = function(event) {
    try {
      const data = JSON.parse(event.data);
      
      // Route message based on type
      handleWebSocketMessage(data);
      
    } catch (e) {
      console.error('WebSocket parse error:', e, 'Raw data:', event.data);
      showNotification(t('common.error') + ': ' + e.message, 'error');
    }
  };
  
  // ========================================================================
  // ON CLOSE - Connection lost
  // ========================================================================
  AppState.ws.onclose = function() {
    // Don't auto-reconnect during WiFi refresh
    if (AppState.wifiReconnectInProgress) {
      console.debug('📶 WebSocket closed (WiFi reconnect in progress - no auto-reconnect)');
      return;
    }
    
    const connectionDuration = Date.now() - (AppState._wsConnectStartTime || 0);
    
    // If connection failed quickly (<2s) and we were using cached IP, try fallback
    if (connectionDuration < 2000 && AppState._wsUsingCachedIp && !useFallback) {
      console.debug('⚠️ Quick disconnect with cached IP - trying hostname fallback...');
      setTimeout(function() { connectWebSocket(true); }, 500);
      return;
    }
    
    console.debug('❌ WebSocket disconnected. Reconnecting in 2s...');
    
    const stateEl = document.getElementById('state');
    if (stateEl) {
      stateEl.textContent = t('status.disconnectedReconnecting');
    }
    
    // Auto-reconnect after delay (normal mode, will use IP if available)
    setTimeout(function() { connectWebSocket(false); }, 2000);
  };
  
  // ========================================================================
  // ON ERROR - Connection error
  // ========================================================================
  AppState.ws.onerror = function(error) {
    console.error('WebSocket error:', error);
    
    // If error with cached IP, try fallback
    if (AppState._wsUsingCachedIp && !useFallback) {
      console.debug('⚠️ WS error with cached IP - will try hostname fallback...');
      // Let onclose handle the fallback
      return;
    }
    
    const stateEl = document.getElementById('state');
    if (stateEl) {
      stateEl.textContent = t('status.errorState');
    }
    
    showNotification(t('status.wsError'), 'error');
  };
}

// ============================================================================
// MESSAGE ROUTER - Route messages to appropriate handlers
// ============================================================================

/**
 * Route incoming WebSocket messages to appropriate handlers
 * @param {object} data - Parsed JSON message from ESP32
 */
function handleWebSocketMessage(data) {
  // Basic schema validation — must be an object with identifiable structure
  if (!data || typeof data !== 'object') {
    console.warn('WS: Ignoring non-object message');
    return;
  }

  // Error messages (high priority)
  if (data.type === 'error') {
    showNotification(data.message, 'error');
    return;
  }
  
  // Sequence table data
  if (data.type === 'sequenceTable') {
    if (typeof renderSequenceTable === 'function') {
      renderSequenceTable(data.data);
    }
    return;
  }
  
  // Sequence execution status
  if (data.type === 'sequenceStatus') {
    if (typeof updateSequenceStatus === 'function') {
      updateSequenceStatus(data);
    }
    return;
  }
  
  // Export data (trigger download)
  if (data.type === 'exportData') {
    handleExportData(data.data);
    return;
  }
  
  // Filesystem listing
  if (data.type === 'fsList') {
    handleFileSystemList(data.files);
    return;
  }
  
  // Log messages from backend
  if (data.type === 'log') {
    handleLogMessage(data);
    return;
  }
  
  // Cache ESP32 IP address for faster WS reconnection (avoids mDNS delay)
  // Only cache if on the same subnet as current connection (prevents STA IP cache when on AP)
  if (data.ip && data.ip !== '0.0.0.0' && !AppState.espIpAddress) {
    if (isSameSubnet(data.ip, window.location.hostname)) {
      AppState.espIpAddress = data.ip;
      console.debug('📡 Cached ESP32 IP:', data.ip);
    }
  }
  
  // Default: Status update for main UI
  if (typeof updateUI === 'function') {
    updateUI(data);
  }
}

// ============================================================================
// MESSAGE HANDLERS - Specific message type processors
// ============================================================================

/**
 * Handle export data - trigger file download
 * @param {object} exportData - Data to export as JSON
 */
function handleExportData(exportData) {
  const jsonStr = JSON.stringify(exportData, null, 2);
  const blob = new Blob([jsonStr], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  
  const a = document.createElement('a');
  a.href = url;
  a.download = 'sequence_' + new Date().toISOString().slice(0,10) + '.json';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  
  URL.revokeObjectURL(url);
  console.debug('📥 Export downloaded');
}

/**
 * Handle filesystem listing - update index.html timestamp display
 * @param {array} files - List of files with names and timestamps
 */
function handleFileSystemList(files) {
  if (!files || files.length === 0) return;
  
  const idxFile = files.find(f => f.name === '/index.html' || f.name === 'index.html');
  if (idxFile) {
    const el = document.getElementById('fsIndexTimestamp');
    if (el) {
      el.textContent = 'index.html: ' + idxFile.time;
    }
  }
}

/**
 * Handle log messages from backend - display in console panel
 * @param {object} logData - Log message with level and content
 */
function handleLogMessage(logData) {
  const logPanel = document.getElementById('logConsolePanel');
  if (!logPanel) return;
  
  const level = logData.level || 'INFO';
  const msg = logData.message || '';
  
  // Color coding based on log level (VSCode Dark+ theme inspired)
  const colors = {
    'ERROR': '#f48771',   // Red
    'WARN': '#dcdcaa',    // Yellow
    'INFO': '#4ec9b0',    // Cyan
    'DEBUG': '#9cdcfe'    // Blue
  };
  const color = colors[level] || '#d4d4d4';
  
  const locale = (typeof I18n !== 'undefined' && I18n.getLang() === 'en') ? 'en-US' : 'fr-FR';
  const timestamp = new Date().toLocaleTimeString(locale, { 
    hour: '2-digit', 
    minute: '2-digit', 
    second: '2-digit' 
  });
  
  const lineEl = document.createElement('div');
  lineEl.style.color = color;
  lineEl.textContent = `[${timestamp}] [${level}] ${msg}`;
  
  logPanel.appendChild(lineEl);
  logPanel.scrollTop = logPanel.scrollHeight;
  
  // Limit to 500 lines to prevent memory issues
  while (logPanel.children.length > 500) {
    logPanel.removeChild(logPanel.firstChild);
  }
}

// Log initialization
console.debug('✅ websocket.js loaded - WebSocket handlers ready');

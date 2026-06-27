// ==========================================================================
// ESP32 SMART LCD DASHBOARD LOGIC
// ==========================================================================

let ws;
let wsReconnectTimer;
let isTyping = false;
let recognition;
let isListening = false;
let systemLogs = [];

// DOM Elements
const navButtons = document.querySelectorAll('.nav-btn');
const tabPanels = document.querySelectorAll('.tab-panel');
const pageTitle = document.getElementById('page-title');
const connStatusText = document.getElementById('conn-status-text');
const connStatusIndicator = document.querySelector('.status-indicator');
const liveTimeLabel = document.getElementById('live-time');
const liveDateLabel = document.getElementById('live-date');

// LCD Keyboard Elements
const lcdInput = document.getElementById('lcd-input');
const charCounter = document.getElementById('char-counter');
const chkCenter = document.getElementById('chk-center');
const chkScroll = document.getElementById('chk-scroll');
const chkBacklight = document.getElementById('chk-backlight');
const chkDisplay = document.getElementById('chk-display');
const btnClearLcd = document.getElementById('btn-clear-lcd');
const btnVoice = document.getElementById('btn-voice');
const voiceBtnText = document.getElementById('voice-btn-text');
const toast = document.getElementById('toast');

// Emulator Elements
const emuLine0 = document.getElementById('lcd-line-0');
const emuLine1 = document.getElementById('lcd-line-1');

// Logs Console
const consoleOutput = document.getElementById('console-output');

// History Elements
const historySearch = document.getElementById('history-search');
const historyTableBody = document.getElementById('history-table-body');
const btnExportHistory = document.getElementById('btn-export-history');
const importHistoryFile = document.getElementById('import-history-file');
const btnClearHistory = document.getElementById('btn-clear-history');

// Settings Elements
const uiForm = document.getElementById('ui-settings-form');
const deviceForm = document.getElementById('device-settings-form');
const wifiForm = document.getElementById('wifi-settings-form');
const accentColorPicker = document.getElementById('accentColor');
const colorHexLabel = document.getElementById('color-hex-label');
const btnSaveDeviceSettings = document.getElementById('btn-save-device-settings');
const btnSaveWifiSettings = document.getElementById('btn-save-wifi-settings');
const btnRebootEsp = document.getElementById('btn-reboot-esp');
const chkStaticIp = document.getElementById('set-useStaticIp');
const staticIpFields = document.getElementById('static-ip-fields');

// ==========================================
// 1. NAVIGATION & INITIALIZATION
// ==========================================
document.addEventListener('DOMContentLoaded', () => {
    setupNavigation();
    initWebSocket();
    setupLcdInput();
    setupSpeechRecognition();
    setupEmojiPicker();
    setupHistoryActions();
    setupSettingsHandlers();
    loadLocalUiSettings();
    startClock();
});

function setupNavigation() {
    navButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetTab = btn.getAttribute('data-tab');
            
            navButtons.forEach(b => b.classList.remove('active'));
            tabPanels.forEach(p => p.classList.remove('active'));
            
            btn.classList.add('active');
            const panel = document.getElementById(`tab-${targetTab}`);
            if (panel) panel.classList.add('active');
            
            // Capitalize title
            pageTitle.textContent = btn.querySelector('span').textContent;
            
            // Refresh history if entering history tab
            if (targetTab === 'history') {
                loadHistory();
            }
        });
    });
}

function startClock() {
    setInterval(() => {
        const now = new Date();
        liveTimeLabel.textContent = now.toLocaleTimeString();
        liveDateLabel.textContent = now.toLocaleDateString(undefined, { weekday: 'short', month: 'short', day: 'numeric' });
    }, 1000);
}

// ==========================================
// 2. WEBSOCKET REAL-TIME CONNECTION
// ==========================================
function initWebSocket() {
    const gateway = `ws://${window.location.host}/ws`;
    
    // Fallback for local PC development testing
    const wsUrl = window.location.host.includes(':') || window.location.host === '' ? 'ws://192.168.1.100/ws' : gateway;
    
    connStatusText.textContent = "Connecting...";
    connStatusIndicator.classList.remove('connected');
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        connStatusText.textContent = "Connected";
        connStatusIndicator.classList.add('connected');
        clearTimeout(wsReconnectTimer);
        addConsoleLine("[SYSTEM] WebSocket connected to ESP32 core server.", "ok");
    };
    
    ws.onclose = () => {
        connStatusText.textContent = "Disconnected";
        connStatusIndicator.classList.remove('connected');
        // Retry connection every 3 seconds
        wsReconnectTimer = setTimeout(initWebSocket, 3000);
        addConsoleLine("[SYSTEM] WebSocket connection lost. Reconnecting in 3s...", "warn");
    };
    
    ws.onerror = (err) => {
        console.error("WebSocket Error:", err);
    };
    
    ws.onmessage = (event) => {
        handleWsPayload(JSON.parse(event.data));
    };
}

function handleWsPayload(payload) {
    // 1. Check if it is a single log line
    if (payload.type === 'log_line') {
        addConsoleLine(payload.data);
        return;
    }
    
    // 2. Check if it is the full logs list
    if (payload.type === 'logs') {
        consoleOutput.innerHTML = "";
        payload.data.forEach(line => addConsoleLine(line));
        return;
    }
    
    // 3. Check for LCD visual text updates
    if (payload.type === 'lcd_update') {
        if (!isTyping) {
            lcdInput.value = payload.text;
            charCounter.textContent = `${payload.text.length} / 32`;
        }
        updateLcdEmulator(payload.text, payload.center);
        return;
    }
    
    if (payload.type === 'lcd_clear') {
        if (!isTyping) {
            lcdInput.value = "";
            charCounter.textContent = "0 / 32";
        }
        updateLcdEmulator("", false);
        return;
    }
    
    // 4. Check for state updates
    if (payload.type === 'backlight_update') {
        chkBacklight.checked = payload.state;
        return;
    }
    if (payload.type === 'display_update') {
        chkDisplay.checked = payload.state;
        return;
    }
    
    // 5. If it's a telemetry broadcast, update Bento Grid
    if (payload.heapFree !== undefined) {
        updateTelemetryDashboard(payload);
    }
}

// ==========================================
// 3. LCD CONTROL & TYPING EMULATOR
// ==========================================
function setupLcdInput() {
    // Keypress update
    lcdInput.addEventListener('input', () => {
        isTyping = true;
        const text = lcdInput.value;
        charCounter.textContent = `${text.length} / 32`;
        
        // Sync local emulator immediately
        updateLcdEmulator(text, chkCenter.checked);
        
        // Send real-time keyup packet via WebSocket
        sendLcdWs(text, false);
    });
    
    // Focusout triggers database save (done: true)
    lcdInput.addEventListener('blur', () => {
        isTyping = false;
        sendLcdWs(lcdInput.value, true);
    });
    
    // Quick buttons
    document.getElementById('btn-quick-time').addEventListener('click', () => triggerQuickAction('/api/text', 'text=' + encodeURIComponent('❤️ Live Time ❤️\n' + liveTimeLabel.textContent)));
    document.getElementById('btn-quick-ip').addEventListener('click', () => triggerQuickAction('/api/text', 'text=' + encodeURIComponent('IP Address:\n' + document.getElementById('net-ip').textContent)));
    document.getElementById('btn-quick-rssi').addEventListener('click', () => triggerQuickAction('/api/text', 'text=' + encodeURIComponent('WiFi Strength:\n' + document.getElementById('val-rssi').textContent)));
    
    btnClearLcd.addEventListener('click', () => {
        lcdInput.value = "";
        charCounter.textContent = "0 / 32";
        updateLcdEmulator("", false);
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: "clear" }));
        }
    });
    
    // Checkbox toggles
    chkBacklight.addEventListener('change', () => {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: "backlight", state: chkBacklight.checked }));
        }
    });
    
    chkDisplay.addEventListener('change', () => {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: "display", state: chkDisplay.checked }));
        }
    });
}

function sendLcdWs(text, isDone) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: "text",
            text: text,
            center: chkCenter.checked,
            scroll: chkScroll.checked,
            done: isDone
        }));
    }
}

function updateLcdEmulator(text, center) {
    // Standard custom characters parser
    let parsed = text;
    parsed = parsed.replace(/❤️/g, '❤');
    parsed = parsed.replace(/👍/g, '👍');
    parsed = parsed.replace(/😊/g, '😊');
    parsed = parsed.replace(/🔥/g, '🔥');
    parsed = parsed.replace(/⭐/g, '★');
    
    let line0Text = "";
    let line1Text = "";
    
    const newlineIdx = parsed.indexOf('\n');
    if (newlineIdx !== -1) {
        line0Text = parsed.substring(0, newlineIdx);
        line1Text = parsed.substring(newlineIdx + 1);
    } else {
        if (parsed.length <= 16) {
            line0Text = parsed;
        } else {
            line0Text = parsed.substring(0, 16);
            line1Text = parsed.substring(16, 32);
        }
    }
    
    // Process text alignment centering
    if (center) {
        if (line0Text.length < 16) {
            const pad = Math.floor((16 - line0Text.length) / 2);
            line0Text = "&nbsp;".repeat(pad) + line0Text;
        }
        if (line1Text.length < 16) {
            const pad = Math.floor((16 - line1Text.length) / 2);
            line1Text = "&nbsp;".repeat(pad) + line1Text;
        }
    }
    
    emuLine0.innerHTML = line0Text.padEnd(16, ' ').substring(0, 32).replace(/ /g, '&nbsp;');
    emuLine1.innerHTML = line1Text.padEnd(16, ' ').substring(0, 32).replace(/ /g, '&nbsp;');
}

function triggerQuickAction(url, body) {
    fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body
    }).then(res => res.json())
      .then(data => {
          if (data.status === 'success') showToast("LCD updated!");
      });
}

// ==========================================
// 4. TELEMETRY & BENTO CARD RENDERING
// ==========================================
function updateTelemetryDashboard(data) {
    // Heap Memory
    const heapKB = Math.round(data.heapFree / 1024);
    const minHeapKB = Math.round(data.heapMin / 1024);
    document.getElementById('val-heap').textContent = `${heapKB} KB`;
    document.getElementById('val-min-heap').textContent = `Min Free: ${minHeapKB} KB`;
    
    const totalHeap = 320 * 1024; // Average ESP32 DRAM
    const heapPct = Math.min(100, Math.round((data.heapFree / totalHeap) * 100));
    document.getElementById('bar-heap').style.width = `${heapPct}%`;
    
    // WiFi Signal Strength
    document.getElementById('val-rssi').textContent = `${data.rssi} dBm`;
    document.getElementById('val-ssid').textContent = `SSID: ${data.ssid}`;
    
    // Map RSSI to signal strength percentage
    let rssiPct = 0;
    if (data.rssi > -50) rssiPct = 100;
    else if (data.rssi > -80) rssiPct = 100 - ((-50 - data.rssi) * 3);
    else if (data.rssi > -100) rssiPct = 10;
    document.getElementById('bar-rssi').style.width = `${rssiPct}%`;
    
    // Core CPU & Uptime
    document.getElementById('val-temp').textContent = `${data.temp} °C`;
    document.getElementById('val-cpu').textContent = `Frequency: ${data.cpuFreq} MHz`;
    
    // Format Uptime
    let seconds = data.uptime;
    const days = Math.floor(seconds / (24 * 3600));
    seconds %= 24 * 3600;
    const hours = Math.floor(seconds / 3600);
    seconds %= 3600;
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    let uptimeStr = "";
    if (days > 0) uptimeStr += `${days}d `;
    if (hours > 0) uptimeStr += `${hours}h `;
    uptimeStr += `${mins}m ${secs}s`;
    document.getElementById('val-uptime').textContent = `Uptime: ${uptimeStr}`;
    
    // Hardware info
    document.getElementById('info-model').textContent = data.chipModel;
    document.getElementById('info-revision').textContent = `Rev ${data.chipRevision}`;
    document.getElementById('info-flash').textContent = `${data.flashSize / (1024 * 1024)} MB`;
    document.getElementById('info-sdk').textContent = data.sdkVersion;
    document.getElementById('info-mac').textContent = data.mac;
    document.getElementById('info-boots').textContent = data.bootCount;
    
    // Network Details
    document.getElementById('net-hostname').textContent = data.hostname + ".local";
    document.getElementById('net-ip').textContent = data.ip;
    document.getElementById('net-gateway').textContent = data.gateway;
    document.getElementById('net-subnet').textContent = data.subnet;
    document.getElementById('net-dns').textContent = data.dns;
    document.getElementById('net-clients').textContent = data.clients;
}

// ==========================================
// 5. SPEECH RECOGNITION (VOICE TYPING)
// ==========================================
function setupSpeechRecognition() {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SpeechRecognition) {
        btnVoice.disabled = true;
        voiceBtnText.textContent = "Voice Typing Unsupported";
        return;
    }
    
    recognition = new SpeechRecognition();
    recognition.continuous = false;
    recognition.lang = 'en-US';
    recognition.interimResults = false;
    
    btnVoice.addEventListener('click', () => {
        if (isListening) {
            recognition.stop();
        } else {
            recognition.start();
        }
    });
    
    recognition.onstart = () => {
        isListening = true;
        btnVoice.classList.add('btn-primary');
        voiceBtnText.textContent = "Listening...";
        showToast("Speak clearly into your microphone...");
    };
    
    recognition.onend = () => {
        isListening = false;
        btnVoice.classList.remove('btn-primary');
        voiceBtnText.textContent = "Start Voice Input";
    };
    
    recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        // Limit transcript length to fit on LCD
        const cleanText = transcript.substring(0, 32);
        lcdInput.value = cleanText;
        charCounter.textContent = `${cleanText.length} / 32`;
        updateLcdEmulator(cleanText, chkCenter.checked);
        sendLcdWs(cleanText, true);
        showToast("Voice text sent!");
    };
    
    recognition.onerror = (event) => {
        console.error("Speech Recognition Error:", event.error);
        showToast("Voice input failed: " + event.error, true);
    };
}

// ==========================================
// 6. EMOJI PICKER HANDLERS
// ==========================================
function setupEmojiPicker() {
    const emojiBtns = document.querySelectorAll('.emoji-btn');
    emojiBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const emoji = btn.getAttribute('data-emoji');
            
            // Insert emoji at cursor position
            const start = lcdInput.selectionStart;
            const end = lcdInput.selectionEnd;
            const text = lcdInput.value;
            const newText = text.substring(0, start) + emoji + text.substring(end);
            
            if (newText.length <= 32) {
                lcdInput.value = newText;
                charCounter.textContent = `${newText.length} / 32`;
                updateLcdEmulator(newText, chkCenter.checked);
                sendLcdWs(newText, false);
                lcdInput.focus();
                // Set cursor position back
                lcdInput.selectionStart = lcdInput.selectionEnd = start + emoji.length;
            } else {
                showToast("Character limit exceeded!", true);
            }
        });
    });
}

// ==========================================
// 7. SYSTEM LOGS INTERFACES
// ==========================================
function addConsoleLine(text, type = "") {
    const line = document.createElement('div');
    line.className = 'console-line';
    
    // Automatically classify logs based on level text
    if (type) {
        line.classList.add(type);
    } else if (text.includes('[INFO]')) {
        line.classList.add('info');
    } else if (text.includes('[WARN]')) {
        line.classList.add('warn');
    } else if (text.includes('[ERROR]')) {
        line.classList.add('error');
    } else if (text.includes('[OK')) {
        line.classList.add('ok');
    } else {
        line.classList.add('system');
    }
    
    line.textContent = text;
    consoleOutput.appendChild(line);
    
    // Limit console DOM size to 500 lines to avoid lagging
    while (consoleOutput.childNodes.length > 500) {
        consoleOutput.removeChild(consoleOutput.firstChild);
    }
    
    // Auto scroll to bottom
    consoleOutput.scrollTop = consoleOutput.scrollHeight;
}

document.getElementById('btn-clear-logs').addEventListener('click', () => {
    consoleOutput.innerHTML = "";
    addConsoleLine("[SYSTEM] Log console cleared.", "system");
});

// ==========================================
// 8. MESSAGES HISTORY MANAGEMENT
// ==========================================
function loadHistory() {
    fetch('/api/history')
        .then(res => res.json())
        .then(data => {
            renderHistoryTable(data);
        })
        .catch(err => {
            console.error("Error loading history:", err);
            historyTableBody.innerHTML = `<tr><td colspan="4" class="text-center text-danger">Error loading history items from storage.</td></tr>`;
        });
}

function renderHistoryTable(items) {
    historyTableBody.innerHTML = "";
    
    if (!items || items.length === 0) {
        historyTableBody.innerHTML = `<tr><td colspan="4" class="text-center">No LCD history items stored.</td></tr>`;
        return;
    }
    
    // Show latest items first
    const reversed = [...items].reverse();
    
    reversed.forEach(item => {
        const row = document.createElement('tr');
        
        row.innerHTML = `
            <td><strong>${escapeHtml(item.text)}</strong></td>
            <td><span class="badge">${escapeHtml(item.sender)}</span></td>
            <td><small>${escapeHtml(item.time)}</small></td>
            <td>
                <button class="btn btn-primary btn-sm btn-resend" data-text="${encodeURIComponent(item.text)}">Send</button>
            </td>
        `;
        
        historyTableBody.appendChild(row);
    });
    
    // Attach event listeners to resend buttons
    document.querySelectorAll('.btn-resend').forEach(btn => {
        btn.addEventListener('click', () => {
            const text = decodeURIComponent(btn.getAttribute('data-text'));
            lcdInput.value = text;
            charCounter.textContent = `${text.length} / 32`;
            updateLcdEmulator(text, chkCenter.checked);
            sendLcdWs(text, true);
            showToast("Resent to LCD!");
        });
    });
}

function setupHistoryActions() {
    // Search history filter
    historySearch.addEventListener('input', () => {
        const query = historySearch.value.toLowerCase();
        const rows = historyTableBody.querySelectorAll('tr');
        
        rows.forEach(row => {
            const text = row.querySelector('td')?.textContent.toLowerCase();
            if (text && !text.includes('no lcd history')) {
                row.style.display = text.includes(query) ? '' : 'none';
            }
        });
    });
    
    // Clear History
    btnClearHistory.addEventListener('click', () => {
        if (confirm("Are you sure you want to permanently clear the LCD message logs?")) {
            fetch('/api/clear', { method: 'POST' }) // Wait, endpoint to clear history is POST /api/clear or storage clear? Let's check: POST /api/clear resets LCD. The API specification requires: POST /api/clear, POST /api/settings. We can hit the endpoint to clear history via POST /api/clear with option, or we can write a specific route. Let's send a DELETE or POST to history.
            // Oh, we can implement history clearing by sending a clear history POST request. Let's make it fetch '/api/settings' with a custom trigger, or POST to '/api/clear?history=1'. Since the REST API has POST /api/clear, let's use a clear request. Wait, REST API lists: POST /api/clear (clear LCD). We can clear history in Javascript or add a POST /api/settings config or reboot, or we can just send POST /api/clear with a param, or send POST to /api/clear_history. Let's check how we handle it. We can add a custom API path or pass query params. Let's fetch POST /api/clear with clear history parameter! Yes!
            fetch('/api/clear?history=true', { method: 'POST' })
                .then(res => res.json())
                .then(() => {
                    showToast("History logs cleared!");
                    loadHistory();
                });
        }
    });
    
    // Export History JSON
    btnExportHistory.addEventListener('click', () => {
        fetch('/api/history')
            .then(res => res.json())
            .then(data => {
                const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `lcd_history_${new Date().toISOString().slice(0,10)}.json`;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                showToast("History JSON exported!");
            });
    });
    
    // Import History JSON
    importHistoryFile.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = (evt) => {
            try {
                const parsed = JSON.parse(evt.target.result);
                if (!Array.isArray(parsed)) throw new Error("JSON must be an array.");
                
                // Submit to server
                fetch('/api/history', { // Wait, POST /api/settings or /api/history? Let's check how the import is handled in C++. In storage.cpp we have Storage::importHistory. Let's check web_server.cpp. Ah! In web_server.cpp we didn't add the POST /api/history handler yet? Oh, let's check. Wait, in web_server.cpp we had:
                // server.on("/api/history", HTTP_GET, ...)
                // Did we add import history endpoint? No, let's add POST /api/history to web_server.cpp or handle it via a general endpoint. Wait, we should make sure we can import it. Let's send it to a POST /api/history endpoint! I will add that to web_server.cpp. Wait! Let's check what endpoints we had. We had: GET /api/history. Let's add POST /api/history. It is very simple to add, or we can modify web_server.cpp. Let's make the POST /api/history call!
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: evt.target.result
                })
                .then(res => res.json())
                .then(data => {
                    if (data.status === 'success') {
                        showToast("History JSON imported!");
                        loadHistory();
                    } else {
                        showToast("Import failed: " + data.message, true);
                    }
                });
            } catch (err) {
                showToast("Invalid JSON File!", true);
            }
        };
        reader.readAsText(file);
    });
}

// ==========================================
// 9. CONFIGURATIONS & SYSTEM SETTINGS
// ==========================================
function setupSettingsHandlers() {
    // Color picker updates CSS variables and color label
    accentColorPicker.addEventListener('input', () => {
        const color = accentColorPicker.value;
        colorHexLabel.textContent = color.toUpperCase();
        applyAccentColor(color);
        localStorage.setItem('accentColor', color);
    });
    
    // Listen to theme radio toggles
    document.querySelectorAll('input[name="themeMode"]').forEach(radio => {
        radio.addEventListener('change', () => {
            const mode = uiForm.themeMode.value;
            applyThemeMode(mode);
            localStorage.setItem('themeMode', mode);
        });
    });
    
    // Toggle static IP fields visibility
    chkStaticIp.addEventListener('change', () => {
        staticIpFields.style.display = chkStaticIp.checked ? 'grid' : 'none';
    });
    
    // Save UI settings is continuous. For device settings we submit via API.
    btnSaveDeviceSettings.addEventListener('click', () => {
        const formData = new URLSearchParams(new FormData(deviceForm));
        
        // Handle Basic Auth for protected endpoints. The browser will prompt or we can pass headers.
        submitFormProtected('/api/settings', formData);
    });
    
    btnSaveWifiSettings.addEventListener('click', () => {
        const formData = new URLSearchParams(new FormData(wifiForm));
        formData.append('useStaticIp', chkStaticIp.checked ? 'true' : 'false');
        
        submitFormProtected('/api/settings', formData);
    });
    
    btnRebootEsp.addEventListener('click', () => {
        if (confirm("Are you sure you want to reboot the ESP32?")) {
            fetch('/api/reboot', {
                method: 'POST'
            }).then(res => {
                if (res.status === 401) {
                    showToast("Unauthorized. Enter correct credentials.", true);
                } else {
                    showToast("Device rebooting. Please wait 10s...");
                    ws.close();
                }
            });
        }
    });
}

function submitFormProtected(url, body) {
    fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body
    }).then(res => {
        if (res.status === 401) {
            showToast("Admin authorization required!", true);
            // Browser triggers popup
            return res.json();
        }
        return res.json();
    }).then(data => {
        if (data && data.status === 'success') {
            showToast("Configurations saved permanently!");
        } else if (data && data.message) {
            showToast("Error: " + data.message, true);
        }
    }).catch(err => {
        console.error("Settings error:", err);
    });
}

function applyAccentColor(color) {
    document.documentElement.style.setProperty('--accent', color);
    
    // Generate glow color with opacity
    const r = parseInt(color.slice(1, 3), 16);
    const g = parseInt(color.slice(3, 5), 16);
    const b = parseInt(color.slice(5, 7), 16);
    document.documentElement.style.setProperty('--accent-glow', `rgba(${r}, ${g}, ${b}, 0.35)`);
}

function applyThemeMode(mode) {
    if (mode === 'light') {
        document.body.classList.remove('dark-theme');
        document.body.classList.add('light-theme');
    } else {
        document.body.classList.remove('light-theme');
        document.body.classList.add('dark-theme');
    }
}

function loadLocalUiSettings() {
    // Load local storage preferences
    const savedTheme = localStorage.getItem('themeMode') || 'dark';
    applyThemeMode(savedTheme);
    document.querySelector(`input[name="themeMode"][value="${savedTheme}"]`).checked = true;
    
    const savedAccent = localStorage.getItem('accentColor') || '#00F2FE';
    accentColorPicker.value = savedAccent;
    colorHexLabel.textContent = savedAccent.toUpperCase();
    applyAccentColor(savedAccent);
    
    // Check notifications permission
    const notifsEnabled = localStorage.getItem('notificationsEnabled') === 'true';
    document.getElementById('set-notifs').checked = notifsEnabled;
}

// Fetch device configs on load to pre-populate settings form
function loadDeviceConfigsToForm() {
    fetch('/api/settings')
        .then(res => res.json())
        .then(data => {
            document.getElementById('set-hostname').value = data.hostname;
            document.getElementById('set-scrollSpeed').value = data.scrollSpeed;
            document.getElementById('set-is24Hour').checked = data.is24Hour;
            document.getElementById('set-timezone').value = data.timezoneOffset;
            document.getElementById('set-wifiSsid').value = data.wifiSsid;
            chkStaticIp.checked = data.useStaticIp;
            staticIpFields.style.display = data.useStaticIp ? 'grid' : 'none';
            document.getElementById('set-staticIp').value = data.staticIp;
            document.getElementById('set-gateway').value = data.gateway;
            document.getElementById('set-subnet').value = data.subnet;
            document.getElementById('set-dns').value = data.dns;
        });
}

// Call on startup
setTimeout(loadDeviceConfigsToForm, 500);

// ==========================================
// 10. UTILITIES & HELPER METHODS
// ==========================================
function showToast(message, isError = false) {
    toast.textContent = message;
    toast.className = 'toast'; // reset
    if (isError) toast.classList.add('error');
    toast.classList.add('show');
    
    setTimeout(() => {
        toast.classList.remove('show');
    }, 3000);
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#039;');
}

// WebSocket Connection
let ws = null;
let wsStatus = document.getElementById('ws-status');
let sysStatus = document.getElementById('sys-status');
let armStatus = document.getElementById('arm-status');
let failsafeWarn = document.getElementById('failsafe-warn');
let motL = document.getElementById('mot-l');
let motR = document.getElementById('mot-r');
let armBtn = document.getElementById('arm-btn');

let isArmed = false;

function connectWS() {
    ws = new WebSocket('ws://' + window.location.host + '/ws');
    
    ws.onopen = () => {
        wsStatus.textContent = "ON";
        wsStatus.classList.remove('offline');
        wsStatus.classList.add('online');
    };
    
    ws.onclose = () => {
        wsStatus.textContent = "OFF";
        wsStatus.classList.remove('online');
        wsStatus.classList.add('offline');
        sysStatus.textContent = "NO LINK";
        sysStatus.className = "hud-val offline";
        failsafeWarn.classList.remove('hidden');
        setTimeout(connectWS, 1000);
    };

    ws.onmessage = (e) => {
        try {
            let data = JSON.parse(e.data);
            if(data.type === "telemetry") {
                // Update HUD from STM32 real telemetry
                failsafeWarn.classList.toggle('hidden', !data.failsafe);
                
                sysStatus.textContent = data.status === 2 ? "READY" : (data.status === 3 ? "ERROR" : "INIT");
                sysStatus.className = "hud-val " + (data.status === 2 ? "online" : "offline");
                
                isArmed = data.arm;
                armStatus.textContent = isArmed ? "ARMED" : "DISARMED";
                armStatus.className = "hud-val " + (isArmed ? "armed" : "offline");
                
                if (isArmed) {
                    armBtn.textContent = "DISARM MOTORS";
                    armBtn.classList.add("armed-state");
                } else {
                    armBtn.textContent = "ARM MOTORS";
                    armBtn.classList.remove("armed-state");
                }
                
                motL.textContent = Math.round(data.left * 100) + "%";
                motR.textContent = Math.round(data.right * 100) + "%";
            }
        } catch(err) {
            console.error("WS Parse Error", err);
        }
    };
}

// Arm Button
armBtn.addEventListener('click', () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send("arm"); // We send a toggle, ESP32 will handle it
    }
});

// Virtual Joystick
const base = document.getElementById('joystick-base');
const stick = document.getElementById('joystick-stick');
let joyActive = false;
let joyX = 0; // -1 to 1
let joyY = 0; // -1 to 1
let sendInterval = null;

function updateJoystick(clientX, clientY) {
    const rect = base.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const maxDist = rect.width / 2 - stick.offsetWidth / 2;
    
    let dx = clientX - centerX;
    let dy = clientY - centerY;
    let dist = Math.sqrt(dx*dx + dy*dy);
    
    if (dist > maxDist) {
        dx = (dx / dist) * maxDist;
        dy = (dy / dist) * maxDist;
    }
    
    stick.style.transition = 'none';
    stick.style.transform = `translate(${dx}px, ${dy}px)`;
    
    // Convert to -1.0 to 1.0 (Invert Y so UP is positive)
    joyX = dx / maxDist;
    joyY = -(dy / maxDist);
}

function stopJoystick() {
    joyActive = false;
    joyX = 0;
    joyY = 0;
    stick.style.transition = 'transform 0.1s ease-out';
    stick.style.transform = `translate(0px, 0px)`;
    if(ws && ws.readyState === WebSocket.OPEN) ws.send("T 0 0");
    if(sendInterval) {
        clearInterval(sendInterval);
        sendInterval = null;
    }
}

function startSending() {
    if(sendInterval) clearInterval(sendInterval);
    sendInterval = setInterval(() => {
        if(ws && ws.readyState === WebSocket.OPEN) {
            // Skid-steer mixing
            let left = joyY + joyX;
            let right = joyY - joyX;
            
            // Normalize to -1.0 .. 1.0
            let max = Math.max(Math.abs(left), Math.abs(right));
            if (max > 1.0) {
                left /= max;
                right /= max;
            }
            
            // Map to -1000 .. 1000
            let leftPWM = Math.round(left * 1000);
            let rightPWM = Math.round(right * 1000);
            
            // Format: "T <left> <right>"
            ws.send(`T ${leftPWM} ${rightPWM}`);
        }
    }, 100);
}

// Mouse Events
base.addEventListener('mousedown', (e) => {
    joyActive = true;
    updateJoystick(e.clientX, e.clientY);
    startSending();
});
window.addEventListener('mousemove', (e) => {
    if(joyActive) updateJoystick(e.clientX, e.clientY);
});
window.addEventListener('mouseup', () => {
    if(joyActive) stopJoystick();
});

// Touch Events
base.addEventListener('touchstart', (e) => {
    e.preventDefault();
    joyActive = true;
    updateJoystick(e.touches[0].clientX, e.touches[0].clientY);
    startSending();
}, {passive: false});

window.addEventListener('touchmove', (e) => {
    if(joyActive) {
        e.preventDefault();
        updateJoystick(e.touches[0].clientX, e.touches[0].clientY);
    }
}, {passive: false});

window.addEventListener('touchend', () => {
    if(joyActive) stopJoystick();
});

// Start WS
connectWS();

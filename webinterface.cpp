#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

// const char* WiFiSetupPage = R"rawliteral(
//     <!DOCTYPE html>
//     <html lang="en">
//     <head>
//         <meta charset="UTF-8">
//         <meta name="viewport" content="width=device-width, initial-scale=1.0">
//         <title>Machine %MACHINE_ID% - WiFi Setup</title>
//         <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
//         <style>
//             /* Reuse your existing :root variables and styles from other pages */
//             :root {
//                 --bg-gradient: linear-gradient(135deg, #f8fafc 0%, #e9ecef 100%);
//                 --card-bg: #ffffff;
//                 --primary: #0f172a;
//                 --accent: #3b82f6;
//                 --danger: #ef4444;
//                 --text: #1e293b;
//                 --text-secondary: #64748b;
//                 --shadow-lg: 0 20px 25px -5px rgba(0,0,0,0.15);
//                 --radius-lg: 24px;
//             }
//             body {
//                 font-family: system-ui, sans-serif;
//                 background: var(--bg-gradient);
//                 min-height: 100vh;
//                 margin: 0;
//                 display: flex;
//                 align-items: center;
//                 justify-content: center;
//                 padding: 1rem;
//                 color: var(--text);
//             }
//             .container {
//                 background: var(--card-bg);
//                 border-radius: var(--radius-lg);
//                 box-shadow: var(--shadow-lg);
//                 padding: 2rem;
//                 width: 100%;
//                 max-width: 460px;
//             }
//             h1 { text-align: center; color: var(--primary); margin-bottom: 1.5rem; }
//             label { display: block; margin: 1rem 0 0.4rem; font-weight: 600; }
//             select, input[type="text"], input[type="password"] {
//                 width: 100%;
//                 padding: 0.8rem;
//                 border: 1px solid #d1d5db;
//                 border-radius: 8px;
//                 font-size: 1rem;
//             }
//             button {
//                 width: 100%;
//                 padding: 1rem;
//                 margin-top: 1.5rem;
//                 background: var(--accent);
//                 color: white;
//                 border: none;
//                 border-radius: 8px;
//                 font-size: 1rem;
//                 cursor: pointer;
//             }
//             button:hover { background: #2563eb; }
//             #status { margin-top: 1rem; text-align: center; min-height: 1.3em; }
//             .error { color: var(--danger); }
//         </style>
//     </head>
//     <body>
//         <div class="container">
//             <h1><i class="fas fa-wifi"></i> WiFi Setup</h1>
//             <p style="text-align:center; color:var(--text-secondary);">Machine %MACHINE_ID%</p>

//             <form id="wifiForm">
//                 <label for="ssid">Network (SSID)</label>
//                 <select id="ssid" name="ssid" required>
//                     <option value="">Select network...</option>
//                     %WIFI_LIST%
//                 </select>

//                 <label for="pass">Password</label>
//                 <input type="password" id="pass" name="pass" placeholder="Enter password" required>

//                 <button type="submit">Connect</button>
//             </form>

//             <div id="status"></div>
//         </div>

//         <script>
//             const form = document.getElementById('wifiForm');
//             const status = document.getElementById('status');

//             form.onsubmit = async (e) => {
//                 e.preventDefault();
//                 status.textContent = 'Connecting...';
//                 status.className = '';

//                 const ssid = document.getElementById('ssid').value;
//                 const pass = document.getElementById('pass').value;

//                 try {
//                     const res = await fetch('/save_wifi', {
//                         method: 'POST',
//                         headers: {'Content-Type': 'application/x-www-form-urlencoded'},
//                         body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
//                     });

//                     if (res.ok) {
//                         status.textContent = 'Success! Rebooting...';
//                         setTimeout(() => location.href = '/', 3000);
//                     } else {
//                         status.textContent = 'Failed – wrong password or network issue';
//                         status.className = 'error';
//                     }
//                 } catch (err) {
//                     status.textContent = 'Connection error';
//                     status.className = 'error';
//                 }
//             };
//         </script>
//     </body>
//     </html>
// )rawliteral";

const char* loginIndex = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>NsighTex – Machine %MACHINE_ID%</title>
            <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
            <style>
                :root {
                    --bg-gradient: linear-gradient(135deg, #f8fafc 0%, #e9ecef 100%);
                    --card-bg: #ffffff;
                    --header-bg: linear-gradient(120deg, #0f172a 0%, #1e293b 100%);
                    --primary: #0f172a;
                    --primary-dark: #1e293b;
                    --accent: #3b82f6;
                    --accent-hover: #2563eb;
                    --success: #10b981;
                    --danger: #ef4444;
                    --warning: #f59e0b;
                    --border: #e2e8f0;
                    --text: #1e293b;
                    --text-secondary: #64748b;
                    --shadow-sm: 0 2px 8px rgba(0,0,0,0.06);
                    --shadow-md: 0 10px 25px -5px rgba(0,0,0,0.1), 0 8px 10px -6px rgba(0,0,0,0.05);
                    --shadow-lg: 0 20px 25px -5px rgba(0,0,0,0.15), 0 10px 10px -5px rgba(0,0,0,0.04);
                    --radius-sm: 8px;
                    --radius-md: 16px;
                    --radius-lg: 24px;
                    --transition: all 0.3s cubic-bezier(0.165, 0.84, 0.44, 1);
                }
                
                * { margin: 0; padding: 0; box-sizing: border-box; }
                
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
                    background: var(--bg-gradient);
                    min-height: 100vh;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    padding: 1.5rem;
                    color: var(--text);
                    line-height: 1.5;
                }
                
                .container {
                    width: 100%;
                    max-width: 460px;
                    background: var(--card-bg);
                    border-radius: var(--radius-lg);
                    box-shadow: var(--shadow-lg);
                    overflow: hidden;
                    opacity: 0;
                    transform: translateY(20px);
                    animation: fadeIn 0.6s ease-out forwards;
                }
                
                @keyframes fadeIn {
                    to { opacity: 1; transform: translateY(0); }
                }
                
                .header {
                    background: var(--header-bg);
                    color: white;
                    padding: 1.5rem;
                    text-align: center;
                    position: relative;
                }
                
                .header::after {
                    content: '';
                    position: absolute;
                    bottom: 0;
                    left: 0;
                    right: 0;
                    height: 4px;
                    background: linear-gradient(90deg, var(--accent), #8b5cf6);
                }
                
                .header h1 {
                    font-size: 1.8rem;
                    font-weight: 800;
                    letter-spacing: -0.5px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    gap: 10px;
                }
                
                .header h1 i { font-size: 1.5rem; }
                .header span { 
                    display: block; 
                    margin-top: 4px; 
                    font-size: 0.95rem; 
                    opacity: 0.9; 
                    font-weight: 500;
                }
                
                .content {
                    padding: 2.25rem;
                }
                
                h2 {
                    text-align: center;
                    font-size: 1.65rem;
                    font-weight: 700;
                    margin-bottom: 1.75rem;
                    color: var(--primary);
                    position: relative;
                }
                
                h2::after {
                    content: '';
                    display: block;
                    width: 50px;
                    height: 3px;
                    background: var(--accent);
                    margin: 0.75rem auto 0;
                    border-radius: 3px;
                }
                
                .form-group {
                    margin-bottom: 1.5rem;
                }
                
                label {
                    display: block;
                    font-weight: 600;
                    margin-bottom: 0.65rem;
                    font-size: 0.95rem;
                    color: var(--text);
                    display: flex;
                    align-items: center;
                    gap: 6px;
                }
                
                label i { color: var(--accent); font-size: 0.9rem; }
                
                input {
                    width: 100%;
                    padding: 0.95rem 1.25rem;
                    border: 1.5px solid var(--border);
                    border-radius: var(--radius-sm);
                    font-size: 1.05rem;
                    transition: var(--transition);
                    background-color: #f8fafc;
                }
                
                input:focus {
                    outline: none;
                    border-color: var(--accent);
                    box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.15);
                    background-color: white;
                }
                
                input::placeholder { color: var(--text-secondary); opacity: 0.7; }
                
                button {
                    width: 100%;
                    padding: 1rem;
                    background: linear-gradient(to right, var(--accent), #2563eb);
                    color: white;
                    border: none;
                    border-radius: var(--radius-sm);
                    font-size: 1rem;
                    font-weight: 600;
                    cursor: pointer;
                    transition: var(--transition);
                    position: relative;
                    overflow: hidden;
                    box-shadow: var(--shadow-sm);
                }
                
                button::after {
                    content: '';
                    position: absolute;
                    top: 0;
                    left: -100%;
                    width: 100%;
                    height: 100%;
                    background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
                    transition: left 0.7s;
                }
                
                button:hover::after { left: 100%; }
                button:hover { transform: translateY(-1px); box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3); }
                button:active { transform: translateY(0); }
                button:disabled {
                    background: #94a3b8;
                    cursor: not-allowed;
                    transform: none;
                    box-shadow: none;
                }
                button:disabled::after { display: none; }
                
                .status {
                    text-align: center;
                    font-weight: 500;
                    margin-top: 1.25rem;
                    min-height: 1.6em;
                    padding: 0.6rem;
                    border-radius: var(--radius-sm);
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    gap: 8px;
                    font-size: 0.95rem;
                }
                
                .status.error {
                    background: rgba(239, 68, 68, 0.1);
                    color: var(--danger);
                    border: 1px solid rgba(239, 68, 68, 0.2);
                }
                .status.info {
                    background: rgba(59, 130, 246, 0.1);
                    color: var(--accent);
                    border: 1px solid rgba(59, 130, 246, 0.2);
                }
                
                .footer {
                    text-align: center;
                    padding: 1.4rem;
                    border-top: 1px solid var(--border);
                    color: var(--text-secondary);
                    font-size: 0.9rem;
                    background-color: #f9fafb;
                    font-weight: 500;
                }
                
                .footer span { display: block; margin-top: 4px; opacity: 0.85; }
                
                @media (max-width: 480px) {
                    .content { padding: 1.75rem; }
                    .header h1 { font-size: 1.65rem; }
                    .header span { font-size: 0.9rem; }
                }
            </style>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1><i class="fas fa-shield-alt"></i> NsighTex</h1>
                    <span>Machine: %MACHINE_ID%</span>
                </div>
                <div class="content">
                    <h2>Secure Access</h2>
                    <form id="loginForm">
                        <div class="form-group">
                            <label for="userid"><i class="fas fa-user"></i> Username</label>
                            <input type="text" id="userid" autocomplete="username" required placeholder="Enter your username">
                        </div>
                        <div class="form-group">
                            <label for="pwd"><i class="fas fa-lock"></i> Password</label>
                            <input type="password" id="pwd" autocomplete="current-password" required placeholder="Enter your password">
                        </div>
                        <button id="loginBtn" type="submit">
                            <i class="fas fa-sign-in-alt"></i> Sign In
                        </button>
                        <div id="status" class="status"></div>
                    </form>
                </div>
                <div class="footer">
                    <div>Embedded Web Interface</div>
                    <span>NsighTex © <span id="year"></span></span>
                </div>
            </div>
            
            <script>
                document.getElementById('year').textContent = new Date().getFullYear();
                
                const form = document.getElementById('loginForm');
                const status = document.getElementById('status');
                const btn = document.getElementById('loginBtn');
                
                form.onsubmit = e => {
                    e.preventDefault();
                    status.textContent = '';
                    status.className = 'status';
                    
                    const userid = document.getElementById('userid').value.trim();
                    const pwd = document.getElementById('pwd').value;
                    
                    if (!userid || !pwd) {
                        status.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Please enter username and password';
                        status.className = 'status error';
                        return;
                    }
                    
                    btn.disabled = true;
                    btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Authenticating...';
                    
                    fetch('/login', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ userid, pwd })
                    })
                    .then(r => {
                        if (!r.ok) throw new Error();
                        return r.text();
                    })
                    .then(token => {
                        localStorage.setItem('sessionToken', token);
                        window.location.href = '/serverIndex?token=' + encodeURIComponent(token);
                    })
                    .catch(() => {
                        status.innerHTML = '<i class="fas fa-times-circle"></i> Invalid credentials. Please try again.';
                        status.className = 'status error';
                        btn.disabled = false;
                        btn.innerHTML = '<i class="fas fa-sign-in-alt"></i> Sign In';
                    });
                };
                
                // Auto-focus username field
                document.getElementById('userid').focus();
            </script>
        </body>
    </html>
)rawliteral";

const char* serverIndex = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>NsighTex – Machine %MACHINE_ID%</title>
            <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
            <style>
                :root {
                    --bg-gradient: linear-gradient(135deg, #f8fafc 0%, #e9ecef 100%);
                    --card-bg: #ffffff;
                    --header-bg: linear-gradient(120deg, #0f172a 0%, #1e293b 100%);
                    --primary: #0f172a;
                    --accent: #3b82f6;
                    --accent-hover: #2563eb;
                    --success: #10b981;
                    --danger: #ef4444;
                    --warning: #f59e0b;
                    --border: #e2e8f0;
                    --text: #1e293b;
                    --text-secondary: #64748b;
                    --shadow-lg: 0 20px 25px -5px rgba(0,0,0,0.15);
                    --radius-lg: 24px;
                    --transition: all 0.4s cubic-bezier(0.165, 0.84, 0.44, 1);
                }
                * { margin:0; padding:0; box-sizing:border-box; }
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                    background: var(--bg-gradient);
                    min-height: 100vh;
                    padding: 1.5rem;
                    color: var(--text);
                }
                .container {
                    max-width: 540px;
                    margin: 0 auto;
                    background: var(--card-bg);
                    border-radius: var(--radius-lg);
                    box-shadow: var(--shadow-lg);
                    overflow: hidden;
                }
                .header {
                    background: var(--header-bg);
                    color: white;
                    padding: 1.6rem;
                    text-align: center;
                    position: relative;
                }
                .header::after {
                    content: '';
                    position: absolute;
                    bottom: 0; left: 0; right: 0;
                    height: 4px;
                    background: linear-gradient(90deg, var(--accent), #60a5fa);
                }
                .header h1 {
                    font-size: 1.9rem;
                    font-weight: 800;
                    margin-bottom: 0.3rem;
                }
                .header .subtitle {
                    font-size: 1rem;
                    opacity: 0.9;
                }
                .status-bar {
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    gap: .6rem;
                    padding: 1.2rem;
                    font-size: 1.45rem;
                    font-weight: 800;
                    letter-spacing: .04em;
                    transition: all .35s ease;
                }
                .status-bar.running {background: rgba(16,185,129,.15); color: var(--success);}
                .status-bar.idle {background: rgba(148,163,184,.18); color: var(--text-secondary);}
                .status-bar.paused {background: rgba(245,158,11,.2);color: var(--warning);}
                .status-bar.stopped {background: rgba(239,68,68,.18);color: var(--danger);}
                .status-bar.running #stateIcon {animation: pulse 1.4s infinite;}
                @keyframes pulse {
                    0% { transform: scale(.85); opacity:.6; }
                    50% { transform: scale(1); opacity:1; }
                    100% { transform: scale(.85); opacity:.6; }
                }
                .timeline {
                    padding: 1.6rem 1.8rem 1.2rem;
                    background: #f9fafb;
                    border-bottom: 1px solid var(--border);
                }
                .timeline-track {
                    position: relative;
                    height: 4px;
                    background: var(--border);
                    margin: 0 8% 1.4rem;
                    border-radius: 4px;
                }
                .timeline-progress {
                    position: absolute;
                    left: 0; top: 0;
                    height: 100%;
                    width: 0%;
                    background: linear-gradient(90deg,var(--accent),#60a5fa);
                    border-radius: 4px;
                    transition: width .6s ease;
                }
                .steps-container {
                    display: flex;
                    justify-content: space-between;
                }
                .step {
                    width: 16%;
                    text-align: center;
                }
                .step-dot {
                    width: 14px;
                    height: 14px;
                    margin: 0 auto;
                    border-radius: 50%;
                    background: #e5e7eb;
                    border: 2px solid var(--border);
                    transition: all .3s ease;
                }
                .step.completed .step-dot {
                    background: var(--success);
                    border-color: var(--success);
                }
                .step.active .step-dot {
                    background: var(--warning);
                    border-color: var(--warning);
                    box-shadow: 0 0 0 6px rgba(245,158,11,.25);
                }
                .step-label {
                    margin-top: .6rem;
                    font-size: .82rem;
                    color: var(--text-secondary);
                    font-weight: 600;
                }
                .timeline.paused .step-dot,
                .timeline.paused .step.active .step-dot,
                .timeline.interrupt .step-dot,
                .timeline.interrupt .step.active .step-dot,
                .timeline.interrupt .step.completed .step-dot {
                    background: var(--danger);
                    border-color: var(--danger);
                    box-shadow: none;
                }
                .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1.3rem; padding: 1.5rem; }
                .big-metric, .metric { background: #f9fafb; border-radius: 16px; padding: 1.5rem; text-align: center; border: 1px solid var(--border); box-shadow: 0 4px 12px rgba(0,0,0,0.04); transition: opacity 0.3s ease; }
                .big-metric { grid-column: 1 / -1; }
                .metric.hidden, .big-metric.hidden {
                    display: none !important;
                }
                .label { font-size:1rem; color: var(--text-secondary); font-weight: 700; margin-bottom: 0.8rem; }
                .value { font-size: 3.1rem; font-weight: 800; line-height: 1; }
                .unit { font-size: 0.9rem; color: var(--text-secondary); margin-top: 0.3rem; }
                .perf-container { height: 50px; background: #f1f5f9; border-radius: 16px; overflow: hidden; position: relative; box-shadow: inset 0 2px 6px rgba(0,0,0,0.08); border: 1px solid var(--border); }
                #perfBar { height: 100%; width: 0%; background: linear-gradient(90deg, var(--accent), var(--accent-hover)); transition: width 1.2s cubic-bezier(0.165, 0.84, 0.44, 1); position: relative; }
                #perfBar::after { content: ''; position: absolute; inset: 0; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.35), transparent); animation: shimmer 2.8s infinite; }
                @keyframes shimmer { 0% { transform: translateX(-100%); } 100% { transform: translateX(200%); } }
                .perf-text { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; font-size: 2.5rem; font-weight: 800; color: white; text-shadow: 0 2px 6px rgba(0,0,0,0.5); z-index: 2; }
                .controls { padding: 1.5rem; display: grid; grid-template-columns: repeat(3, 1fr); gap: 1.1rem; border-top: 1px solid var(--border); }
                button { padding: 1rem; border: none; border-radius: 12px; font-size: 1.05rem; font-weight: 600; cursor: pointer; color: white; transition: var(--transition); }
                button:hover { transform: translateY(-2px); }
                button:disabled { opacity: 0.6; cursor: not-allowed; transform: none !important; }
                button.mauve   { background: linear-gradient(90deg, #4f46e5, #7c3aed); }
                button.amber   { background: linear-gradient(90deg, var(--warning), #d97706); }
                button.danger  { background: linear-gradient(90deg, var(--danger), #dc2626); }
                .footer { padding: 1.3rem 1.6rem; background: #f9fafb; display: flex; justify-content: space-between; align-items: center; font-size: 0.92rem; color: var(--text-secondary); border-top: 1px solid var(--border); }
                .wifi-icon { position: absolute; top: 1.4rem; right: 1.4rem; font-size: 1.8rem; opacity: 0.9; }
                @media (max-width: 560px) { .grid, .controls { grid-template-columns: 1fr; } .perf-text { font-size: 2.6rem; } }
            </style>
        </head>
        <body>
            <div class="wifi-icon" id="wifiIcon"><i class="fas fa-wifi-slash"></i></div>

            <div class="container">
                <div class="header">
                    <h1><i class="fas fa-industry"></i> Ourdissoire</h1>
                    <div class="subtitle">Machine: %MACHINE_ID%</div>
                </div>

                <div id="stateBar" class="status-bar idle">
                    <i id="stateIcon" class="fas fa-circle"></i>
                    <span id="stateText">ATTENTE</span>
                </div>

                <div class="timeline" id="timeline">
                    <div class="timeline-track">
                        <div class="timeline-progress" id="timelineProgress"></div>
                    </div>

                    <div class="steps-container">
                        <div class="step" data-code="103">
                            <div class="step-dot"></div>
                            <div class="step-label">Encantrage</div>
                        </div>
                        <div class="step" data-code="105">
                            <div class="step-dot"></div>
                            <div class="step-label">Nouage et Passage</div>
                        </div>
                        <div class="step" data-code="107">
                            <div class="step-dot"></div>
                            <div class="step-label">Piquage</div>
                        </div>
                        <div class="step" data-code="109">
                            <div class="step-dot"></div>
                            <div class="step-label">Ourdissage</div>
                        </div>
                        <div class="step" data-code="111">
                            <div class="step-dot"></div>
                            <div class="step-label">Ensouplage</div>
                        </div>
                    </div>
                </div>

                <div class="grid">
                    <div class="big-metric" style="padding: 1.2rem 1.5rem;">
                        <div class="label">Performance</div>
                        <div class="perf-container">
                            <div id="perfBar"></div>
                            <div class="perf-text"><span id="performance">0.0</span>%</div>
                        </div>
                    </div>

                    <div class="metric">
                        <div class="label">Beam Length</div>
                        <div class="value" id="currentMeters">0</div>
                        <div class="unit">m</div>
                    </div>

                    <div class="metric">
                        <div class="label">Revolutions</div>
                        <div class="value" id="totalRevs">0</div>
                        <div class="unit">Revs</div>
                    </div>

                    <div class="metric">
                        <div class="label">Linear Speed</div>
                        <div class="value" id="linearSpeed">0.00</div>
                        <div class="unit">m/s</div>
                    </div>

                    <div class="metric">
                        <div class="label">Rotational Speed</div>
                        <div class="value" id="rotSpeed">0.00</div>
                        <div class="unit">RPM</div>
                    </div>
                </div>

                <div class="controls">
                    <button class="mauve" onclick="goToConfig()"><i class="fas fa-cog"></i> Settings</button>
                    <button class="amber" id="resetBtn"><i class="fas fa-undo"></i> Reset</button>
                    <button class="danger" id="rebootBtn"><i class="fas fa-power-off"></i> Reboot</button>
                </div>

                <div class="footer">
                    <div>Uptime: <span id="uptime">0m 0s</span></div>
                    <div>IP: <span id="ipAddr">-</span></div>
                    <button onclick="logout()" style="background:none;border:none;color:var(--text-secondary);cursor:pointer;font-weight:500;">Logout</button>
                </div>
            </div>

            <script>
                let ws = null;
                window.startTime = Date.now();

                function getToken() {
                    const t = localStorage.getItem('sessionToken');
                    if (!t) { 
                        console.warn('⚠️ Session expired');
                        location.href = '/'; 
                        return '';
                    }
                    return t;
                }

                // === STOP CODE LABELS (from your halt_reasons[] array) ===
                function getStopLabel(code) {
                    const labels = {
                        100: "EN MARCHE",
                        101: "ARRET NORMAL",
                        102: "CASSE FIL",
                        113: "PAUSE",
                        114: "FIN PAUSE",
                        115: "REPARATION OPERATEUR",
                        121: "ATTENTE MECANIQUE",
                        122: "FIN REPARATION MECANIQUE",
                        123: "ATTENTE ELECTRIQUE",
                        124: "FIN REPARATION ELECTRIQUE",
                        146: "REPARATION MECANIQUE",
                        147: "REPARATION ELECTRIQUE",
                        default: (c) => `Code ${c}`
                    };
                    return labels[code] || (labels.default ? labels.default(code) : `Code ${code}`);
                }

                // === TIMELINE: Group stages to steps ===
                // First 3 codes (103,106,104) → Step 0 (Encantrage)
                function getStepIndex(stageCode) {
                    if ([103, 106, 104].includes(stageCode)) return 0;  // Encantrage group
                    if (stageCode === 105) return 1;                     // Nouage
                    if (stageCode === 107) return 2;                     // Piquage  
                    if (stageCode === 109) return 3;                     // Ourdissage
                    if ([111, 112].includes(stageCode)) return 4;        // Ensouplage group
                    return -1;
                }

                function connectWS() {
                    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
                    const wsUrl = `${proto}://${location.hostname}:81/`;
                    console.log(`🔌 Connecting to ${wsUrl}`);
                    
                    if (ws) { ws.onclose = null; ws.close(); }
                    
                    ws = new WebSocket(wsUrl);

                    ws.onopen = () => {
                        console.log('✅ WebSocket connected');
                        document.getElementById('wifiIcon').innerHTML = '<i class="fas fa-wifi"></i>';
                    };

                    ws.onmessage = (e) => {
                        try {
                            const d = JSON.parse(e.data);
                            updateDashboard(d);
                        } catch (err) {
                            console.error('❌ WS parse error:', err, 'Raw:', e.data?.substring(0, 150));
                        }
                    };

                    ws.onclose = (e) => {
                        console.warn(`⚠️ WS closed (code ${e.code}), reconnecting in 3s`);
                        document.getElementById('wifiIcon').innerHTML = '<i class="fas fa-wifi-slash"></i>';
                        setTimeout(connectWS, 3000);
                    };

                    ws.onerror = (err) => console.error('❌ WS error:', err);
                }

                function updateDashboard(d) {
                    const stateBar = document.getElementById("stateBar");
                    const stateText = document.getElementById("stateText");
                    const stateIcon = document.getElementById("stateIcon");
                    
                    stateBar.classList.remove("running", "stopped", "idle");
                    const stopCode = d.stopCode ?? 100;
                    
                    if (stopCode === 100) {
                        stateBar.classList.add("running");
                        stateIcon.className = "fas fa-play-circle";
                        stateText.textContent = "EN MARCHE";
                    } else {
                        stateBar.classList.add("stopped");
                        stateIcon.className = "fas fa-circle-stop";
                        // ✅ Always use the JS lookup table
                        stateText.textContent = getStopLabel(stopCode).toUpperCase();
                    }

                    // === TIMELINE ===
                    const prodStep = d.prodStep ?? 0;
                    const steps = document.querySelectorAll(".step");
                    const timelineProgress = document.getElementById("timelineProgress");
                    
                    steps.forEach(s => s.classList.remove("completed", "active"));
                    const idx = getStepIndex(prodStep);
                    
                    if (idx !== -1) {
                        steps.forEach((s, i) => {
                            if (i < idx) s.classList.add("completed");
                            if (i === idx) s.classList.add("active");
                        });
                        const progress = steps.length > 1 ? (idx / (steps.length - 1)) * 100 : 100;
                        timelineProgress.style.width = `${Math.min(progress, 100)}%`;
                    } else {
                        timelineProgress.style.width = "0%";
                    }

                    // === METRIC CARDS VISIBILITY (only during Ourdissage = stage 109) ===
                    const isOurdissage = prodStep === 109;
                    const ourdissageMetrics = ['currentMeters', 'totalRevs', 'linearSpeed', 'rotSpeed'];
                    
                    ourdissageMetrics.forEach(id => {
                        const card = document.getElementById(id)?.closest('.metric');
                        if (card) card.classList.toggle('hidden', !isOurdissage);
                    });

                    // === METRICS VALUES (update always, visibility handled by CSS) ===
                    const setVal = (id, val, formatter = v => v) => {
                        const el = document.getElementById(id);
                        if (el && val !== undefined && val !== null) {
                            el.textContent = formatter(val);
                        }
                    };

                    setVal('currentMeters', d.currentMeters, v => Math.round(v));
                    setVal('totalRevs', d.drumRPM, v => Math.round(v));
                    
                    if (d.linearSpeed !== undefined) {
                        setVal('linearSpeed', d.linearSpeed, v => parseFloat(v).toFixed(2));
                    }
                    if (d.angularSpeed !== undefined) {
                        // Your scaling: ESP32 sends value/100 → display as decimal (e.g., 1234 → 12.34)
                        setVal('rotSpeed', d.angularSpeed, v => parseFloat(v).toFixed(2));
                    }

                    if (d.performance !== undefined) {
                        const perf = Math.min(Math.max(Math.round(d.performance * 10) / 10, 0), 100);
                        setVal('performance', perf);
                        const bar = document.getElementById('perfBar');
                        if (bar) bar.style.width = `${perf}%`;
                    }

                    if (d.wifi !== undefined) {
                        const icon = document.getElementById('wifiIcon');
                        if (icon) {
                            icon.innerHTML = d.wifi 
                                ? '<i class="fas fa-wifi"></i>' 
                                : '<i class="fas fa-wifi-slash"></i>';
                        }
                    }
                }

                // === INITIALIZATION ===
                document.addEventListener('DOMContentLoaded', () => {
                    connectWS();
                    
                    // Uptime timer
                    setInterval(() => {
                        const elapsed = Math.floor((Date.now() - window.startTime) / 1000);
                        const h = Math.floor(elapsed / 3600);
                        const m = Math.floor((elapsed % 3600) / 60);
                        const s = elapsed % 60;
                        const uptimeEl = document.getElementById('uptime');
                        if (uptimeEl) uptimeEl.textContent = (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
                    }, 1000);
                    
                    // IP display
                    const ipEl = document.getElementById('ipAddr');
                    if (ipEl) ipEl.textContent = location.hostname;
                });

                // === RESET BUTTON (let WebSocket handle display updates) ===
                document.getElementById('resetBtn').onclick = async () => {
                    const btn = document.getElementById('resetBtn');
                    const originalHtml = btn.innerHTML;
                    
                    btn.disabled = true;
                    btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Resetting...';

                    try {
                        const response = await fetch('/sendCommand?token=' + encodeURIComponent(getToken()) + '&cmd=R', {
                            method: 'GET',
                            headers: { 'Cache-Control': 'no-cache' }
                        });
                        if (!response.ok) {
                            console.warn('⚠️ Reset failed:', response.status);
                            alert('Reset command failed');
                        }
                    } catch (err) {
                        console.error('❌ Reset error:', err);
                        alert('Network error');
                    } finally {
                        setTimeout(() => {
                            btn.disabled = false;
                            btn.innerHTML = originalHtml;
                        }, 2000);
                    }
                };

                // === REBOOT BUTTON ===
                document.getElementById('rebootBtn').onclick = () => {
                    if (confirm('Reboot the system now?')) {
                        fetch('/reboot?token=' + encodeURIComponent(getToken()), {method: 'POST'})
                            .then(() => {
                                alert('Reboot sent — page will refresh in ~15s');
                                setTimeout(() => location.reload(), 15000);
                            })
                            .catch(err => {
                                console.error('Reboot failed:', err);
                                alert('Failed to send reboot');
                            });
                    }
                };

                function goToConfig() {
                    location.href = '/config?token=' + encodeURIComponent(getToken());
                }

                function logout() {
                    localStorage.clear();
                    location.href = '/';
                }
            </script>
        </body>
    </html>
)rawliteral";

const char* configPage = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>NsighTex – Machine %MACHINE_ID%</title>
            <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
            <style>
                :root {
                    --bg-gradient: linear-gradient(135deg, #f8fafc 0%, #e9ecef 100%);
                    --card-bg: #ffffff;
                    --header-bg: linear-gradient(120deg, #0f172a 0%, #1e293b 100%);
                    --primary: #0f172a;
                    --accent: #3b82f6;
                    --accent-hover: #2563eb;
                    --success: #10b981;
                    --danger: #ef4444;
                    --warning: #f59e0b;
                    --border: #e2e8f0;
                    --text: #1e293b;
                    --text-secondary: #64748b;
                    --shadow-lg: 0 20px 25px -5px rgba(0,0,0,0.15);
                    --shadow-md: 0 10px 25px -5px rgba(0,0,0,0.1);
                    --radius-lg: 24px;
                    --radius-md: 16px;
                    --transition: all 0.3s cubic-bezier(0.165, 0.84, 0.44, 1);
                }
                * { margin:0; padding:0; box-sizing:border-box; }
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                    background: var(--bg-gradient);
                    min-height: 100vh;
                    padding: 1.5rem;
                    color: var(--text);
                }
                .container {
                    max-width: 520px;
                    margin: 0 auto;
                    background: var(--card-bg);
                    border-radius: var(--radius-lg);
                    box-shadow: var(--shadow-lg);
                    overflow: hidden;
                }
                .header {
                    background: var(--header-bg);
                    color: white;
                    padding: 1.6rem;
                    text-align: center;
                    position: relative;
                }
                .header::after {
                    content: '';
                    position: absolute;
                    bottom: 0; left: 0; right: 0;
                    height: 4px;
                    background: linear-gradient(90deg, var(--accent), #60a5fa);
                }
                .header h1 {
                    font-size: 1.85rem;
                    font-weight: 800;
                    margin-bottom: 0.3rem;
                }
                .header .subtitle {
                    font-size: 1rem;
                    opacity: 0.9;
                }
                .section {
                    padding: 2rem 1.8rem;
                }
                .card {
                    background: #f9fafb;
                    border-radius: var(--radius-md);
                    padding: 1.6rem;
                    margin-bottom: 1.6rem;
                    border: 1px solid var(--border);
                    box-shadow: var(--shadow-md);
                }
                h2 {
                    font-size: 1.4rem;
                    font-weight: 700;
                    color: var(--primary);
                    margin-bottom: 1.4rem;
                    display: flex;
                    align-items: center;
                    gap: 10px;
                }
                .form-group {
                    margin-bottom: 1.5rem;
                }
                label {
                    display: block;
                    font-weight: 600;
                    margin-bottom: 0.6rem;
                    font-size: 0.98rem;
                    color: var(--text);
                }
                .hint {
                    font-size: 0.85rem;
                    color: var(--text-secondary);
                    margin-top: 0.3rem;
                    font-style: italic;
                }
                input {
                    width: 100%;
                    padding: 0.9rem 1.2rem;
                    border: 1.5px solid var(--border);
                    border-radius: 10px;
                    font-size: 1.05rem;
                    background: white;
                    transition: var(--transition);
                }
                input:focus {
                    outline: none;
                    border-color: var(--accent);
                    box-shadow: 0 0 0 3px rgba(59,130,246,0.15);
                }
                input[type="file"] {
                    padding: 0.8rem;
                    border: 2px dashed var(--border);
                    background: #f1f5f9;
                    cursor: pointer;
                }
                input[type="file"]:hover {
                    border-color: var(--accent);
                }
                button {
                    width: 100%;
                    padding: 1.05rem;
                    border: none;
                    border-radius: 10px;
                    font-size: 1.05rem;
                    font-weight: 600;
                    cursor: pointer;
                    transition: var(--transition);
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    gap: 10px;
                }
                button.primary {
                    background: linear-gradient(90deg, var(--accent), var(--accent-hover));
                    color: white;
                    box-shadow: 0 4px 12px rgba(59,130,246,0.25);
                }
                button.primary:hover {
                    transform: translateY(-2px);
                    box-shadow: 0 8px 20px rgba(59,130,246,0.35);
                }
                button.secondary {
                    background: linear-gradient(90deg, #64748b, #475569);
                    color: white;
                }
                button.danger {
                    background: linear-gradient(90deg, var(--danger), #dc2626);
                    color: white;
                }
                .footer {
                    padding: 1.5rem;
                    border-top: 1px solid var(--border);
                    display: flex;
                    gap: 1rem;
                    background: #f9fafb;
                }
                .footer button {
                    flex: 1;
                }
                .progress-container {
                    margin: 1.8rem 0;
                }
                .progress {
                    height: 38px;
                    background: #f1f5f9;
                    border-radius: 999px;
                    overflow: hidden;
                    position: relative;
                    box-shadow: inset 0 2px 6px rgba(0,0,0,0.08);
                    border: 1px solid var(--border);
                }
                #bar {
                    height: 100%;
                    width: 0%;
                    background: linear-gradient(90deg, var(--accent), var(--accent-hover));
                    transition: width 0.5s ease;
                    position: relative;
                }
                #bar::after {
                    content: '';
                    position: absolute;
                    inset: 0;
                    background: linear-gradient(90deg, transparent, rgba(255,255,255,0.35), transparent);
                    animation: shimmer 2.5s infinite;
                }
                @keyframes shimmer {
                    0% { transform: translateX(-100%); }
                    100% { transform: translateX(200%); }
                }
                #txt {
                    position: absolute;
                    inset: 0;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    font-size: 1.35rem;
                    font-weight: 800;
                    color: white;
                    text-shadow: 0 1px 4px rgba(0,0,0,0.6);
                }
                .status {
                    text-align: center;
                    font-weight: 600;
                    margin-top: 1rem;
                    padding: 0.9rem;
                    border-radius: 10px;
                    font-size: 1rem;
                }
                .status.ready { background: rgba(16,185,129,0.12); color: var(--success); border: 1px solid rgba(16,185,129,0.3); }
                .status.error  { background: rgba(239,68,68,0.12); color: var(--danger); border: 1px solid rgba(239,68,68,0.3); }
                @media (max-width: 520px) {
                    .section { padding: 1.6rem 1.4rem; }
                    .card { padding: 1.4rem; }
                    .footer { flex-direction: column; gap: 1rem; }
                }
            </style>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1><i class="fas fa-cog"></i> Configuration</h1>
                    <div class="subtitle">Machine: %MACHINE_ID%</div>
                </div>

                <div class="section">
                    <div class="card">
                        <h2><i class="fas fa-industry"></i> Machine Settings</h2>
                        <form id="configForm">
                            <div class="form-group">
                                <label for="api_url"><i class="fas fa-cloud"></i> API Base URL</label>
                                <input type="text" id="api_url" value="%API_URL%" required placeholder="https://example.com/api/">
                            </div>
                            <div class="form-group">
                                <label for="machine_id"><i class="fas fa-hashtag"></i> Machine ID (0–255)</label>
                                <input type="number" id="machine_id" value="%MACHINE_ID%" min="0" max="255" required>
                            </div>
                            <div class="form-group">
                                <label for="circumference"><i class="fas fa-ruler"></i> Drum Circumference (meters)</label>
                                <input type="number" id="circumference" value="%CIRCUMFERENCE%" min="0.001" max="10.0" step="0.001" required>
                                <div class="hint">Used to calculate RPM from linear speed. Default: 3.125 m</div>
                            </div>
                            <button type="submit" class="primary"><i class="fas fa-save"></i> Save Configuration</button>
                        </form>
                    </div>

                    <div class="card">
                        <h2><i class="fas fa-upload"></i> Firmware Update (OTA)</h2>
                        <div class="form-group">
                            <label for="update"><i class="fas fa-file-archive"></i> Select .bin file</label>
                            <input type="file" id="update" accept=".bin" required>
                        </div>
                        <button id="uploadBtn" class="primary"><i class="fas fa-cloud-upload-alt"></i> Upload & Install</button>

                        <div class="progress-container">
                            <div class="progress">
                                <div id="bar"></div>
                                <div id="txt">Ready</div>
                            </div>
                        </div>

                        <div id="otaStatus" class="status ready">
                            <i class="fas fa-check-circle"></i> Ready to receive firmware
                        </div>
                    </div>
                </div>

                <div class="footer">
                    <button class="secondary" onclick="location.href='/serverIndex?token='+encodeURIComponent(localStorage.getItem('sessionToken')||'')">
                        <i class="fas fa-arrow-left"></i> Back to Dashboard
                    </button>
                    <button class="danger" onclick="localStorage.clear(); location.href='/'">
                        <i class="fas fa-sign-out-alt"></i> Logout
                    </button>
                </div>
            </div>

            <script src="https://cdnjs.cloudflare.com/ajax/libs/crypto-js/4.1.1/crypto-js.min.js"></script>
            <script>
                let otaReady = true;

                function getToken() {
                    const t = localStorage.getItem('sessionToken');
                    if (!t) { alert('Session expired'); location.href = '/'; }
                    return t;
                }

                // Save Configuration
                document.getElementById('configForm').onsubmit = async (e) => {
                    e.preventDefault();
                    const payload = {
                        api_url: document.getElementById('api_url').value.trim(),
                        machine_id: parseInt(document.getElementById('machine_id').value),
                        circumference: parseFloat(document.getElementById('circumference').value)
                    };

                    try {
                        const res = await fetch('/saveConfig?token=' + encodeURIComponent(getToken()), {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify(payload)
                        });
                        if (res.ok) {
                            alert('Configuration saved successfully!');
                            location.href = '/serverIndex?token=' + encodeURIComponent(getToken());
                        } else {
                            alert('Save failed: ' + (await res.text()));
                        }
                    } catch (err) {
                        alert('Error: ' + err.message);
                    }
                };

                // Firmware Upload
                document.getElementById('uploadBtn').onclick = async () => {
                    if (!otaReady) return alert('Upload in progress – please wait');

                    const fileInput = document.getElementById('update');
                    const file = fileInput.files[0];
                    if (!file) return alert('Please select a .bin file');
                    if (file.size > 4 * 1024 * 1024) return alert('File too large (max 4MB)');

                    otaReady = false;
                    const token = getToken();

                    const bar = document.getElementById('bar');
                    const txt = document.getElementById('txt');
                    const status = document.getElementById('otaStatus');
                    const btn = document.getElementById('uploadBtn');

                    bar.style.width = '0%';
                    txt.textContent = '0%';
                    status.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Uploading...';
                    status.className = 'status';
                    btn.disabled = true;

                    const xhr = new XMLHttpRequest();
                    xhr.open('POST', '/update?token=' + encodeURIComponent(token));

                    xhr.upload.onprogress = (ev) => {
                        if (ev.lengthComputable) {
                            const percent = Math.round((ev.loaded / ev.total) * 100);
                            bar.style.width = percent + '%';
                            txt.textContent = percent + '%';
                        }
                    };

                    xhr.onload = () => {
                        otaReady = true;
                        btn.disabled = false;

                        try {
                            const res = JSON.parse(xhr.responseText);
                            if (res.success) {
                                bar.style.background = 'linear-gradient(90deg, var(--success), #0da27e)';
                                txt.textContent = 'Done!';
                                status.innerHTML = '<i class="fas fa-check-circle"></i> Update successful – rebooting...';
                                status.className = 'status ready';
                                setTimeout(() => location.href = '/serverIndex?token=' + encodeURIComponent(token), 4000);
                            } else {
                                bar.style.background = 'linear-gradient(90deg, var(--danger), #c81e1e)';
                                status.innerHTML = '<i class="fas fa-exclamation-triangle"></i> ' + (res.message || 'Update failed');
                                status.className = 'status error';
                            }
                        } catch (e) {
                            // Likely reboot started → success
                            bar.style.background = 'linear-gradient(90deg, var(--success), #0da27e)';
                            txt.textContent = 'Done!';
                            status.innerHTML = '<i class="fas fa-check-circle"></i> Update started – page will reload soon';
                            status.className = 'status ready';
                            setTimeout(() => location.reload(), 5000);
                        }
                    };

                    xhr.onerror = () => {
                        otaReady = true;
                        btn.disabled = false;
                        status.innerHTML = '<i class="fas fa-times-circle"></i> Network error – try again';
                        status.className = 'status error';
                    };

                    const formData = new FormData();
                    formData.append('update', file);
                    xhr.send(formData);
                };
            </script>
        </body>
    </html>
)rawliteral";

#endif

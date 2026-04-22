#include <HardwareSerial.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebSocketsServer.h>
#include <queue>
#include <cstring>
#include <webinterface.h>

// Hardware Pin Definitions
constexpr int ESP32_RXD0 = 44;             // GPIO44 -> ATmega328 TX
constexpr int ESP32_TXD0 = 43;             // GPIO43 -> ATmega328 RX
constexpr int CYD_RXD = 10;                // Screen RX
constexpr int CYD_TXD = 9;                 // Screen TX
constexpr int I2C_SDA = 13;                // screen I2C SDA
constexpr int I2C_SCL = 14;                // screen I2C SCL
constexpr uint8_t SCREEN_I2C_ADDR = 0x08;  // screen address

volatile bool powerFailDetected = false;                   // to detect powerfail
String API_BASE_URL = "http://192.168.1.248:7272/DBLOG/";  // Default API URL
uint8_t MACHINE_ID = 98;
String host;
String sessionToken = "";
constexpr int SwichPowerUpTime = 60000UL;
unsigned long restartTime = 0;
bool webServerStarted = false;
bool updating = false;
bool portalRequested = false;  // ← Flag for screen "Portal" command

bool useMockoon = true;
String MOCKOON_BASE_URL = "http://192.168.1.122:7272/";

char screenBuffer[64];
size_t screenBufPos = 0;
constexpr int BUFFER_SIZE = 16;
char commandBuffer[BUFFER_SIZE];

std::queue<String> screenCommandQueue;
std::queue<String> pendingWebRequests;

enum class RxState {
  IDLE,
  WAITING_FOR_START,
  RECEIVING_JSON
};
typedef enum {
  ENCANTRAGE,
  ENCANTRAGE_PARTIEL,
  FIN_ENCANTRAGE,
  NOUAGE,
  PIQUAGE,
  OURDISSAGE,
  ENSOUPLAGE,
  FIN_ENSOUPLAGE
} ProductionStages;
// Global State
struct MachineState {
  float performance = 0.0;
  bool onRepaire = true;
  bool wifiConnected = true;
  uint8_t stopCode = 100;
  uint8_t previousStopCode = 0;
  uint8_t stageCode = 0;
  unsigned long powerfailTimestamp = 0;
  int beamLength = 1212;
  uint8_t windSection = 1;
  float circumferance = 3.125;
  int currentMeters = 0;
  int drumRevs = 0;
  float linearSpeed = 0.0f;
  float angularSpeed = 0.0f;
  bool wifiSetupDone = false;
  bool triggerPortal = false;
  bool initialStateLoaded = false;
};
// HTTP Request State
struct HttpRequestState {
  bool active = false;
  unsigned long startTime = 0;
  int retryCount = 0;
  int MAX_RETRIES = 3;
  int HTTP_TIMEOUT_MS = 6000;

  bool isFetchMachineData = false;
  unsigned long lastFetchTime = 0;

  HTTPClient* http = nullptr;
  // HTTPClient http;
  String jsonPayload;
  char url[128] = { 0 };
};
// HTTP Request queu
struct PendingWebRequest {
  char operatorID[16] = { 0 };
  uint8_t machineID = 0;
  int haltCode = 0;
  int16_t position = 0;
  uint16_t revolutions = 0;
  unsigned long timestamp = 0;  // when it was queued
};

unsigned long lastScreenSendTime = 0;
const unsigned long MIN_SCREEN_SEND_INTERVAL = 20;

WiFiManager wm;
MachineState state;
WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;
HttpRequestState httpState;
static RxState rxState = RxState::IDLE;
ProductionStages currentStage = ENCANTRAGE;

// USART instances
HardwareSerial atmegaSerial(0);  // UART0 for ATmega328
HardwareSerial screenSerial(2);  // UART2 for screen
// ====== serial functions  sensors <=> main <=> screen display ============
void serialTask(void* parameter);
void handleScreenData();
void processScreenData(const char* data);
void handleAtmegaData();
void processAtmegaCommand(const char* cmd);
void sendCommand(char cmd, int value);
void UpdateScreen(char prefix, int value);
// ====== interface web server ========================
void sendWebRequest(const char* operatorID, int haltCode, int16_t value);
void fetchMachineData();
void queueWebRequest(const char* operatorID, int haltCode, int16_t valeur);
void handleWebRequest();
// ======= local web page and local web server =========
void handleWebServer();
String generateSessionToken();
void resetMDNS();
bool isAuthenticated();
void sendDashboardUpdate(bool force);
// ======= wifi provisionning =========================
void setupWiFi();
// ======= mescilinous functions ======================
void scheduleRestart();
void i2cScanner();
bool isApiReachable();
void syncState();

// ====================================================

void setup(void) {
  Serial.begin(115200);
  atmegaSerial.begin(38400, SERIAL_8N1, ESP32_RXD0, ESP32_TXD0);
  screenSerial.begin(115200, SERIAL_8N1, CYD_RXD, CYD_TXD);

  pinMode(39, OUTPUT);
  digitalWrite(39, LOW);

  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  Wire.setClock(100000);
  delay(1000);

  i2cScanner();

  // === Load / Initialize configuration from Preferences ===
  {
    preferences.begin("app-config", false);  // false = read/write mode (required for writing)
    bool needsWrite = false;
    if (!preferences.isKey("machine_id")) {
      preferences.putUChar("machine_id", MACHINE_ID);
      Serial.println("NVS was empty → set default Machine ID = 99");
      needsWrite = true;
    }
    if (!preferences.isKey("api_url") || preferences.getString("api_url").length() == 0) {
      preferences.putString("api_url", API_BASE_URL);
      Serial.println("NVS was empty → set default API URL");
      needsWrite = true;
    }
    if (!preferences.isKey("session_token") || preferences.getString("session_token").length() == 0) {
      sessionToken = "";
      preferences.putString("session_token", sessionToken);
      Serial.println("NVS was empty → cleared session token");
      needsWrite = true;
    }

    MACHINE_ID = preferences.getUChar("machine_id", 99);
    API_BASE_URL = preferences.getString("api_url", API_BASE_URL);
    sessionToken = preferences.getString("session_token", "");
    preferences.end();
  }
  // create tasks for serial
  xTaskCreatePinnedToCore(serialTask, "SerialTask", 16384, NULL, 1, NULL, 1);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  // check if there is a real power failure
  {
    for (int attempt = 0; attempt < 3 && !powerFailDetected; attempt++) {
      atmegaSerial.println("S1");
      Serial.printf(" Sent 'S1' to ATmega (attempt %d)\n", attempt + 1);

      unsigned long timeout = millis() + 800;
      while (millis() < timeout && !powerFailDetected) {
        handleAtmegaData();                  // ← Process UART buffer FIRST
        vTaskDelay(1 / portTICK_PERIOD_MS);  // ← Then yield (1ms is enough)
      }

      if (powerFailDetected) {
        Serial.printf(" Received 'F1' on attempt %d → Power fail confirmed!\n", attempt + 1);
        state.powerfailTimestamp = millis();
        delay(SwichPowerUpTime);  // delay only on real power fail
      }
    }
    if (!powerFailDetected) {
      Serial.printf(" No F1 after 3 attempts → warm restart (no power fail)\n", millis() / 1000.0);
    }
  }
  // check if there are an saved wifi credentials and attempt to reconnect
  {
    host = "machine" + String(MACHINE_ID);
    WiFi.onEvent(WiFiEvent);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setHostname(host.c_str());

    String apName = "Machine" + String(MACHINE_ID);
    String apPass = "Nsight1234";

    wm.setConfigPortalTimeout(180);  // 3 minutes if portal opens
    wm.setConnectTimeout(20);
    wm.setCleanConnect(true);
    wm.setShowInfoErase(false);
    wm.setShowInfoUpdate(false);

    // === Main logic: let WiFiManager decide ===
    Serial.println(F("[WiFi] Starting autoConnect..."));
    bool connected = wm.autoConnect(apName.c_str(), apPass.c_str());

    if (connected) {
      Serial.printf(F("[WiFi] ✅ Connected! IP: %s\n"), WiFi.localIP().toString().c_str());
      state.wifiConnected = true;
    } else {
      Serial.println(F("[WiFi] ⏳ Could not connect - portal was shown or connection failed. Auto-retry in background via events."));
      state.wifiConnected = false;
    }

    state.wifiSetupDone = true;
  }

  Serial.printf(F("✅ Setup complete - Host: %s.local | WiFi: %s\n"), host.c_str(), state.wifiConnected ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("Loaded configuration - API URL: %s, Machine ID: %u, mDNS Hostname: %s.local\n", API_BASE_URL.c_str(), MACHINE_ID, host.c_str());

  // ==================== WEBSOCKET SETUP ====================
  webSocket.begin();
  webSocket.enableHeartbeat(30000, 3000, 2);
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
      case WStype_CONNECTED:
        Serial.printf("[WS] ✅ Client #%u connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
        sendDashboardUpdate(true);  // Send initial state
        break;
      case WStype_DISCONNECTED:
        Serial.printf("[WS] ❌ Client #%u disconnected\n", num);
        break;
      case WStype_TEXT:
        Serial.printf("[WS] 📩 Client #%u: %.*s\n", num, (int)length, payload);
        break;
    }
  });
}
void loop(void) {
  if (webServerStarted) {
    server.handleClient();  // This is mandatory!
  }
  webSocket.loop();
  if (powerFailDetected && state.wifiConnected && isApiReachable() && !httpState.active) {
    unsigned long downtime = (millis() - state.powerfailTimestamp + SwichPowerUpTime) / 1000;
    sendWebRequest("0", 19, downtime);
    powerFailDetected = false;
  }
  handleWiFiReconnection();
  static unsigned long lastLog = 0;
  if (millis() - lastLog >= 60000) {

    if (state.wifiConnected && !httpState.active) {
      if (!isApiReachable()) {
        Serial.println(F("⚠️ API unreachable - WiFi recovery will handle"));
        Serial.println(F("[Monitor] API unreachable → marking for reconnection"));
        Serial.printf(F("Stale WiFi detected (API unreachable)! RSSI: %d dBm, IP: %s.\n"), WiFi.RSSI(), WiFi.localIP().toString().c_str());
      }
    }

    fetchMachineData();

    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t usedHeap = totalHeap - freeHeap;

    float heapUsedPercent = (float)usedHeap / totalHeap * 100.0f;
    float heapFreePercent = (float)freeHeap / totalHeap * 100.0f;
    float minFreePercent = totalHeap > 0 ? (float)minFreeHeap / totalHeap * 100.0f : 0.0f;

    Serial.printf(F("-Heap → Total: %u  Used: %u  Free: %u  MinEver: %u bytes\n"), totalHeap, usedHeap, freeHeap, minFreeHeap);
    Serial.printf(F("-     →            Used: %.1f%%   Free: %.1f%%   MinEver: %.1f%%\n"), heapUsedPercent, heapFreePercent, minFreePercent);

    lastLog = millis();
  }
  scheduleRestart();                   // your existing function
  vTaskDelay(1 / portTICK_PERIOD_MS);  // yield a bit
}
void serialTask(void* parameter) {
  for (;;) {
    handleScreenData();
    handleAtmegaData();
    sendScreenCommand();
    if (!httpState.active && !pendingWebRequests.empty() && state.wifiConnected) {
      handleWebRequest();
    }
    vTaskDelay(30 / portTICK_PERIOD_MS);  // Yield for 10ms
  }
}

//========== Data from screen ====================
void handleScreenData() {
  while (screenSerial.available()) {
    char c = screenSerial.read();
    switch (rxState) {
      case RxState::IDLE:
      case RxState::WAITING_FOR_START:
        if (c == '<') {
          // Start of a possible command/marker
          String marker = screenSerial.readStringUntil('>');  // still blocking but very rare (only on '<')
          marker.trim();                                      // safety
          if (marker == "START") {
            rxState = RxState::RECEIVING_JSON;
            screenBufPos = 0;
            screenBuffer[0] = '\0';
            Serial.println("Screen: START received");
          } else if (marker == "END" && rxState == RxState::RECEIVING_JSON) {
            rxState = RxState::IDLE;
            Serial.println("Screen: Unexpected END");
          }
        }
        break;
      case RxState::RECEIVING_JSON:
        if (c == '<') {
          // Possible start of END marker — peek ahead without consuming too much
          String marker = screenSerial.readStringUntil('>');
          marker.trim();
          if (marker == "END") {
            screenBuffer[screenBufPos] = '\0';  // null-terminate
            Serial.printf("Received from screen (%d bytes): %s\n", screenBufPos, screenBuffer);
            processScreenData(screenBuffer);
            rxState = RxState::IDLE;
          } else {
            if (screenBufPos < sizeof(screenBuffer) - 2) {
              screenBuffer[screenBufPos++] = '<';
            }
            for (char mc : marker) {
              if (screenBufPos < sizeof(screenBuffer) - 2) {
                screenBuffer[screenBufPos++] = mc;
              }
            }
            if (screenBufPos < sizeof(screenBuffer) - 1) {
              screenBuffer[screenBufPos++] = '>';
            }
          }
        } else if (screenBufPos < sizeof(screenBuffer) - 1) {
          // Normal JSON character
          screenBuffer[screenBufPos++] = c;
        } else {
          // Buffer overflow protection
          Serial.println("Screen RX buffer overflow!");
          rxState = RxState::IDLE;
        }
        break;
    }
  }
}
void processScreenData(const char* data) {
  Serial.printf("RAW from screen: %s\n", data);

  char buffer[256];
  strncpy(buffer, data, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  char* cmd = nullptr;
  char* valueStr = nullptr;
  char* opid = nullptr;
  char* avStr = nullptr;  // Only appears with Stage 109

  char* token = strtok(buffer, ",");
  while (token != nullptr) {
    char* colon = strchr(token, ':');
    if (colon) {
      *colon = '\0';
      char* key = token;
      char* val = colon + 1;

      if (strcmp(key, "cmd") == 0) cmd = val;
      else if (strcmp(key, "value") == 0) valueStr = val;
      else if (strcmp(key, "OpID") == 0) opid = val;
      else if (strcmp(key, "Av") == 0) avStr = val;  // Avancement
    }
    token = strtok(nullptr, ",");
  }

  if (!cmd) return;

  Serial.printf("Parsed → CMD:%s | VALUE:%s | Av:%s | OpID:%s\n", cmd, valueStr ? valueStr : "NULL", avStr ? avStr : "NULL", opid ? opid : "NULL");

  if (strcmp(cmd, "Stage") == 0 && valueStr) {
    uint8_t stageCode = (uint8_t)atoi(valueStr);
    state.stageCode = stageCode;
    sendWebRequest(opid ? opid : "0", stageCode, 0);
    sendCommand('P', state.stageCode);

    // === Handle Avancement when stage = OURDISSAGE (109) ===
    if (stageCode == 109 && avStr != nullptr) {
      float avFloat = atof(avStr);                          // "2.000"
      int32_t avScaled = (int32_t)round(avFloat * 1000.0);  // 2.000 → 2000
      Serial.printf("→ Avancement: %s → %.3f → sending A%d to ATmega\n", avStr, avFloat, avScaled);
      sendCommand('A', avScaled);
      sendWebRequest(opid ? opid : "0", 197, avScaled);
      fetchMachineData();
      sendCommand('L', state.beamLength);
    }
    switch (stageCode) {
      case 103: currentStage = ENCANTRAGE; break;
      case 106: currentStage = ENCANTRAGE_PARTIEL; break;
      case 104: currentStage = FIN_ENCANTRAGE; break;
      case 105: currentStage = NOUAGE; break;
      case 107: currentStage = PIQUAGE; break;
      case 109: currentStage = OURDISSAGE; break;
      case 111: currentStage = ENSOUPLAGE; break;
      case 112: currentStage = FIN_ENSOUPLAGE; break;
    }
    // need to update dashboard here;
    sendDashboardUpdate(true);
  } else if (strcmp(cmd, "Circ") == 0 && valueStr != nullptr) {
    float circFloat = atof(valueStr);                         // "3.125"
    int32_t circScaled = (int32_t)round(circFloat * 1000.0);  // 3.125 → 3125
    Serial.printf("→ Circumference: %s → %.3f → sending C%d to ATmega\n", valueStr, circFloat, circScaled);
    sendCommand('C', circScaled);
    sendWebRequest(opid ? opid : "0", 196, circScaled);
    // need to update dashboard here;
    sendDashboardUpdate(true);
  } else if (strcmp(cmd, "Halt") == 0 && valueStr) {
    state.stopCode = atoi(valueStr);
    sendWebRequest(opid ? opid : "0", state.stopCode, 0);

    if (state.stopCode == 146 || state.stopCode == 147 || state.stopCode == 115) {
      state.onRepaire = true;
      sendCommand('P', state.stopCode);
    } else {
      state.onRepaire = false;
    }
    // need to update dashboard here;
    sendDashboardUpdate(true);
  } else if (strcmp(cmd, "machID") == 0 && valueStr) {
    int id = atoi(valueStr);
    if (id >= 0 && id <= 255) {
      MACHINE_ID = id;
      preferences.begin("app-config", false);
      preferences.putUChar("machine_id", id);
      preferences.end();
      Serial.printf("Machine ID updated to %d\n", id);
    }
  } else if (strcmp(cmd, "Data") == 0) {
    // Refresh screen with current values
    fetchMachineData();
    syncState();
  } else if (strcmp(cmd, "Portal") == 0) {
    Serial.println(F(" 🚪 Portal command received → opening config portal"));
    portalRequested = true;  // ← Set flag, don't block loop
    UpdateScreen('X', 0);
  }
}

//========== Data from the sensors==========================
void handleAtmegaData() {
  static size_t bufferIndex = 0;
  while (atmegaSerial.available()) {
    char c = atmegaSerial.read();
    if (c == '\n' || c == '\r') {
      if (bufferIndex == 0) continue;
      commandBuffer[bufferIndex] = '\0';
      Serial.printf("Atmega → ESP: %s\n", commandBuffer);
      char prefix = commandBuffer[0];
      int value = (bufferIndex > 1) ? atoi(commandBuffer + 1) : 0;
      if (prefix == 'F') {
        powerFailDetected = true;
        sendCommand('P', state.stageCode);
        sendCommand('Y', state.windSection);
        sendCommand('L', state.beamLength);
        Serial.println("Power fail forced!");
      } else if (prefix == 'R' || prefix == 'M' || prefix == 'S' || prefix == 'A') {
        switch (prefix) {
          case 'R':
            state.drumRevs = value;
            break;
          case 'M':
            state.currentMeters = value;
            break;
          case 'S':
            state.linearSpeed = value / 100;
            break;
          case 'A':
            state.angularSpeed = value / 100;
            break;
        }
        sendDashboardUpdate(true);
        UpdateScreen(prefix, value);
      } else {
        processAtmegaCommand(commandBuffer);
      }
      bufferIndex = 0;
    } else if (bufferIndex < BUFFER_SIZE - 1) {
      commandBuffer[bufferIndex++] = c;
    } else {
      Serial.println("Atmega buffer overflow!");
      bufferIndex = 0;
    }
  }
}
void processAtmegaCommand(const char* cmd) {
  if (cmd == nullptr || strlen(cmd) == 0) return;
  char prefix = cmd[0];
  int value = 0;
  if (strlen(cmd) > 1) { value = atoi(cmd + 1); }
  Serial.printf("Processing Atmega command: %c with value %d\n", prefix, value);
  switch (prefix) {
    case 'H':
      if (state.onRepaire) {
      } else {
        state.stopCode = value;
        sendWebRequest("0", state.stopCode, 0);
        sendDashboardUpdate(true);
      }
      Serial.printf("Stop code received: %d\n", value);
      break;
    case 'L':
      sendWebRequest("0", 120, value);
      Serial.printf("chunk Length per minute %d\n", value);
      break;
    case 'Y':
      state.windSection = value;
      sendWebRequest("0", 195, state.windSection);
      break;
    case 'D':
      syncState();
      if (state.stopCode == 146 || state.stopCode == 147 || state.stopCode == 115) {
        sendCommand('P', state.stopCode);
      }
      break;
    default: Serial.printf("Unknown command from Atmega: %s\n", cmd); break;
  }

  // Now send to screen using the new signature
  UpdateScreen(prefix, value);
  // need to update dashboard here;
  sendDashboardUpdate(true);
}
void sendCommand(char cmd, int value) {
  atmegaSerial.print(cmd);
  atmegaSerial.print(value);
  atmegaSerial.print('\n');  // optional
}

//========== Update screen Data display ====================
void UpdateScreen(char prefix, int value) {
  if (prefix == '\0') return;

  // Build the command string like "M1234" or "R0" or "A999"
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "%c%d", prefix, value);

  String commandStr = cmd;

  // Prevent flooding the queue with identical consecutive commands
  if (!screenCommandQueue.empty() && screenCommandQueue.back() == commandStr) {
    return;
  }

  // Limit queue size to avoid memory issues
  if (screenCommandQueue.size() >= 25) {
    screenCommandQueue.pop();
    Serial.println("Warning: Screen command queue full - dropped oldest");
  }

  screenCommandQueue.push(std::move(commandStr));

  // debug
  Serial.printf("Queued for screen: %s\n", cmd);
}
void sendScreenCommand() {
  if (screenCommandQueue.empty()) return;

  // Rate limit: don't spam I2C too fast
  if (millis() - lastScreenSendTime < MIN_SCREEN_SEND_INTERVAL) {
    return;
  }

  String cmd = screenCommandQueue.front();
  screenCommandQueue.pop();

  // Optional: skip duplicate consecutive commands (very useful)
  static String lastSentCmd = "";
  if (cmd == lastSentCmd) {
    // Still pop it but don't send again
    lastSentCmd = cmd;  // update anyway
    lastScreenSendTime = millis();
    return;
  }
  lastSentCmd = cmd;

  // === Send via I2C with proper error checking ===
  Wire.beginTransmission(SCREEN_I2C_ADDR);
  Wire.write((const uint8_t*)cmd.c_str(), cmd.length());
  Wire.write('\n');                            // Your HMI expects this
  uint8_t error = Wire.endTransmission(true);  // true = STOP

  // === DEBUG: See EVERYTHING that is actually sent ===
  Serial.printf("I2C→Screen | '%s' | len=%d | err=%d | queue left=%d\n", cmd.c_str(), cmd.length(), error, screenCommandQueue.size());

  if (error != 0) {
    Serial.printf("  → I2C ERROR %d (2=addr NACK, 3=data NACK, 4=other)\n", error);
    // Optional: push it back to queue for retry (careful not to loop forever)
    screenCommandQueue.push(cmd);
  } else {
    Serial.println("  → SUCCESS");
  }

  lastScreenSendTime = millis();
}

//========== Handle server and coude data exchange =========
void sendWebRequest(const char* operatorID, int haltCode, int16_t value) {
  if (!state.wifiConnected || !isApiReachable()) {
    Serial.println(F("WiFi not connected → queuing request"));
    queueWebRequest(operatorID, haltCode, value);
    setupWiFi();
    return;
  }

  if (httpState.active) {
    Serial.println(F("HTTP busy → queuing request"));
    queueWebRequest(operatorID, haltCode, value);
    return;
  }

  // Ready to send immediately
  if (!httpState.http) {
    httpState.http = new HTTPClient();
  }

  httpState.isFetchMachineData = false;
  httpState.active = true;
  httpState.retryCount = 0;
  httpState.startTime = millis();

  // snprintf(httpState.url, sizeof(httpState.url), "%sRecording_Events_Api.php", API_BASE_URL.c_str());
  String baseUrl = useMockoon ? MOCKOON_BASE_URL : API_BASE_URL;
  if (useMockoon) {
    snprintf(httpState.url, sizeof(httpState.url), "%sPostMachineData", baseUrl.c_str());
  } else {
    snprintf(httpState.url, sizeof(httpState.url), "%sRecording_Events_Api.php", baseUrl.c_str());
  }

  httpState.http->setTimeout(httpState.HTTP_TIMEOUT_MS);
  httpState.http->setConnectTimeout(5000);
  httpState.http->begin(httpState.url);
  httpState.http->addHeader("Content-Type", "application/json");
  httpState.http->addHeader("User-Agent", "Machine" + String(MACHINE_ID));
  httpState.http->addHeader("Connection", "close");

  StaticJsonDocument<300> doc;
  doc["idOperator"] = operatorID ? operatorID : "0";
  doc["idMachine"] = MACHINE_ID;
  doc["codeStop"] = haltCode;
  doc["Valeur"] = value;
  doc["iPAddress"] = WiFi.localIP().toString();

  httpState.jsonPayload.clear();
  serializeJson(doc, httpState.jsonPayload);

  Serial.printf(F("Sending web request: %s\n"), httpState.jsonPayload.c_str());

  handleWebRequest();
}
void fetchMachineData() {
  if (!state.wifiConnected || !isApiReachable()) {
    Serial.println(F("WiFi not connected → skipping fetch"));
    setupWiFi();
    return;
  }
  if (millis() - httpState.lastFetchTime < 5000) {
    return;
  }
  if (httpState.active || !pendingWebRequests.empty()) {
    Serial.printf(F("Skipping fetch (active=%d, queue=%d)\n"), httpState.active, pendingWebRequests.size());
    return;
  }

  httpState.lastFetchTime = millis();

  if (!httpState.http) {
    httpState.http = new HTTPClient();
  }

  httpState.isFetchMachineData = true;
  httpState.active = true;
  httpState.retryCount = 0;

  // snprintf(httpState.url, sizeof(httpState.url), "%sGet_Speed_and_Perfommence.php", API_BASE_URL.c_str());

  String baseUrl = useMockoon ? MOCKOON_BASE_URL : API_BASE_URL;
  if (useMockoon) {
    snprintf(httpState.url, sizeof(httpState.url), "%sgetMachineData", baseUrl.c_str());
  } else {
    snprintf(httpState.url, sizeof(httpState.url), "%sGet_Speed_and_Perfommence.php", baseUrl.c_str());
  }

  httpState.http->setTimeout(httpState.HTTP_TIMEOUT_MS);
  httpState.http->setConnectTimeout(5000);
  httpState.http->begin(httpState.url);
  httpState.http->addHeader("Content-Type", "application/json");
  httpState.http->addHeader("User-Agent", "ESP32-Machine/" + String(MACHINE_ID));
  httpState.http->addHeader("Connection", "close");

  StaticJsonDocument<100> doc;
  doc["idMachine"] = MACHINE_ID;
  httpState.jsonPayload.clear();
  serializeJson(doc, httpState.jsonPayload);

  Serial.printf(F("Fetching machine data: %s\n"), httpState.jsonPayload.c_str());

  handleWebRequest();
}
void handleWebRequest() {
  if (!httpState.http) return;
  const int maxAttempts = httpState.isFetchMachineData ? 1 : httpState.MAX_RETRIES;
  bool success = false;

  for (httpState.retryCount = 1; httpState.retryCount <= maxAttempts; ++httpState.retryCount) {
    httpState.startTime = millis();

    int responseCode = httpState.http->POST(httpState.jsonPayload);
    String payload = httpState.http->getString();

    Serial.printf(F("HTTP %s | Code: %d | Body: %s\n"), httpState.isFetchMachineData ? "FETCH" : "EVENT", responseCode, payload.c_str());

    if (responseCode > 0) {
      // ==================== INSIDE handleWebRequest() ====================
      if (httpState.isFetchMachineData) {
        StaticJsonDocument<400> responseDoc;  // increased size a bit for safety
        DeserializationError error = deserializeJson(responseDoc, payload);

        if (error == DeserializationError::Ok) {

          // === Extract previous state from server ===
          state.stopCode = responseDoc["previousStopCode"].as<int>();  // 100 = running
          state.stageCode = responseDoc["stage"].as<int>();
          state.windSection = responseDoc["section"].as<int>();
          state.beamLength = responseDoc["beamLength"].as<int>();
          state.performance = responseDoc["performance"].as<float>();
          state.circumferance = responseDoc["circumferance"].as<float>();

          Serial.printf("Initial state loaded from server → Stop:%d | Stage:%d | Section:%d | Beam:%d | Perf:%.1f | Cercumferance:%.3f\n", state.stopCode, state.stageCode, state.windSection, state.beamLength, state.performance, state.circumferance);

          UpdateScreen('P', (int)(state.performance * 10));  // if you want performance as P85.0
          if (state.stageCode == 109) {
            sendCommand('L', state.beamLength);
            sendCommand('C', (int)(state.circumferance * 1000));
          }
          // === Send full initial state to CYD screen (only once) ===
          if (!state.initialStateLoaded) {
            syncState();
            // need to update dashboard here;
            sendDashboardUpdate(true);
            state.initialStateLoaded = true;
          }
        } else {
          Serial.printf("JSON parse error: %s\n", error.c_str());
        }
      }
      success = true;
      break;
    } else {
      Serial.printf(F("HTTP Error (Attempt %d/%d): %s\n"), httpState.retryCount, maxAttempts, httpState.http->errorToString(responseCode).c_str());
      if (httpState.retryCount < maxAttempts) {
        delay(1000);
        httpState.http->end();
        httpState.http->begin(httpState.url);
        httpState.http->addHeader("Content-Type", "application/json");
        httpState.http->addHeader("User-Agent", "Machine" + String(MACHINE_ID));
      }
    }
  }

  if (httpState.http) {
    httpState.http->end();  // Release connection, keep object alive
    // DO NOT: delete httpState.http;  ← This causes crashes on reuse
  }

  httpState.active = false;
  httpState.retryCount = 0;

  // === Process next queued JSON payload if any ===
  if (!pendingWebRequests.empty()) {
    String nextJson = pendingWebRequests.front();
    pendingWebRequests.pop();

    Serial.printf(F("Processing queued request | Queue left: %d\n"), pendingWebRequests.size());
    Serial.printf(F("Queued JSON: %s\n"), nextJson.c_str());

    // Reuse the same httpState.jsonPayload for the next request
    httpState.jsonPayload = nextJson;
    httpState.isFetchMachineData = false;  // queued requests are always event type

    // Re-prepare HTTPClient for the next request
    if (!httpState.http) httpState.http = new HTTPClient();
    snprintf(httpState.url, sizeof(httpState.url), "%sRecording_Events_Api.php", API_BASE_URL.c_str());
    httpState.http->setTimeout(httpState.HTTP_TIMEOUT_MS);
    httpState.http->setConnectTimeout(5000);
    httpState.http->begin(httpState.url);
    httpState.http->addHeader("Content-Type", "application/json");
    httpState.http->addHeader("User-Agent", "Machine" + String(MACHINE_ID));
    httpState.http->addHeader("Connection", "close");

    handleWebRequest();  // recursive call to process the next one (safe because active=false now)
  }
}
void queueWebRequest(const char* operatorID, int haltCode, int16_t value) {
  if (pendingWebRequests.size() >= 20) {
    pendingWebRequests.pop();
    Serial.println(F("Request queue full - dropped oldest"));
  }

  StaticJsonDocument<300> doc;
  doc["idOperator"] = operatorID ? operatorID : "0";
  doc["idMachine"] = MACHINE_ID;
  doc["codeStop"] = haltCode;
  doc["Valeur"] = value;
  doc["iPAddress"] = WiFi.localIP().toString();  // even if WiFi is down, we can store "0.0.0.0" or skip

  String jsonStr;
  serializeJson(doc, jsonStr);
  pendingWebRequests.push(jsonStr);

  Serial.printf(F("Request queued | Queue size: %d | HaltCode: %d\n"), pendingWebRequests.size(), haltCode);
}

// ========== WiFi  Event Handler && WIFI provisionning ============
void setupWiFi() {
  static bool wifiSetupInProgress = false;
  if (wifiSetupInProgress) return;
  wifiSetupInProgress = true;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(false);

  String apName = "Machine" + String(MACHINE_ID);
  String apPass = "Nsight1234";  // Portal password

  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(15);
  wm.setConnectRetries(4);
  wm.setSaveConfigCallback([]() {
    Serial.println(F("WiFi credentials saved by user"));
  });

  wm.setShowInfoErase(false);
  wm.setShowInfoUpdate(false);
  wm.setCleanConnect(true);

  bool connected = wm.autoConnect();
  if (connected) {
    Serial.printf(F("[WiFi] ✅ Connected! IP: %s | RSSI: %d\n"), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println(F("[WiFi] ⏳ Connecting in background (event-driven)..."));
    state.wifiConnected = false;
    UpdateScreen('X', 0);
  }

  wifiSetupInProgress = false;
  state.wifiSetupDone = true;
}
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    // === Standard WiFi events (0-25) ===
    case ARDUINO_EVENT_WIFI_READY:
      Serial.println(F("[WiFi] Interface ready"));
      break;
    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println(F("[WiFi] Station started"));
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println(F("[WiFi] Connected to AP"));
      state.wifiConnected = true;
      digitalWrite(39, HIGH);
      UpdateScreen('X', 1);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      {
        wifi_err_reason_t reason = static_cast<wifi_err_reason_t>(info.wifi_sta_disconnected.reason);
        Serial.printf(F("[WiFi] Disconnected. Reason: %d (%s)\n"), static_cast<int>(reason), WiFi.disconnectReasonName(reason));
        state.wifiConnected = false;
        digitalWrite(39, LOW);
        UpdateScreen('X', 0);
        MDNS.end();
        break;
      }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf(F("[WiFi] ✅ Got IP: %s | RSSI: %d\n"), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      resetMDNS();
      if (!webServerStarted) handleWebServer();
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println(F("[WiFi] ⚠️ Lost IP address"));
      state.wifiConnected = false;
      MDNS.end();
      break;

    // === IP-layer events (100-199) ===
    case 100:  // ARDUINO_EVENT_IP_STA_GOT_IP
      Serial.printf(F("[IP] ✅ IPv4 assigned: %s\n"), WiFi.localIP().toString().c_str());
      break;
    case 111:  // ARDUINO_EVENT_IP_AP_STADISCONNECTED
      Serial.println(F("[IP] Client disconnected from AP (IP layer)"));
      break;
    case 130:  // ARDUINO_EVENT_ETH_GOT_IP
      Serial.println(F("[IP] Ethernet got IP (if enabled)"));
      break;

    default:
      Serial.printf(F("[WiFi] ℹ️ Unhandled event: %d\n"), event);
      Serial.printf(F("   Full event info: event_id=%d\n"), event);
      break;
  }
}
void handleWiFiReconnection() {
  // === ONLY handle screen-triggered portal request ===
  if (portalRequested) {
    Serial.println(F("[WiFi] 🚪 Opening config portal (screen request)..."));

    // Stop current WiFi to avoid conflicts
    WiFi.disconnect(true);
    delay(200);

    String apName = "Machine" + String(MACHINE_ID);
    String apPass = "Nsight1234";

    wm.setConfigPortalTimeout(180);                        // 3 min timeout for user to configure
    wm.startConfigPortal(apName.c_str(), apPass.c_str());  // Blocking

    // Reset flag after portal closes
    portalRequested = false;

    // If new credentials were saved, attempt reconnect (no portal fallback)
    if (wm.getWiFiIsSaved()) {
      Serial.println(F("[WiFi] New credentials saved → attempting reconnect..."));
      wm.setConfigPortalTimeout(0);  // Disable portal fallback
      wm.autoConnect(apName.c_str(), apPass.c_str());
    }
  }
}

//=========== Webserver functions =========================
void handleWebServer() {
  randomSeed(millis());
  if (!MDNS.begin(host.c_str())) {
    Serial.println("Error setting up MDNS responder!");
    // while (1) vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  Serial.println("mDNS responder started");
  if (webServerStarted) return;
  webServerStarted = true;
  server.on("/", HTTP_GET, []() {
    sessionToken = "";
    preferences.begin("app-config", false);
    preferences.remove("session_token");
    preferences.end();

    String page = loginIndex;
    page.replace("%MACHINE_ID%", String(MACHINE_ID));
    server.send(200, "text/html", page);
  });
  server.on("/login", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, server.arg("plain"));
      if (error) {
        server.send(400, "text/plain", String("Invalid JSON: ") + error.c_str());
        return;
      }
      String username = doc["userid"] | "";
      String password = doc["pwd"] | "";
      if (username == "Nsight" && password == "Nsight17") {
        sessionToken = generateSessionToken();
        server.send(200, "text/plain", sessionToken);
      } else {
        server.send(401, "text/plain", "Unauthorized: Incorrect credentials");
      }
    } else {
      server.send(400, "text/plain", "No login data provided");
    }
  });
  server.on("/logout", HTTP_POST, []() {
    sessionToken = "";
    preferences.begin("app-config", false);
    preferences.remove("session_token");
    preferences.end();
    resetMDNS();
    server.send(200, "application/json", "{\"success\": true, \"message\": \"Logged out successfully\"}");
  });
  auto requireAuth = []() {
    return isAuthenticated();
  };
  server.on("/serverIndex", HTTP_GET, [requireAuth]() {
    if (!requireAuth()) return;
    String page = serverIndex;
    page.replace("%MACHINE_ID%", String(MACHINE_ID));
    server.send(200, "text/html", page);
  });
  server.on("/config", HTTP_GET, [requireAuth]() {
    if (!requireAuth()) return;
    String page = configPage;
    page.replace("%API_URL%", API_BASE_URL);
    page.replace("%MACHINE_ID%", String(MACHINE_ID));
    page.replace("%CIRCUMFERENCE%", String(state.circumferance, 3));
    server.send(200, "text/html", page);
  });
  server.on("/sendCommand", HTTP_GET, [requireAuth]() {
    if (!requireAuth()) return;

    String cmd = server.arg("cmd");

    if (cmd == "R" || cmd == "r") {
      state.beamLength = 0;
      state.drumRevs = 0;
      state.performance = 0.0;
      UpdateScreen('M', 0);
      sendCommand('R', 1);
      Serial.println(">>> WEB RESET: Sent 'R\\n' to ATmega");
      sendDashboardUpdate(true);
      server.send(200, "text/plain", "OK: Reset sent");
    } else {
      server.send(400, "text/plain", "Invalid command");
    }
  });
  server.on(
    "/saveConfig", HTTP_POST, [requireAuth]() {
      if (!requireAuth()) return;

      if (!server.hasArg("plain") || server.arg("plain").length() == 0) {
        server.send(400, "text/plain", "No data sent");
        return;
      }

      String raw = server.arg("plain");
      Serial.printf("saveConfig payload: %s\n", raw.c_str());

      StaticJsonDocument<400> doc;
      DeserializationError error = deserializeJson(doc, raw);
      if (error) {
        Serial.printf("JSON error: %s\n", error.c_str());
        server.send(400, "text/plain", "Invalid JSON");
        return;
      }

      bool needRestart = false;
      bool mdnsChanged = false;

      // *** CRITICAL: Open Preferences for writing ***
      preferences.begin("app-config", false);  // false = read/write mode

      if (doc.containsKey("api_url")) {
        String url = doc["api_url"].as<String>();
        if (!url.startsWith("http://") && !url.startsWith("https://")) {
          preferences.end();
          server.send(400, "text/plain", "API URL must start with http:// or https://");
          return;
        }
        API_BASE_URL = url;
        preferences.putString("api_url", url);
        Serial.println("Updated API URL");
      }

      if (doc.containsKey("machine_id")) {
        int id = doc["machine_id"].as<int>();
        if (id < 0 || id > 255) {
          preferences.end();
          server.send(400, "text/plain", "Machine ID must be 0-255");
          return;
        }
        MACHINE_ID = id;
        preferences.putUChar("machine_id", (uint8_t)id);
        host = "machine" + String(MACHINE_ID);
        mdnsChanged = true;
        needRestart = true;
        Serial.printf("Updated Machine ID to %d\n", id);
      }
      preferences.end();

      if (doc.containsKey("circumference")) {
        float circ = doc["circumference"].as<float>();
        if (circ > 0.0 && circ <= 50.0) {
          state.circumferance = circ;
          int32_t circScaled = (int32_t)round(circ * 1000.0);  // 3.125 → 3125
          sendCommand('C', circScaled);
          sendWebRequest("0", 196, circScaled);
          Serial.printf("Updated circumference to %.3f m\n", circ);
        } else {
          preferences.end();
          server.send(400, "text/plain", "Circumference must be 0.001–10.0 meters");
          return;
        }
      }
      // Apply mDNS change if needed
      if (mdnsChanged) {
        resetMDNS();
      }
      // Schedule restart if Machine ID changed
      if (needRestart) {
        restartTime = millis() + 3000;
      }

      // Success response
      StaticJsonDocument<200> resp;
      resp["success"] = true;
      resp["message"] = "Configuration saved successfully";
      resp["newMachineId"] = MACHINE_ID;
      resp["ipAddress"] = WiFi.localIP().toString();

      String output;
      serializeJson(resp, output);
      server.send(200, "application/json", output);

      Serial.println("Configuration saved and committed to flash");
    },
    nullptr);
  server.on("/reboot", HTTP_POST, [requireAuth]() {
    if (!requireAuth()) {
      server.send(401, "text/plain", "Unauthorized: Please log in");
      return;
    }
    StaticJsonDocument<200> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Reboot initiated";
    responseDoc["ipAddress"] = WiFi.localIP().toString();
    String response;
    serializeJson(responseDoc, response);
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", response);
    atmegaSerial.println("B1");
    resetMDNS();
    restartTime = millis() + 1000;  // Schedule restart
  });
  server.on(
    "/update", HTTP_POST, [requireAuth]() {
      if (!requireAuth()) {
        server.send(401, "text/plain", "Unauthorized: Please log in");
        return;
      }
      StaticJsonDocument<200> responseDoc;
      responseDoc["success"] = !Update.hasError();
      responseDoc["message"] = Update.hasError() ? "Firmware update failed" : "Firmware update successful";
      responseDoc["ipAddress"] = WiFi.localIP().toString();
      String response;
      serializeJson(responseDoc, response);
      server.sendHeader("Connection", "close");
      server.send(200, "application/json", response);
      updating = false;
      restartTime = millis() + 1000;
    },
    [requireAuth]() {
      if (!requireAuth()) {
        server.send(401, "text/plain", "Unauthorized: Please log in");
        return;
      }
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        updating = true;
        memset(commandBuffer, 0, BUFFER_SIZE);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
          updating = false;
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
          updating = false;
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          preferences.begin("app-config", false);
          preferences.remove("api_url");
          preferences.remove("machine_id");
          preferences.remove("max_distance_mm");
          preferences.remove("strip_sensor_offset");
          preferences.end();
          ESP.restart();
        } else {
          Update.printError(Serial);
          updating = false;
        }
      }
    });
  server.begin();
  Serial.println("Web server started");
}
String generateSessionToken() {
  String token = "";
  const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (int i = 0; i < 16; i++) {
    token += alphanum[random(0, sizeof(alphanum) - 1)];
  }
  preferences.begin("app-config", false);
  preferences.putString("session_token", token);
  preferences.end();
  return token;
}
bool isAuthenticated() {
  if (server.hasArg("token")) {
    String token = server.arg("token");
    if (token == sessionToken && sessionToken != "") {
      return true;
    }
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting to login");
  return false;
}
void resetMDNS() {
  // Clean up any existing instance
  MDNS.end();
  delay(50);  // Brief pause for cleanup

  // Ensure hostname is mDNS-compliant: lowercase, a-z/0-9/- only, <63 chars
  String safeHost = host;
  safeHost.toLowerCase();
  safeHost.replace("_", "-");  // mDNS doesn't allow underscores

  if (safeHost.length() == 0 || safeHost.length() > 63) {
    Serial.printf(F("❌ Invalid hostname for mDNS: '%s'\n"), safeHost.c_str());
    return;
  }

  // Start mDNS responder
  if (MDNS.begin(safeHost.c_str())) {
    Serial.printf(F("✅ mDNS started: %s.local\n"), safeHost.c_str());
    // Add services
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);  // Your WebSocket port
    MDNS.addServiceTxt("http", "tcp", "device", "NsighTex");
    MDNS.addServiceTxt("http", "tcp", "id", String(MACHINE_ID));
    MDNS.setInstanceName(("Machine-" + String(MACHINE_ID)).c_str());

  } else {
    Serial.printf(F("❌ mDNS.begin() FAILED for %s.local\n"), safeHost.c_str());
    Serial.printf(F("   WiFi.status(): %d, IP: %s\n"), WiFi.status(), WiFi.localIP().toString().c_str());
  }
}

// ==================== WEBSOCKET DASHBOARD UPDATE (immediate) ====================
void sendDashboardUpdate(bool force = false) {
  StaticJsonDocument<512> doc;

  // Values expected by serverIndex JavaScript
  doc["stopCode"] = state.stopCode;
  doc["prodStep"] = state.stageCode;
  doc["circumference"] = state.circumferance;


  // Status text for the big bar
  if (state.stopCode == 100) {
    doc["LastCodeLabel"] = "EN MARCHE";
  } else {
    doc["LastCodeLabel"] = "";  // Let JavaScript handle the label mapping
  }

  // Metrics
  doc["performance"] = roundf(state.performance * 10.0f) / 10.0f;
  doc["currentMeters"] = state.currentMeters;
  doc["drumRPM"] = state.drumRevs;
  doc["linearSpeed"] = state.linearSpeed;
  doc["angularSpeed"] = state.angularSpeed;


  doc["wifi"] = state.wifiConnected;

  String jsonStr;
  serializeJson(doc, jsonStr);

  webSocket.broadcastTXT(jsonStr);

  if (force) {
    Serial.printf("[WS] Immediate update → stop:%d stage:%d perf:%.1f beam:%d\n", state.stopCode, state.stageCode, state.performance, state.beamLength);
  }
}

// ========= mescelineouce functions ======================
void scheduleRestart() {
  if (restartTime != 0 && millis() - restartTime >= 5000) {
    ESP.restart();
  }
}
void i2cScanner() {
  Serial.println("\n=== I2C Scanner ===");
  int nDevices = 0;
  for (uint8_t address = 8; address < 120; address++) {  // start from 0x08, skip reserved
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      nDevices++;
    } else if (error == 4) {
      Serial.printf("Unknown error at address 0x%02X\n", address);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.printf("Found %d device(s)\n", nDevices);
  }
  Serial.println("Scan finished.\n");
}
bool isApiReachable() {
  // === Quick fail: WiFi not connected ===
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // === Parse API_BASE_URL to extract host and port ===
  String url = API_BASE_URL;
  url.trim();
  url.toLowerCase();

  // Check scheme
  int schemeEnd = url.indexOf("://");
  if (schemeEnd == -1) {
    Serial.println(F("[API] ❌ Invalid URL format"));
    return false;
  }

  String protocol = url.substring(0, schemeEnd);
  String remainder = url.substring(schemeEnd + 3);  // Skip "://"

  // Extract host:port (stop at first '/')
  int pathStart = remainder.indexOf('/');
  String hostPort = (pathStart != -1) ? remainder.substring(0, pathStart) : remainder;

  // Extract port (default: 80 for http, 443 for https)
  int portStart = hostPort.indexOf(':');
  String targetHost = hostPort;
  int targetPort = (protocol == "https") ? 443 : 80;

  if (portStart != -1) {
    targetHost = hostPort.substring(0, portStart);
    int parsedPort = hostPort.substring(portStart + 1).toInt();
    if (parsedPort > 0 && parsedPort <= 65535) {
      targetPort = parsedPort;
    }
  }

  // === DNS Resolution with Caching (Core 3.3.7: 2-arg hostByName) ===
  static String lastResolvedHost = "";
  static IPAddress lastResolvedIP;
  static unsigned long lastDnsLookup = 0;
  const unsigned long DNS_CACHE_TTL = 60000;  // 60 seconds

  IPAddress targetIP;
  bool dnsOk = false;

  // Check if we can use cached result
  if (targetHost == lastResolvedHost && millis() - lastDnsLookup < DNS_CACHE_TTL) {
    targetIP = lastResolvedIP;
    dnsOk = true;
  } else {
    // ✅ CORRECT FOR CORE 3.3.7: hostByName takes ONLY 2 arguments
    int dnsResult = WiFi.hostByName(targetHost.c_str(), targetIP);

    if (dnsResult == 1) {
      // Cache successful resolution
      lastResolvedHost = targetHost;
      lastResolvedIP = targetIP;
      lastDnsLookup = millis();
      dnsOk = true;
    } else {
      // DNS failed - try fallback: parse as literal IP
      if (targetIP.fromString(targetHost)) {
        dnsOk = true;  // Treat as direct IP address
      }
    }
  }

  // If DNS completely failed, check gateway as network health indicator
  if (!dnsOk) {
    IPAddress gw = WiFi.gatewayIP();
    if (gw != IPAddress(0, 0, 0, 0)) {
      WiFiClient gwClient;
      gwClient.setTimeout(1500);
      if (gwClient.connect(gw, 80)) {
        gwClient.stop();
        return true;  // Gateway reachable = network OK, server may be down
      }
    }
    return false;  // Network issue
  }

  // === Attempt TCP connection to target server ===
  WiFiClient client;
  client.setTimeout(2500);  // 2.5s connection timeout

  bool connected = false;

  // Try connecting by IP first (faster, avoids redundant DNS)
  if (client.connect(targetIP, targetPort)) {
    connected = true;
  }
  // Fallback: try by hostname (for SNI/virtual hosting scenarios)
  else if (!targetHost.equals(targetIP.toString()) && client.connect(targetHost.c_str(), targetPort)) {
    connected = true;
  }

  if (connected) {
    client.stop();
    return true;
  }

  // === Connection failed: Diagnose network vs server issue ===
  IPAddress localIP = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();

  // Check basic network configuration
  if (localIP == IPAddress(0, 0, 0, 0) || gateway == IPAddress(0, 0, 0, 0)) {
    return false;  // Invalid network config
  }

  // Test gateway reachability to distinguish server down vs network down
  WiFiClient gwClient;
  gwClient.setTimeout(1500);
  if (gwClient.connect(gateway, 80)) {
    gwClient.stop();
    // Gateway reachable but server not = server issue (don't trigger WiFi reconnect)
    return true;
  }

  // Gateway unreachable = network issue (WiFi reconnection needed)
  return false;
}
void syncState() {
  UpdateScreen('N', MACHINE_ID);
  UpdateScreen('O', state.stageCode);
  UpdateScreen('X', state.wifiConnected ? 1 : 0);
  UpdateScreen('H', state.stopCode);
  UpdateScreen('Y', state.windSection);
  UpdateScreen('P', (int)(state.performance * 10));
  sendCommand('P', state.stageCode);
  if (state.stageCode == 109) {
    sendCommand('L', state.beamLength);
  }
}
String getStageLabel(uint8_t code) {
  switch (code) {
    case 103: return "Encantage";
    case 106: return "Encantage Partiel";
    case 104: return "Fin Encantage";
    case 105: return "Nouage";
    case 107: return "Piquage";
    case 109: return "Ourdissage";
    case 111: return "Ensouplage";
    case 112: return "Fin Ensouplage";
    default: return "Inconnu";
  }
}

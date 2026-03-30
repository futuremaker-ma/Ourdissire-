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
#include <array>
#include <queue>
#include <cstring>
#include <algorithm>
#include <webinterface.h>

// Hardware Pin Definitions
constexpr int ESP32_RXD0 = 44;             // GPIO44 -> ATmega328 TX
constexpr int ESP32_TXD0 = 43;             // GPIO43 -> ATmega328 RX
constexpr int CYD_RXD = 9;                 // Screen RX
constexpr int CYD_TXD = 10;                // Screen TX
constexpr int I2C_SDA = 14;                // screen I2C SDA
constexpr int I2C_SCL = 21;                // screen I2C SCL
constexpr uint8_t SCREEN_I2C_ADDR = 0x08;  // screen address
constexpr unsigned long I2C_INTERVAL = 20;
#define I2C_DEBUG_LOG 0

// Configuration
String API_BASE_URL = "http://192.168.1.248:7272/DBLOG/";  // Default API URL
uint8_t MACHINE_ID = 99;                                   // Default Machine ID
uint16_t speed = 0;

String host;
String sessionToken = "";

bool updating = false;
bool triggerPortal = true;
constexpr size_t BUFFER_SIZE = 32;
constexpr int HTTP_TIMEOUT_MS = 6000;        // Http request timeout
constexpr int MAX_RETRIES = 3;               // Http requests retries
constexpr int SAFETY_BUFFER_MS = 60000;      // Time laps before connect to wifi
constexpr size_t MAX_PENDING_COMMANDS = 60;  // Limit to prevent memory overflow
constexpr unsigned long interval = 10000;
unsigned long previousMillis = 0;
volatile bool powerFailDetected = false;
volatile bool f1Received = true;
unsigned long lastI2CSentTime = 0;
bool screenReady = false;
bool webServerStarted = false;
bool wifiSetupDone = false;
bool screenRequested = false;


// Global State
struct MachineState {
  uint16_t revolutions = 0;
  float performance = 0.0;
  int lastHaltCode = -1;
  uint8_t haltIssue = 0;
  bool isOperational = true;
  bool wifiConnected = false;
  bool resumeHaltCode = false;
  unsigned long fCommandTimestamp = 0;
  int lastCodeStop = -1;
  String lastCodeLabel = "";
};
// HTTP Request State
struct HttpRequestState {
  bool active = false;
  unsigned long startTime = 0;
  int retryCount = 0;
  // String url;
  char url[128] = { 0 };
  String json;
  HTTPClient* http;
  bool isFetchMachineData = false;
  unsigned long nextRetryTime = 0;
};
// screen commands
struct ScreenCommand {
  char prefix;
  int value;
};
// Global variable to track reconnection state
struct WiFiReconnectState {
  bool reconnecting = false;
};
// === NEW: Ourdissoire live data for instant WebSocket ===
struct OurdissoireLive {
  String machineState = "normal_stop";  // running / normal_stop / chain_stop
  int currentMeters = 0;
  float linearSpeed = 0.0f;
  int drumRPM = 0;
};

OurdissoireLive liveData;
OurdissoireLive lastLiveSent;
// dashboard data for update
struct DashboardData {
  uint16_t revolutions = 0;
  float performance = 0.0f;
  int lastCodeStop = -1;
  String lastCodeLabel = "";
  bool wifiConnected = false;

  bool changed(const DashboardData& other) const {
    return revolutions != other.revolutions || abs(performance - other.performance) > 0.1f || lastCodeStop != other.lastCodeStop || lastCodeLabel != other.lastCodeLabel || wifiConnected != other.wifiConnected;
  }
};
struct ScreenLastSent {
  int8_t halt = -1;
  uint16_t speed = 9999;  // impossible value
  float perf = -1.0f;
  int8_t wifi = -1;
  uint8_t machine = 0;
  // add others only if you send them to screen
};

ScreenLastSent screenLast;

// Queue for pending ATmega commands (raw <prefix><value> as array)
using AtmegaCommand = std::string;
std::queue<AtmegaCommand> pendingAtmegaCommands;
// queue commands for screen updates
std::queue<ScreenCommand> screenQueue;

WiFiManager wm;
Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket(81);
// Global Instances
MachineState state;
HttpRequestState httpState;
WiFiReconnectState wifiState;
DashboardData currentData;
DashboardData lastSentData;
// USART instances
HardwareSerial atmegaSerial(0);  // UART0 for ATmega328
HardwareSerial screenSerial(2);  // UART2 for screen

volatile bool dataReceived = false;
char commandBuffer[BUFFER_SIZE];
String serialBuffer;
char screenBuffer[512];
size_t screenBufPos = 0;
unsigned long lastFetchTime = 0;
unsigned long restartTime = 0;

uint16_t lastProductionStepCode = 0;
String lastProductionStepLabel = "";

// Function Prototypes
void setupWiFi();
void setupWebServer();
void sendDashboardUpdate();
void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void handleAtmegaData();
void processAtmegaCommand(const char* cmd);
void processHaltEvent(int value);
void handleScreenData();
void processScreenJson(const char* json);
void sendWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions);
void handleWebRequest();
void fetchMachineData();
void updateMachineState(const JsonDocument& doc);
void serialTask(void* parameter);
void scheduleRestart();
String generateSessionToken();
bool isAuthenticated();
void resetMDNS();
void updateScreen(char prefix, int value);
void refreshScreenNow();
String processor(const String& var);
bool isApiReachable();

void setup() {
  // Initialize serial communications
  Serial.begin(115200);
  // Initialize pins
  pinMode(39, OUTPUT);
  digitalWrite(39, LOW);


  atmegaSerial.begin(38400, SERIAL_8N1, ESP32_RXD0, ESP32_TXD0);
  screenSerial.begin(9600, SERIAL_8N1, CYD_RXD, CYD_TXD);
  delay(100);

  // Initialize I2C
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  delay(1000);
  // Load configuration from Preferences
  preferences.begin("app-config", true);
  MACHINE_ID = preferences.getUChar("machine_id", MACHINE_ID);
  API_BASE_URL = preferences.getString("api_url", API_BASE_URL);
  sessionToken = preferences.getString("session_token", "");
  preferences.end();

  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(serialTask, "SerialTask", 12000, NULL, 1, NULL, 1);

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
      state.fCommandTimestamp = millis();
      delay(SAFETY_BUFFER_MS);  // 60-second delay only on real power fail
    }
  }
  if (!powerFailDetected) {
    Serial.printf(" No F1 after 3 attempts → warm restart (no power fail)\n", millis() / 1000.0);
  }

  // Proceed to WiFi setup
  setupWiFi();
  // Attach WiFi event handler
  WiFi.onEvent(handleWiFiEvent);
  // setup websocket
  webSocket.begin();
  webSocket.enableHeartbeat(30000, 3000, 2);
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
      case WStype_CONNECTED:
        Serial.printf(F("[WS] Client #%u connected → accepting EVERYONE (debug mode)\n"), num);
        // Optional: you can also print the client's IP for better debugging
        Serial.printf(F("[WS] Client #%u IP: %s\n"), num, webSocket.remoteIP(num).toString().c_str());

        // Force full refresh by resetting last sent data
        lastSentData = DashboardData{};  // This zeros/clears all fields
        // Immediately send current dashboard state to the newly connected client
        sendDashboardUpdate();
        break;

      case WStype_DISCONNECTED:
        Serial.printf(F("[WS] Client #%u disconnected\n"), num);
        break;
      case WStype_TEXT:
        // Optional: if you want to see what the client is sending
        Serial.printf(F("[WS] Client #%u sent: %.*s\n"), num, length, payload);
        // If you later implement client→server messages (commands, auth, etc.)
        // you can process them here
        break;

      case WStype_BIN:
        Serial.printf(F("[WS] Client #%u sent binary data (%u bytes)\n"), num, length);
        break;

      case WStype_ERROR:
        Serial.printf(F("[WS] Client #%u error occurred!\n"), num);
        break;

      case WStype_FRAGMENT_TEXT_START:
      case WStype_FRAGMENT_BIN_START:
      case WStype_FRAGMENT:
      case WStype_FRAGMENT_FIN:
        // Fragmented messages - usually rare in simple dashboards
        break;
      default:
        Serial.printf(F("[WS] Client #%u unknown event type: %d\n"), num, type);
        break;
    }
  });
  // Set mDNS hostname
  host = "machine" + String(MACHINE_ID);
  Serial.printf("Loaded configuration - API URL: %s, Machine ID: %u, mDNS Hostname: %s.local\n", API_BASE_URL.c_str(), MACHINE_ID, host.c_str());
  if (MDNS.begin(host.c_str())) {
    Serial.printf(F("mDNS started: %s.local\n"), host.c_str());
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "dev", "NsighTex");
    MDNS.addServiceTxt("http", "tcp", "id", String(MACHINE_ID));
    MDNS.addServiceTxt("http", "tcp", "device", "NsighTex");
  } else {
    Serial.printf(F("mDNS begin failed!\n"));
  }
}
void loop() {
  server.handleClient();
  webSocket.loop();
  scheduleRestart();
  handleScreenI2C();
  if (powerFailDetected && state.wifiConnected && isApiReachable() && !httpState.active) {
    unsigned long downtime = (millis() - state.fCommandTimestamp + SAFETY_BUFFER_MS) / 1000;
    sendWebRequest("0", MACHINE_ID, 19, 0, downtime);
    powerFailDetected = false;
  }
  static unsigned long lastLog = 0;
  if (millis() - lastLog >= 60000) {
    if (WiFi.status() != WL_CONNECTED || !isApiReachable()) {
      Serial.printf(F("Stale WiFi detected (API unreachable)! RSSI: %d dBm, IP: %s. Forcing reconnect...\n"), WiFi.RSSI(), WiFi.localIP().toString().c_str());
      wifiState.reconnecting = true;
      setupWiFi();
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
}
void serialTask(void* parameter) {
  for (;;) {
    handleScreenData();
    vTaskDelay(1 / portTICK_PERIOD_MS);
    handleAtmegaData();
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Yield for 10ms
  }
}
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(false);
  digitalWrite(39, 0);  // LED on
  String apName = "Machine" + String(MACHINE_ID);
  wm.setConfigPortalTimeout(180);  // 3 minutes max portal
  wm.setConnectTimeout(15);        // Wait 15s per attempt
  wm.setConnectRetries(4);         // Try 4 times before giving up
  wm.setSaveConfigCallback([]() {
    Serial.printf(F("WiFi credentials saved\n"));
  });
  // CRITICAL: This forces WiFiManager to try saved credentials first
  wm.setShowInfoErase(false);
  wm.setShowInfoUpdate(false);
  wm.setCleanConnect(true);  // Don't jump to portal immediately

  bool connected = false;
  bool hasSavedCreds = (wm.getWiFiSSID() != "");

  if (!hasSavedCreds) {
    // No saved creds → open portal immediately (on power up)
    Serial.printf(F("No saved WiFi credentials → starting config portal\n"));
    connected = wm.startConfigPortal(apName.c_str(), "Nsight1234");
  } else {
    // Has saved creds → try to connect without portal
    wm.setConfigPortalTimeout(0);                              // Disable auto portal
    connected = wm.autoConnect(apName.c_str(), "Nsight1234");  // Will try connect, return false if fails, no portal
  }

  if (connected) {
    Serial.printf(F("Connected! IP: %s\n"), WiFi.localIP().toString().c_str());
    state.wifiConnected = true;
    digitalWrite(39, 1);
    updateScreen('X', 1);
    // Save current lease as "static" if we don't have one yet
    setupWebServer();
    fetchMachineData();
    triggerPortal = false;
    wifiSetupDone = true;
  } else {
    updateScreen('X', 0);
    Serial.println("failed to connect");
    wifiSetupDone = true;  // Still mark as done to allow periodic retries
  }
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
  MDNS.end();
  delay(100);  // Small delay
  if (!MDNS.begin(host.c_str())) {
    Serial.printf(F("Error setting up mDNS responder!\n"));
  } else {
    Serial.printf(F("mDNS responder started: %s .local \n"), host);
    MDNS.addService("http", "tcp", 80);
    // Optional: make it show up nicer in some browsers
    MDNS.addServiceTxt("http", "tcp", "device", "NsighTex");
  }
}
void setupWebServer() {
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
    server.send(200, "text/html", page);
  });
  server.on("/sendCommand", HTTP_GET, [requireAuth]() {
    if (!requireAuth()) return;

    String cmd = server.arg("cmd");

    if (cmd == "R" || cmd == "r") {  // Accept R or r
      // Clear live data on ESP32 side
      liveData.currentMeters = 0;
      liveData.linearSpeed = 0.0f;
      liveData.drumRPM = 0;
      liveData.machineState = "normal_stop";

      // Force immediate dashboard update
      lastLiveSent = {};  // invalidate comparison
      sendDashboardUpdate();
      // updateScreen('H', 0);
      updateScreen('M', 0);
      // Send reset command to ATmega with explicit newline
      atmegaSerial.print("R\n");  // use print + \n instead of println for clarity
      Serial.println(">>> WEB RESET: Sent 'R\\n' to ATmega");

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
void handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf(F(" WiFi STA connected to SSID: %s\n"), WiFi.SSID().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf(F(" WiFi got IP: %s, Gateway: %s"), WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());
      state.wifiConnected = true;
      digitalWrite(39, 1);
      updateScreen('X', 1);
      setupWebServer();
      fetchMachineData();
      wifiState.reconnecting = false;
      currentData.wifiConnected = true;
      sendDashboardUpdate();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      {
        WiFiEventInfo_t info = {};  // Zero-initialize
        uint8_t reason = 0;         // Default
        Serial.printf(F("WiFi disconnected (no detailed reason available in this core version)\n"));
        state.wifiConnected = false;
        digitalWrite(39, 0);
        updateScreen('X', 0);
        triggerPortal = false;
        wifiState.reconnecting = false;
        currentData.wifiConnected = false;
        sendDashboardUpdate();
        // Trigger reconnect immediately
        setupWiFi();

        break;
      }
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.printf(F("Lost IP - forcing disconnect/reconnect\n"));
      setupWiFi();
      break;
  }
}
void handleAtmegaData() {
  static size_t bufferIndex = 0;
  while (atmegaSerial.available()) {
    char c = atmegaSerial.read();
    if (c == '\n' || c == '\r') {
      if (bufferIndex == 0) continue;
      commandBuffer[bufferIndex] = '\0';
      char prefix = commandBuffer[0];
      if (prefix == 'F') {
        Serial.printf(" F1 received → FORCING POWER FAIL DETECTION!\n", millis() / 1000.0);
        powerFailDetected = true;
      } else if (prefix == 'R' || prefix == 'S') {
        processAtmegaCommand(commandBuffer);
      } else if (httpState.active || !state.wifiConnected) {
        if (pendingAtmegaCommands.size() >= MAX_PENDING_COMMANDS) pendingAtmegaCommands.pop();
        pendingAtmegaCommands.push(std::string(commandBuffer));
      } else {
        processAtmegaCommand(commandBuffer);
        // atmegaSerial.println("A1");
      }
      bufferIndex = 0;
    } else if (bufferIndex < BUFFER_SIZE - 1) {
      commandBuffer[bufferIndex++] = c;
    } else {
      bufferIndex = 0;
    }
  }
}
void processAtmegaCommand(const char* cmd) {
  char prefix = cmd[0];
  String valStr = String(cmd + 1);
  int value = valStr.toInt();
  float fvalue = valStr.toFloat();
  bool liveChanged = false;
  Serial.print("received from Atmega328: ");
  Serial.print(cmd);
  Serial.println();
  switch (prefix) {
    case 'H':
      {
        String newState;
        if (value == 0) newState = "running";
        else if (value == 1) newState = "normal_stop";
        else if (value == 2) newState = "chain_stop";
        if (liveData.machineState != newState) {
          liveData.machineState = newState;
          liveChanged = true;
        }
        processHaltEvent(value);
        break;
      }
    case 'M':
      if (liveData.currentMeters != value) {
        liveData.currentMeters = value;
        liveChanged = true;
      }
      updateScreen('M', value);
      break;
    case 'S':
      if (abs(liveData.linearSpeed - fvalue) > 0.01f) {
        liveData.linearSpeed = fvalue;
        liveChanged = true;
      }
      updateScreen('S', (int)(fvalue * 100));
      break;
    case 'R':
      if (liveData.drumRPM != value) {
        liveData.drumRPM = value;
        liveChanged = true;
      }
      updateScreen('R', value);
      break;
    case 'L':
      sendWebRequest("0", MACHINE_ID, 120, 0, value);
      break;
    case 'F':
      if (!powerFailDetected) {
        powerFailDetected = true;
        Serial.printf(F(" Power fail confirmed — will log 60s downtime\n"), millis() / 1000.0);
      }
      break;
  }
  if (liveChanged) {
    sendDashboardUpdate();
  }
}
void processHaltEvent(int value) {
  state.haltIssue = value;
  sendDashboardUpdate();
  if (state.isOperational) {
    updateScreen('H', value);
    if (value == 1)
      sendWebRequest("0", MACHINE_ID, 101, 0, 0);
    else if (value == 0)
      sendWebRequest("0", MACHINE_ID, 100, 0, 0);
    else
      sendWebRequest("0", MACHINE_ID, value, 0, 0);
    
    updateScreen('H', value);
    if (value == 0) {
      state.lastHaltCode = 0;
    }
  } else if (state.resumeHaltCode) {
    updateScreen('H', state.lastHaltCode);
    state.resumeHaltCode = false;
  }
}
void handleScreenData() {
  static bool receiving = false;
  static bool inJson = false;

  while (screenSerial.available()) {
    char c = screenSerial.read();
    if (c == '<') {
      String marker = screenSerial.readStringUntil('>');  // Keep this String (infrequent)
      if (marker == "START") {
        receiving = true;
        inJson = true;
        screenBufPos = 0;
        screenBuffer[0] = '\0';
      } else if (marker == "END" && receiving) {
        receiving = false;
        inJson = false;
        screenBuffer[screenBufPos] = '\0';  // Null-terminate
        processScreenJson(screenBuffer);    // Pass char*
      }
    } else if (inJson && screenBufPos < sizeof(screenBuffer) - 1) {
      screenBuffer[screenBufPos++] = c;
    }
  }
}
void processScreenJson(const char* json) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error == DeserializationError::Ok) {
    const char* command = doc["command"];
    if (!command) return;
    Serial.printf(F(" Received Screen JSON Command: %s\n"), command);
    if (strcmp(command, "REQ") == 0) {
      const char* operatorID = doc["idOperator"];
      int haltCode = doc["codeStop"];
      uint16_t position = doc["reposition"];
      uint16_t revolutions = doc["revolution"];
      state.haltIssue = haltCode;
      sendWebRequest(operatorID, MACHINE_ID, haltCode, position, revolutions);
      if (haltCode >= 146 && haltCode <= 160) {
        state.isOperational = false;
        atmegaSerial.printf(F("P%d\n"), haltCode);
      } else {
        state.isOperational = true;
        atmegaSerial.printf(F("P%d"), haltCode);
      }
    } else if (strcmp(command, "URL") == 0) {
      const char* newUrl = doc["URL"];
      if (newUrl && strlen(newUrl) > 0) {
        String url = newUrl;
        if (url.startsWith("http://") || url.startsWith("https://")) {
          preferences.begin("app-config", false);
          preferences.putString("api_url", url);
          preferences.end();
          API_BASE_URL = url;
          fetchMachineData();
        }
      }
    } else if (strcmp(command, "Portal") == 0) {
      String apName = "Machine_" + String(MACHINE_ID);
      triggerPortal = true;
      wm.setTimeout(300);
      WiFi.disconnect();
      state.wifiConnected = false;
      updateScreen('X', 0);
      if (wm.startConfigPortal(apName.c_str(), "Nsight1234")) {
        state.wifiConnected = true;
        updateScreen('X', 1);
        setupWebServer();
        fetchMachineData();
      } else {
        setupWiFi();
      }
    } else if (strcmp(command, "IDmach") == 0) {
      JsonVariant machineIdVariant = doc["machineID"];
      if (!machineIdVariant.isNull()) {
        int newMachineId = machineIdVariant.as<int>();
        if (newMachineId >= 0 && newMachineId <= 255) {
          preferences.begin("app-config", false);
          preferences.putUChar("machine_id", newMachineId);
          preferences.end();
          MACHINE_ID = newMachineId;
          host = "machine" + String(MACHINE_ID);
          MDNS.setInstanceName(host.c_str());
          setupWiFi();
          setupWebServer();
          fetchMachineData();
        }
      }
    } else if (strcmp(command, "Data") == 0 && state.haltIssue == 0) {
      screenRequested = true;
      refreshScreenNow();
    }
  } else {
    Serial.printf(F(" Screen JSON Deserialization Error: %s\n"), error.c_str());
  }
}
void sendWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions) {
  if (!state.wifiConnected) {
    Serial.printf(F(" Skipped sendWebRequest: No WiFi connection\n"), millis() / 1000.0);
    setupWiFi();
    return;
  }
  if (httpState.active) {
    Serial.printf(F(" Skipped sendWebRequest: Previous request still active\n"), millis() / 1000.0);
    return;
  }
  if (!httpState.active) httpState.retryCount = 0;
  if (!httpState.http) { httpState.http = new HTTPClient(); }
  httpState.isFetchMachineData = false;
  httpState.active = true;
  snprintf(httpState.url, sizeof(httpState.url), "%sRecording_Events_Api.php", API_BASE_URL.c_str());
  httpState.http->setTimeout(HTTP_TIMEOUT_MS);
  httpState.http->setConnectTimeout(5000);
  httpState.http->begin(httpState.url);
  httpState.http->addHeader("Content-Type", "application/json");
  httpState.http->addHeader("User-Agent", "Machine" + String(MACHINE_ID));
  httpState.http->addHeader("Connection", "close");

  StaticJsonDocument<300> doc;
  doc["idOperator"] = operatorID ? operatorID : "0";
  doc["idMachine"] = machineID;
  doc["codeStop"] = haltCode;
  doc["idDevice"] = machineID;
  doc["reposition"] = position;
  doc["revolution"] = revolutions;
  doc["iPAddress"] = WiFi.localIP().toString();
  serializeJson(doc, httpState.json);

  Serial.printf(F(" sendWebRequest: %s\n"), httpState.json.c_str());
  handleWebRequest();
}
void fetchMachineData() {
  if (!state.wifiConnected) {
    Serial.printf(F(" Skipped fetch Machine Data: No WiFi connection\n"), millis() / 1000.0);
    setupWiFi();
    return;
  }
  if (millis() - lastFetchTime < 5000) {
    Serial.printf(F(" Skipped fetch Machine Data: Too frequent\n"), millis() / 1000.0);
    return;
  }
  if (httpState.active || !pendingAtmegaCommands.empty()) {
    Serial.printf(F(" Skipped fetch Machine Data: Prioritizing sendWebRequest (active=%d, queue size=%d)\n"),
                  httpState.active, pendingAtmegaCommands.size());
    return;
  }
  lastFetchTime = millis();
  if (!httpState.active) httpState.retryCount = 0;
  if (!httpState.http) { httpState.http = new HTTPClient(); }
  httpState.isFetchMachineData = true;
  httpState.active = true;
  snprintf(httpState.url, sizeof(httpState.url), "%sGet_Speed_and_Perfommence.php", API_BASE_URL.c_str());
  httpState.http->setTimeout(HTTP_TIMEOUT_MS);
  httpState.http->setConnectTimeout(5000);
  httpState.http->begin(httpState.url);
  httpState.http->addHeader("Content-Type", "application/json");
  httpState.http->addHeader("User-Agent", "ESP32-Machine/" + String(MACHINE_ID));
  httpState.http->addHeader("Connection", "close");

  StaticJsonDocument<100> requestDoc;
  requestDoc["idMachine"] = MACHINE_ID;
  serializeJson(requestDoc, httpState.json);

  Serial.printf(F(" fetch Machine Data - payload: %s\n"), httpState.json.c_str());
  handleWebRequest();
}
void handleWebRequest() {
  int maxAttempts = httpState.isFetchMachineData ? 1 : MAX_RETRIES;  // 3 for events, 1 for fetch
  bool success = false;

  for (httpState.retryCount = 1; httpState.retryCount <= maxAttempts; ++httpState.retryCount) {
    httpState.startTime = millis();
    int responseCode = httpState.http->POST(httpState.json);
    String payload = httpState.http->getString();

    Serial.printf(F(" HTTP %s Response Code: %d, Body: %s\n"), httpState.isFetchMachineData ? "FETCH" : "SEND", responseCode, payload.c_str());

    if (responseCode > 0) {
      if (httpState.isFetchMachineData) {
        StaticJsonDocument<300> responseDoc;
        if (deserializeJson(responseDoc, payload) == DeserializationError::Ok) {
          updateMachineState(responseDoc);
        }
      }
      success = true;
      break;  // Success → exit retry loop
    } else {
      Serial.printf(F(" HTTP Error (Attempt %d/%d): %s\n"), httpState.retryCount, maxAttempts, httpState.http->errorToString(responseCode).c_str());
      if (httpState.retryCount < maxAttempts) {
        delay(1000);  // Wait before retry
        httpState.http->end();
        httpState.http->begin(httpState.url);
        httpState.http->addHeader("Content-Type", "application/json");
        httpState.http->addHeader("User-Agent", "Machine" + String(MACHINE_ID));
      }
    }
  }
  // After loop (success or failure)
  if (!success) {
    Serial.printf(F(" HTTP request failed after %d attempts. Checking server reachability...\n"), maxAttempts);
    if (!isApiReachable()) {
      Serial.printf(F(" Server unreachable → forcing WiFi reconnect NOW\n"));
      WiFi.disconnect(true);  // true = clear state
      delay(200);             // Brief pause for lwIP cleanup
      setupWiFi();
      // Wait up to 15s for reconnect before continuing (non-blocking elsewhere)
      unsigned long timeout = millis() + 15000;
      while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        delay(500);
        Serial.printf(F("."));
      }
      Serial.printf(F("\n"));
      Serial.printf(F(WiFi.status() == WL_CONNECTED ? " Reconnected!\n" : " Reconnect timed out\n"));
    } else {
      Serial.printf(F(" Server reachable but request failed → possible server-side issue\n"));
    }
  }

  if (httpState.http) {
    httpState.http->end();
    delete httpState.http;
    httpState.http = nullptr;
  }
  httpState.active = false;
  httpState.retryCount = 0;
  // === NOW PROCESS NEXT QUEUED COMMAND (if any) ===
  if (!pendingAtmegaCommands.empty()) {
    const AtmegaCommand& cmd = pendingAtmegaCommands.front();
    pendingAtmegaCommands.pop();
    Serial.printf(" → Processing queued ATmega cmd: %s\n", cmd.data());
    processAtmegaCommand(cmd.data());  // ← This will call sendWebRequest() → gets 3 attempts!
    atmegaSerial.println("A1");
  }
}
void updateMachineState(const JsonDocument& doc) {
  state.lastHaltCode = doc["LastCodeStop"].as<int>();
  String label = doc["LastCodeLabel"].as<String>();
  state.performance = doc["performance"].as<float>();
  state.lastCodeLabel = label;  // Store LastCodeLabel

  currentData.performance = doc["performance"].as<float>();
  currentData.lastCodeStop = doc["LastCodeStop"].as<int>();
  currentData.lastCodeLabel = doc["LastCodeLabel"].as<String>();

  sendDashboardUpdate();

  if (state.lastHaltCode > 0) {
    updateScreen('H', state.lastHaltCode);
    state.haltIssue = state.lastHaltCode;
    atmegaSerial.printf("C%d\n", state.haltIssue);
    state.resumeHaltCode = true;
    if (state.lastHaltCode >= 46 && state.lastHaltCode <= 60) {
      state.isOperational = false;
      atmegaSerial.println("M1");
    }
  } else {
    refreshScreenNow();
  }
}
void handleScreenI2C() {
  unsigned long now = millis();
  // 1. Probe screen when we think it's dead (fast reconnect)
  static unsigned long lastProbe = 0;
  static bool lastScreenReady = false;
  if (!screenReady && (now - lastProbe >= 1000)) {  // probe every 600ms when missing
    lastProbe = now;
    Wire.beginTransmission(SCREEN_I2C_ADDR);
    screenReady = (Wire.endTransmission() == 0);
  }
  if (screenReady && !lastScreenReady) {  // ← New: detect transition to ready
    Serial.printf(F("Screen detected after being unavailable → forcing refresh\n"));
    refreshScreenNow();  // Force update on re-detection
  }
  lastScreenReady = screenReady;
  // 2. Send next command if ready and 200ms passed
  if (screenReady && !screenQueue.empty() && (now - lastI2CSentTime >= I2C_INTERVAL)) {
    ScreenCommand cmd = screenQueue.front();
    screenQueue.pop();

    char buf[16];
    snprintf(buf, sizeof(buf), "%c%d\n", cmd.prefix, cmd.value);

    Wire.beginTransmission(SCREEN_I2C_ADDR);
    Wire.write((const uint8_t*)buf, strlen(buf));
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      lastI2CSentTime = now;
      if (I2C_DEBUG_LOG) Serial.printf(F(" → Screen: %c%d\n"), cmd.prefix, cmd.value);
    } else {
      screenReady = false;  // screen disappeared
      if (screenQueue.size() >= 60) {
        // drop oldest or newest
        screenQueue.pop();
      }
      screenQueue.push(cmd);  // retry later
      if (I2C_DEBUG_LOG) Serial.printf(F(" I2C failed (err=%d), retrying %c%d later\n"), err, cmd.prefix, cmd.value);
    }
  }
}
void scheduleRestart() {
  if (restartTime != 0 && millis() - restartTime < 5000) {
  } else if (restartTime != 0) {
    ESP.restart();
  }
}
void updateScreen(char prefix, int value) {
  screenQueue.push({ prefix, value });
  Serial.printf(F("sent to screen: %c%d"), prefix, value);
}
void refreshScreenNow() {
  bool force = screenRequested;
  screenRequested = false;
  Serial.printf(F("Screan is updated\n"));
  if (state.haltIssue != screenLast.halt || force) {
    updateScreen('H', state.haltIssue);
  }
  if (state.revolutions != screenLast.speed || force) {
    updateScreen('S', (int)state.revolutions);
  }
  if (fabs(state.performance - screenLast.perf) > 0.09f || force) {
    updateScreen('P', int(round(state.performance * 10)));
  }
  if ((WiFi.status() == WL_CONNECTED ? 1 : 0) != screenLast.wifi || force) {
    updateScreen('X', WiFi.status() == WL_CONNECTED ? 1 : 0);
  }
  if (MACHINE_ID != screenLast.machine || force) {
    updateScreen('N', (int)MACHINE_ID);
  }
}
void sendDashboardUpdate() {
  bool changed = currentData.changed(lastSentData);

  if (liveData.machineState != lastLiveSent.machineState || liveData.currentMeters != lastLiveSent.currentMeters || abs(liveData.linearSpeed - lastLiveSent.linearSpeed) > 0.01f || liveData.drumRPM != lastLiveSent.drumRPM) {
    changed = true;
  }

  if (!changed) return;

  StaticJsonDocument<512> doc;

  // Original fields
  doc["speed"] = currentData.revolutions;
  doc["performance"] = roundf(currentData.performance * 10.0f) / 10.0f;
  doc["LastCodeStop"] = currentData.lastCodeStop;
  doc["LastCodeLabel"] = currentData.lastCodeLabel;
  doc["wifi"] = currentData.wifiConnected;
  doc["updating"] = updating;

  doc["currentProductionStep"] = lastProductionStepCode;  // e.g. 109
  doc["productionStepLabel"] = lastProductionStepLabel;   // e.g. "Ourdissage"

  // Ourdissoire live data
  doc["state"] = liveData.machineState;
  doc["currentMeters"] = liveData.currentMeters;
  doc["linearSpeed"] = roundf(liveData.linearSpeed * 100.0f) / 100.0f;
  doc["drumRPM"] = liveData.drumRPM;

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);

  lastSentData = currentData;
  lastLiveSent = liveData;
}
String processor(const String& var) {
  if (var == "MACHINE_ID") return String(MACHINE_ID);
  if (var == "API_URL") return API_BASE_URL;
  return String();
}
bool isApiReachable() {
  WiFiClient client;
  // Parse host/port (your code already has this)
  String url = API_BASE_URL;
  url.toLowerCase();
  int schemeEnd = url.indexOf("://");
  if (schemeEnd == -1) return false;
  String hostPart = url.substring(schemeEnd + 3);
  int pathStart = hostPart.indexOf('/');
  if (pathStart != -1) hostPart = hostPart.substring(0, pathStart);
  int portStart = hostPart.indexOf(':');
  String host = (portStart != -1) ? hostPart.substring(0, portStart) : hostPart;
  int port = (portStart != -1) ? hostPart.substring(portStart + 1).toInt() : 80;  // HTTP, not HTTPS

  client.setTimeout(3000);  // 3s timeout
  // Serial.printf(F(" Testing reachability: %s:%d\n"), host.c_str(), port);
  if (client.connect(host.c_str(), port)) {
    client.stop();
    Serial.printf(F(" Server reachable\n"));
    return true;
  }
  IPAddress gw = WiFi.gatewayIP();
  if (gw == IPAddress(0, 0, 0, 0) || WiFi.localIP() == IPAddress(0, 0, 0, 0) || WiFi.RSSI() == 0) {
    Serial.printf(F(" Invalid WiFi state (gw/IP/RSSI=0) → network issue\n"));
    return false;
  }

  client.setTimeout(2000);
  Serial.printf(F(" Testing gateway: %s:80\n"), gw.toString().c_str());
  if (client.connect(gw, 80)) {  // Or port 53 if your router uses DNS
    client.stop();
    Serial.printf(F(" Gateway reachable → server likely down (no WiFi reconnect needed)\n"));
    return true;  // Treat as "reachable" for WiFi purposes
  } else {
    Serial.printf(F(" Gateway unreachable → WiFi/network issue\n"));
    return false;
  }
}
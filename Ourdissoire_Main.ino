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
constexpr int endPointPowerupTime = 60000UL;
unsigned long restartTime = 0;
bool webServerStarted = false;
bool updating = false;

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
  bool resumeHaltCode = false;
  uint8_t stopCode = 100;
  uint8_t previousStopCode = 0;
  uint8_t stageCode = 0;
  unsigned long powerfailTimestamp = 0;
  int beamLength = 3500;
  uint8_t windSection = 0;
  int currentMeters = 0;
  int drumRevs = 0;

  bool wifiSetupDone = false;
  bool triggerPortal = false;
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

struct OurdissoireLive {
  String machineState = "normal_stop";  // running / normal_stop / chain_stop
  int currentMeters = 0;
  float linearSpeed = 0.0f;
  int drumRPM = 0;
};

OurdissoireLive liveData;
OurdissoireLive lastLiveSent;
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
DashboardData currentData;
DashboardData lastSentData;
uint16_t lastProductionStepCode = 0;
String lastProductionStepLabel = "";

WiFiManager wm;
MachineState state;
WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;
HttpRequestState httpState;
static RxState rxState = RxState::IDLE;
ProductionStages currentStage = OURDISSAGE;

// USART instances
HardwareSerial atmegaSerial(0);  // UART0 for ATmega328
HardwareSerial screenSerial(2);  // UART2 for screen
// ====== serial functions  sensors <=> main <=> screen display ============
void serialTask(void* parameter);
void handleScreenData();
void processScreenData(const char* data);
void handleAtmegaData();
void processAtmegaCommand(const char* cmd);
void UpdateScreen(char prefix, int value);
// ====== interface web server ========================
void sendWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions);
void fetchMachineData();
void queueWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions);
void handleWebRequest();
// ======= local web page and local web server =========
void handleWebServer();
void broadcastMachineState();
String processor(const String& var);
String generateSessionToken();
void resetMDNS();
bool isAuthenticated();
void sendDashboardUpdate();
// ======= wifi provisionning =========================
void setupWiFi();
void i2cScanner();

void setup(void) {
  Serial.begin(115200);
  atmegaSerial.begin(38400, SERIAL_8N1, ESP32_RXD0, ESP32_TXD0);
  screenSerial.begin(115200, SERIAL_8N1, CYD_RXD, CYD_TXD);
  // indicator LED for wifi connectivity
  pinMode(39, OUTPUT);
  digitalWrite(39, LOW);

  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  delay(1000);

  i2cScanner();

  // === Load / Initialize configuration from Preferences ===
  preferences.begin("app-config", false);  // false = read/write mode (required for writing)
  bool needsWrite = false;
  if (!preferences.isKey("machine_id")) {
    MACHINE_ID = 98;
    preferences.putUChar("machine_id", MACHINE_ID);
    Serial.println("NVS was empty → set default Machine ID = 99");
    needsWrite = true;
  }
  if (!preferences.isKey("api_url") || preferences.getString("api_url").length() == 0) {
    API_BASE_URL = "http://192.168.1.248:7272/DBLOG/";
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

  // Now safely read the values
  MACHINE_ID = preferences.getUChar("machine_id", 99);
  API_BASE_URL = preferences.getString("api_url", API_BASE_URL);
  sessionToken = preferences.getString("session_token", "");

  preferences.end();

  host = "machine" + String(MACHINE_ID);
  {
    // === Pre-fill default WiFi credentials if nothing is saved ===
    if (!wm.getWiFiIsSaved()) {
      Serial.println("No WiFi credentials found → setting factory defaults");

      const char* defaultSSID = host.c_str();
      const char* defaultPass = "hikma1234";

      // This forces WiFiManager to save the default credentials
      bool saved = wm.autoConnect(defaultSSID, defaultPass);

      if (saved) {
        Serial.printf("Default credentials saved successfully → SSID: %s\n", defaultSSID);
      } else {
        Serial.println("Failed to save default WiFi credentials");
      }
    } else {
      Serial.println("WiFi credentials already saved in storage");
    }
  }
  // Create FreeRTOS task (run on Core 1 - recommended)
  xTaskCreatePinnedToCore(serialTask, "SerialTask", 14000, NULL, 1, NULL, 1);


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
      delay(endPointPowerupTime);  // delay only on real power fail
    }
  }
  if (!powerFailDetected) {
    Serial.printf(" No F1 after 3 attempts → warm restart (no power fail)\n", millis() / 1000.0);
  }
  setupWiFi();
  Serial.printf("Loaded configuration - API URL: %s, Machine ID: %u, mDNS Hostname: %s.local\n", API_BASE_URL.c_str(), MACHINE_ID, host.c_str());

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

void loop(void) {
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
  Serial.printf("RAW: %s\n", data);

  char buffer[200];
  strncpy(buffer, data, sizeof(buffer));
  buffer[sizeof(buffer) - 1] = '\0';

  char* token = strtok(buffer, ",");

  char* cmd = NULL;
  char* value = NULL;
  char* opid = NULL;

  while (token != NULL) {
    char* colon = strchr(token, ':');
    if (colon) {
      *colon = '\0';
      char* key = token;
      char* val = colon + 1;

      if (strcmp(key, "cmd") == 0) cmd = val;
      else if (strcmp(key, "value") == 0) value = val;
      else if (strcmp(key, "OpID") == 0) opid = val;
    }
    token = strtok(NULL, ",");
  }
  if (!cmd) return;

  Serial.printf("CMD: %s | VALUE: %s | OpID: %s\n", cmd, value ? value : "NULL", opid ? opid : "NULL");

  if (strcmp(cmd, "Stage") == 0 && value) {
    state.stageCode = atoi(value);
    sendWebRequest(opid, MACHINE_ID, state.stageCode, 0, 0);
    switch (state.stageCode) {
      case 103: currentStage = ENCANTRAGE; break;
      case 106: currentStage = ENCANTRAGE_PARTIEL; break;
      case 104: currentStage = FIN_ENCANTRAGE; break;
      case 105: currentStage = NOUAGE; break;
      case 107: currentStage = PIQUAGE; break;
      case 109: currentStage = OURDISSAGE; break;
      case 111: currentStage = ENSOUPLAGE; break;
      case 112: currentStage = FIN_ENSOUPLAGE; break;
      default: printf("there is no stage that meets this Stage code\n");
    }
    Serial.printf("Stage: %d\n", currentStage);
  } else if (strcmp(cmd, "Halt") == 0 && value) {
    state.stopCode = atoi(value);
    sendWebRequest(opid, MACHINE_ID, state.stopCode, 0, 0);
    if (state.stopCode == 146 || state.stopCode == 147 || state.stopCode == 115) {
      state.onRepaire = true;
    } else {
      state.onRepaire = false;
    }
  } else if (strcmp(cmd, "machID") == 0 && value) {
    int id = atoi(value);
    if (id >= 0 && id <= 255) {
      MACHINE_ID = id;
      preferences.begin("app-config", false);
      preferences.putUChar("machine_id", id);
      preferences.end();
    }
  } else if (strcmp(cmd, "URL") == 0 && value) {
    String url = value;
    if (url.startsWith("http://") || url.startsWith("https://")) {
      API_BASE_URL = url;
      preferences.begin("app-config", false);
      preferences.putString("api_url", url);
      preferences.end();
    }
  } else if (strcmp(cmd, "Data") == 0) {
    UpdateScreen('N', MACHINE_ID);
    UpdateScreen('O', state.stageCode);
    UpdateScreen('X', state.wifiConnected == true ? 1 : 0);
    UpdateScreen('H', state.stopCode);
  } else if (strcmp(cmd, "Portal") == 0) {
    Serial.println("open wifi provesioning Portal");
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
        Serial.println("Power fail forced!");
      } else if (prefix == 'R' || prefix == 'M' || prefix == 'S' || prefix == 'A') {
        UpdateScreen(prefix, value);
        if (prefix == 'M') {
          if (value >= state.beamLength + 10) {
            state.windSection++;
            UpdateScreen('Y', state.windSection);
            atmegaSerial.println("R1");
          }
        }
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
        sendWebRequest("0", MACHINE_ID, state.stopCode, 0, 0);
      }
      Serial.printf("Stop code received: %d\n", value);
      break;
    case 'L':
      sendWebRequest("0", MACHINE_ID, 94, 0, value);
      Serial.printf("chunk Length per minute %d\n", value);
      break;
    default: Serial.printf("Unknown command from Atmega: %s\n", cmd); break;
  }

  // Now send to screen using the new signature
  UpdateScreen(prefix, value);
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
  // Serial.printf("Queued for screen: %s\n", cmd);
}
void sendScreenCommand() {
  if (screenCommandQueue.empty()) return;

  String cmd = screenCommandQueue.front();
  screenCommandQueue.pop();

  // Force clean state before every write
  Wire.end();
  delayMicroseconds(100);
  Wire.begin();  // re-init

  Wire.beginTransmission(SCREEN_I2C_ADDR);  // 0x08
  Wire.write((const uint8_t*)cmd.c_str(), cmd.length());
  Wire.write('\n');
  uint8_t error = Wire.endTransmission(true);  // true = send stop

  if (error != 0) {
    Serial.printf("I2C Error (%d) sending '%s'\n", error, cmd.c_str());
  }
  // else success (you can uncomment a print if you want confirmation)
}

//========== Handle server and coude data exchange =========
void sendWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions) {
  if (!state.wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi not connected → queuing request"));
    queueWebRequest(operatorID, machineID, haltCode, position, revolutions);
    return;
  }

  if (httpState.active) {
    Serial.println(F("HTTP busy → queuing request"));
    queueWebRequest(operatorID, machineID, haltCode, position, revolutions);
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
  snprintf(httpState.url, sizeof(httpState.url), "%sRecording_Events_Api.php", API_BASE_URL.c_str());
  httpState.http->setTimeout(httpState.HTTP_TIMEOUT_MS);
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

  httpState.jsonPayload.clear();
  serializeJson(doc, httpState.jsonPayload);

  Serial.printf(F("Sending web request: %s\n"), httpState.jsonPayload.c_str());

  handleWebRequest();
}
void fetchMachineData() {
  if (!state.wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi not connected → skipping fetch"));
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

  snprintf(httpState.url, sizeof(httpState.url), "%sGet_Speed_and_Perfommence.php", API_BASE_URL.c_str());

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
      if (httpState.isFetchMachineData) {
        StaticJsonDocument<300> responseDoc;
        if (deserializeJson(responseDoc, payload) == DeserializationError::Ok) {
          state.previousStopCode = responseDoc["LastCodeStop"].as<int>();
          String label = responseDoc["LastCodeLabel"].as<String>();
          state.performance = responseDoc["performance"].as<float>();
          state.beamLength = responseDoc["speed"].as<int>();
          state.stageCode = responseDoc["lainID"].as<int>();

          if (state.previousStopCode >= 100) {
            UpdateScreen('H', state.previousStopCode);
            if (state.previousStopCode == 146 || state.previousStopCode == 147 || state.previousStopCode == 115) {
              state.onRepaire = true;
            }
          }
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

  // Cleanup
  if (httpState.http) {
    httpState.http->end();
    delete httpState.http;
    httpState.http = nullptr;
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
void queueWebRequest(const char* operatorID, uint8_t machineID, int haltCode, int16_t position, uint16_t revolutions) {
  if (pendingWebRequests.size() >= 20) {
    pendingWebRequests.pop();
    Serial.println(F("Request queue full - dropped oldest"));
  }

  StaticJsonDocument<300> doc;
  doc["idOperator"] = operatorID ? operatorID : "0";
  doc["idMachine"] = machineID;
  doc["codeStop"] = haltCode;
  doc["idDevice"] = machineID;
  doc["reposition"] = position;
  doc["revolution"] = revolutions;
  doc["iPAddress"] = WiFi.localIP().toString();  // even if WiFi is down, we can store "0.0.0.0" or skip

  String jsonStr;
  serializeJson(doc, jsonStr);
  pendingWebRequests.push(jsonStr);

  Serial.printf(F("Request queued | Queue size: %d | HaltCode: %d\n"), pendingWebRequests.size(), haltCode);
}

// ========== Wifi provisionning =========================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(false);

  digitalWrite(39, LOW);

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

  bool connected = false;

  if (wm.getWiFiIsSaved()) {
    Serial.println(F("Saved WiFi credentials found → trying to connect..."));
    wm.setConfigPortalTimeout(0);  // Do not open portal during first try
    connected = wm.autoConnect(apName.c_str(), apPass.c_str());
  } else {
    Serial.println(F("No saved credentials → opening config portal"));
    connected = wm.startConfigPortal(apName.c_str(), apPass.c_str());
  }

  if (connected) {
    Serial.printf(F("✅ WiFi Connected! IP: %s\n"), WiFi.localIP().toString().c_str());
    state.wifiConnected = true;
    digitalWrite(39, HIGH);
    UpdateScreen('X', 1);
    handleWebServer();
    fetchMachineData();
  } else {
    Serial.println("❌ WiFi connection failed or portal timed out");
    state.wifiConnected = false;
    UpdateScreen('X', 0);
  }

  state.wifiSetupDone = true;
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
      UpdateScreen('M', 0);
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
void broadcastMachineState() {
  StaticJsonDocument<512> doc;

  doc["haltCode"] = state.stopCode;
  doc["prodStep"] = state.stageCode;
  doc["performance"] = state.performance;
  doc["currentMeters"] = state.currentMeters;
  doc["drumRPM"] = state.drumRevs;
  doc["linearSpeed"] = 0.0;  // calculate if you have speed
  doc["wifi"] = state.wifiConnected;
  // Add LastCodeLabel if you store it

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
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

// bool isApiReachable() {
//   WiFiClient client;
//   // Parse host/port (your code already has this)
//   String url = API_BASE_URL;
//   url.toLowerCase();
//   int schemeEnd = url.indexOf("://");
//   if (schemeEnd == -1) return false;
//   String hostPart = url.substring(schemeEnd + 3);
//   int pathStart = hostPart.indexOf('/');
//   if (pathStart != -1) hostPart = hostPart.substring(0, pathStart);
//   int portStart = hostPart.indexOf(':');
//   String host = (portStart != -1) ? hostPart.substring(0, portStart) : hostPart;
//   int port = (portStart != -1) ? hostPart.substring(portStart + 1).toInt() : 80;  // HTTP, not HTTPS

//   client.setTimeout(3000);  // 3s timeout
//   // Serial.printf(F(" Testing reachability: %s:%d\n"), host.c_str(), port);
//   if (client.connect(host.c_str(), port)) {
//     client.stop();
//     Serial.printf(F(" Server reachable\n"));
//     return true;
//   }
//   IPAddress gw = WiFi.gatewayIP();
//   if (gw == IPAddress(0, 0, 0, 0) || WiFi.localIP() == IPAddress(0, 0, 0, 0) || WiFi.RSSI() == 0) {
//     Serial.printf(F(" Invalid WiFi state (gw/IP/RSSI=0) → network issue\n"));
//     return false;
//   }

//   client.setTimeout(2000);
//   Serial.printf(F(" Testing gateway: %s:80\n"), gw.toString().c_str());
//   if (client.connect(gw, 80)) {  // Or port 53 if your router uses DNS
//     client.stop();
//     Serial.printf(F(" Gateway reachable → server likely down (no WiFi reconnect needed)\n"));
//     return true;  // Treat as "reachable" for WiFi purposes
//   } else {
//     Serial.printf(F(" Gateway unreachable → WiFi/network issue\n"));
//     return false;
//   }
// }

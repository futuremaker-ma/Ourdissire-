#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "ui.h"
#include "vars.h"
#include "screens.h"


uint32_t lastTick = 0;

int Machine_ID = 99;
char* productionStageLabel = "";
char* stopLabel = "";
bool isConnected = false;
bool isRunning = true;
bool hideReport = true;
ProductionStage currentStage = ENCANTRAGE;
Page displayIndex = DEFAULT_PAGE;
Category currentCategory = IDLE;
float performance = 0;
int beamLength = 0;
uint8_t section = 0;
int drumRevolutions = 0;
float linearSpeed = 0.0;
float angulareSpeed = 0.0;
double stageDuration = 0;
double stopDuration = 0;
char* URL = "";
char* operatorID = "";
char* command = "";
uint8_t stopCode = 0;
uint8_t stageCode = 103;

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480

#define I2C_ADDRESS 0x08  // I2C address of the ESP32
#define BUFFER_SIZE 10
char i2cBuffer[BUFFER_SIZE];

volatile bool dataReceivedFlag = false;

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 20 * (LV_COLOR_DEPTH / 8))
static lv_color_t* draw_buf = nullptr;


// Get the Touchscreen data
void touchscreen_read(lv_indev_t* indev, lv_indev_data_t* data) {
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 260, 3840, 1, SCREEN_WIDTH);
    y = map(p.y, 280, 3760, SCREEN_HEIGHT, 1);
    z = p.z;
    data->state = LV_INDEV_STATE_PRESSED;
    // Set the coordinates
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Wire.begin(I2C_ADDRESS);
  // Wire.onReceive(receiveEvent);

  // Start LVGL
  lv_init();
  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  touchscreen.setRotation(0);

  // Create a display object
  draw_buf = (lv_color_t*)malloc(DRAW_BUF_SIZE / 4);
  if (!draw_buf) {
    Serial.println("Failed to allocate memory for draw buffer!");
    while (1) { yield(); }
  }
  lv_display_t* disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, DRAW_BUF_SIZE);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
  // Initialize an LVGL input device object (Touchscreen)
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  // Set the callback function to read Touchscreen input
  lv_indev_set_read_cb(indev, touchscreen_read);
  // Function to draw the GUI (text, buttons and sliders)
  Serial.println("before ui_init()");
  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  Serial.printf("free: %d, biggest: %d\n", mon.free_cnt, mon.free_biggest_size);
  ui_init();
  Serial.println("preparation is done!");
  delay(1000);

  command = "data";
  SendData();
}
void loop() {

  // if (dataReceivedFlag) {
  //   char prefix = i2cBuffer[0];
  //   int value = 0;
  //   if (strlen(i2cBuffer) > 1) {
  //     value = atoi(&i2cBuffer[1]);
  //   } else {
  //     Serial.println("Invalid I2C data");
  //     dataReceivedFlag = false;
  //     return;
  //   }
  //   dataReceivedFlag = false;
  //   switch (prefix) {
  //     case 'H':
  //       stopCode = value;
  //       break;
  //     case 'P':
  //       performance = value / 10;
  //       break;
  //     case 'S':
  //       linearSpeed = value;
  //       angulareSpeed = linearSpeed / 3.15;
  //       break;
  //     case 'M':
  //       beamLength = value;
  //       break;
  //     case 'R':
  //       drumRevolutions = value;
  //       break;
  //     case 'N':
  //       Machine_ID = value;
  //       break;
  //     case 'X':
  //       isConnected = value == 1 ? true : false;
  //       break;
  //     case 'Y':
  //       section = value;
  //       break;
  //     default:
  //       Serial.println(F("Unknown command"));
  //       return;  // Exit if the prefix is unrecognized
  //   }
  // }

  uint32_t now = millis();
  if (now - lastTick >= 1000) {
    lastTick = now;
    if (stopCode == 0) {  // Machine is running
      stageDuration++;
    } else {  // Machine is stopped
      stopDuration++;
    }
  }

  static unsigned long lastLvglTick = 0;
  if (millis() - lastLvglTick >= 5) {
    lv_tick_inc(5);
    lastLvglTick = millis();
  }
  lv_task_handler();
  tick_screen_main();
  delay(5);  // let this time pass
}

// void receiveEvent(int numBytes) {
//   static int bufferIndex = 0;
//   while (Wire.available() && bufferIndex < BUFFER_SIZE - 1) {
//     char receivedChar = Wire.read();
//     if (receivedChar == '\n') {
//       i2cBuffer[bufferIndex] = '\0';
//       bufferIndex = 0;
//       dataReceivedFlag = true;
//       Serial.print("Received via I2C: ");
//       Serial.println(i2cBuffer);
//     } else {
//       i2cBuffer[bufferIndex++] = receivedChar;
//     }
//   }
//   // Discard excess bytes
//   while (Wire.available()) Wire.read();
//   if (bufferIndex >= BUFFER_SIZE - 1) {
//     i2cBuffer[BUFFER_SIZE - 1] = '\0';
//     bufferIndex = 0;
//     Serial.println("I2C buffer overflow");
//   }
// }

void SendData() {
  StaticJsonDocument<300> jsonDoc;

  if (strcmp(command, "Stage") == 0) {
    jsonDoc["command"] = "Stage";
    jsonDoc["stageCode"] = stageCode;
    // Optional: jsonDoc["currentStage"] = (int)currentStage;  // if you need it
  } else if (strcmp(command, "Halt") == 0) {
    jsonDoc["command"] = "Halt";
    jsonDoc["stopCode"] = stopCode;
  } else if (strcmp(command, "machID") == 0) {
    jsonDoc["command"] = "machID";
    jsonDoc["machineID"] = Machine_ID;  // better key name
  } else if (strcmp(command, "URL") == 0) {
    jsonDoc["command"] = "URL";
    jsonDoc["URL"] = URL;
  } else if (strcmp(command, "Portal") == 0) {
    jsonDoc["command"] = "Portal";
  } else if (strcmp(command, "Reboot") == 0) {
    jsonDoc["command"] = "Reboot";
  } else if (strcmp(command, "Data") == 0) {
    jsonDoc["command"] = "Data";
  } else {
    jsonDoc["command"] = command ? command : "Unknown";
  }
  // === Always add Operator ID when it's meaningful ===
  if (operatorID != NULL && operatorID[0] != '\0') {
    jsonDoc["OpID"] = operatorID;
  }
  // Serialize
  String payload;
  serializeJson(jsonDoc, payload);

  command = "";

  Serial.print("<START>");
  Serial.print(payload);
  Serial.println("<END>");
}

void printMemoryUsage() {
  size_t totalHeap = ESP.getHeapSize();             // Total heap size
  size_t freeHeap = ESP.getFreeHeap();              // Free heap size
  size_t usedHeap = totalHeap - freeHeap;           // Used heap size
  size_t largestFreeBlock = ESP.getMaxAllocHeap();  // Largest free block

  // Heap fragmentation percentage: (1 - largest free block / free heap) * 100
  float heapFragmentation = 0.0;
  if (freeHeap > 0) {
    heapFragmentation = (1.0 - ((float)largestFreeBlock / freeHeap)) * 100.0;
  } else {
    heapFragmentation = 0.0;  // Prevent division by zero
  }

  // Percentage of memory used
  float usagePercentage = (float)usedHeap / totalHeap * 100;

  Serial.println(F("=== Memory Usage ==="));
  Serial.printf("Used Heap: %u bytes (%.2f%%)\n", usedHeap, usagePercentage);
  Serial.printf("Heap Fragmentation: %.2f%%\n", heapFragmentation);
  // Serial.printf("Largest Free Block: %u bytes\n", largestFreeBlock);
  Serial.println(F("===================="));
}
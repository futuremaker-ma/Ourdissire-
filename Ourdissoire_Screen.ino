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
uint8_t stopCode = 100;
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


constexpr int I2C_SDA = 21;  // screen I2C SDA
constexpr int I2C_SCL = 22;  // screen I2C SCL
#define I2C_BUFFER_SIZE 64
char i2cBuffer[I2C_BUFFER_SIZE];
volatile int receivedLength = 0;
volatile bool dataReceivedFlag = false;
// ==================== I2C COMMAND QUEUE ====================
#define I2C_QUEUE_SIZE 8
char i2cQueue[I2C_QUEUE_SIZE][I2C_BUFFER_SIZE];
volatile uint8_t queueHead = 0;
volatile uint8_t queueTail = 0;


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

  // Prevent common TFT_eSPI + ESP32 APB callback warning
  WiFi.mode(WIFI_OFF);
  WiFi.setSleep(true);

  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin(0x08);
  Wire.onReceive(receiveEvent);

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

  command = "Data";
  SendData();
}
void loop() {

  // ==================== PROCESS I2C QUEUE ====================
  while (queueTail != queueHead) {  // while there are commands in queue
    char* cmd = i2cQueue[queueTail];
    char prefix = cmd[0];
    int value = atoi(&cmd[1]);

    Serial.printf("I2C received: %s  → prefix=%c value=%d\n", cmd, prefix, value);

    switch (prefix) {
      case 'H': stopCode = value; break;
      case 'O':
        stageCode = value;
        switch (value) {
          case 103: currentStage = ENCANTRAGE; break;
          case 106: currentStage = ENCANTRAGE_PARTIEL; break;
          case 104: currentStage = FIN_ENCANTRAGE; break;
          case 105: currentStage = NOUAGE; break;
          case 107: currentStage = PIQUAGE; break;
          case 109: currentStage = OURDISSAGE; break;
          case 111: currentStage = ENSOUPLAGE; break;
          case 112: currentStage = FIN_ENSOUPLAGE; break;
          default: Serial.printf("Unknown stage code: %d\n", value);
        }
        break;
      case 'N': Machine_ID = value; break;
      case 'P': performance = (float)value / 10.0; break;
      case 'M': beamLength = value; break;
      case 'R': drumRevolutions = value; break;
      case 'S': linearSpeed = (float)value / 100.0; break;
      case 'A': angulareSpeed = (float)value / 100.0; break;
      case 'X': isConnected = (value == 1); break;
      case 'Y': section = value; break;
      default: Serial.printf("Unknown I2C prefix: %c (full: %s)\n", prefix, cmd);
    }

    // Move to next command
    queueTail = (queueTail + 1) % I2C_QUEUE_SIZE;
  }

  uint32_t now = millis();
  if (now - lastTick >= 1000) {
    lastTick = now;
    if (stopCode == 100) {  // Machine is running
      stageDuration++;
    } else {  // Machine is stopped
      stopDuration++;
    }
    if (stopCode == 101 || stopCode == 102) {
      stageDuration++;
    }
  }
  // === LVGL handling (safer timing) ===
  static unsigned long lastLvglTick = 0;
  if (millis() - lastLvglTick >= 10) {  // try 15 or 20
    lv_tick_inc(15);
    lastLvglTick = millis();
  }
  lv_task_handler();
  tick_screen_main();
}
void receiveEvent(int numBytes) {
  if (numBytes == 0) return;

  char tempBuf[I2C_BUFFER_SIZE];
  int len = 0;

  while (Wire.available() && len < I2C_BUFFER_SIZE - 1) {
    char c = Wire.read();
    if (c == '\n' || c == '\r') break;  // stop at newline
    tempBuf[len++] = c;
  }
  tempBuf[len] = '\0';

  if (len < 2) return;  // invalid command

  // Add to queue (circular buffer)
  uint8_t nextHead = (queueHead + 1) % I2C_QUEUE_SIZE;
  if (nextHead != queueTail) {  // not full
    strncpy(i2cQueue[queueHead], tempBuf, I2C_BUFFER_SIZE - 1);
    i2cQueue[queueHead][I2C_BUFFER_SIZE - 1] = '\0';
    queueHead = nextHead;
  } else {
    Serial.println("I2C queue full!");
  }
}
void SendData() {
  Serial.print("<START>");

  // Command
  if (command && command[0] != '\0') {
    Serial.print("cmd:");
    Serial.print(command);
  } else {
    Serial.print("cmd:Data");
  }

  // Value (generic)
  if (strcmp(command, "Stage") == 0) {
    Serial.print(",value:");
    Serial.print(stageCode);
  } else if (strcmp(command, "Halt") == 0) {
    Serial.print(",value:");
    Serial.print(stopCode);
  } else if (strcmp(command, "machID") == 0) {
    Serial.print(",value:");
    Serial.print(Machine_ID);
  } else if (strcmp(command, "URL") == 0) {
    if (URL && URL[0] != '\0') {
      Serial.print(",value:");
      Serial.print(URL);
    }
  }

  // Operator ID (optional but consistent)
  if (operatorID && operatorID[0] != '\0') {
    Serial.print(",OpID:");
    Serial.print(operatorID);
  }

  Serial.println("<END>");

  operatorID = "0";
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

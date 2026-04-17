#include <NeoSWSerial.h>
#include <EEPROM.h>
#include <math.h>

#define EEPROM_ADDR_CIRC 0
NeoSWSerial mySerial(11, 12);

#define REVOLUTION_PIN 2
#define PIN_CHAIN_STOP 4

char mySerialBuffer[16];
uint8_t mySerialIndex = 0;

// ================== TIMING ==================
int16_t STOP_TIMEOUT_MS = 3000;
const unsigned long LENGTH_INTERVAL_MS = 60000;
const unsigned long STATUS_INTERVAL_MS = 500;
const uint8_t MIN_PULSES_TO_RESUME = 2;

// ================== MACHINE CONSTANTS ==================
float Avancement = 2.000f;
float DRUM_CIRCUMFERENCE_M = 3.125f;
float getInitialDiameter() {
  return DRUM_CIRCUMFERENCE_M / 3.14159265f;
}
const float TANGENT = 0.1756f;
const float SPEED_CHANGE_THRESHOLD = 0.5f;

// ================== VARIABLES ==================
volatile uint32_t drumRevolutions = 0;
volatile unsigned long lastPulseTime = 0;
volatile uint8_t recentPulses = 0;

uint32_t lastProcessedRevs = 0;
float beamLength = 0.0f;
float minuteLength = 0.0f;

float linearSpeed = 0.0f;
float lastLinearSpeed = 0.0f;

bool machineRunning = true;
bool zeroSpeedSent = false;
bool mainprogramready = false;
bool stopHandled = false;

unsigned long lastLengthTime = 0;
unsigned long lastStatusTime = 0;

float targetLength = 0.0f;
uint32_t targetRevolutions = 0;
int windSection = 1;

// ================== STAGES ==================
enum ProductionStages {
  ENCOUTRAGE,
  ENCOUTRAGE_PARTIEL,
  FIN_ENCOUTRAGE,
  NOUAGE,
  PIQUAGE,
  OURDISSAGE,
  ENSOUPLAGE,
  REPARATION
};
ProductionStages prodStage = ENCOUTRAGE;
// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  mySerial.begin(38400);

  pinMode(REVOLUTION_PIN, INPUT_PULLUP);
  pinMode(PIN_CHAIN_STOP, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(REVOLUTION_PIN), isrPulse, RISING);

  loadCircumference();

  Serial.println("System started");
}
// ================== LOOP ==================
void loop() {
  handleSerial();
  processRevolutions();
  handleSpeed();
  handleStopLogic();
  handleLengthBatch();
  handleStatus();
}
// ================== ISR ==================
void isrPulse() {
  drumRevolutions++;
  recentPulses++;
  lastPulseTime = millis();
}
// ================== PROCESS REVOLUTIONS ==================
void processRevolutions() {
  noInterrupts();
  uint32_t currentRevs = drumRevolutions;
  interrupts();

  while (lastProcessedRevs < currentRevs) {
    lastProcessedRevs++;

    float N = lastProcessedRevs;

    // ✅ FIXED: diameter grows correctly
    float diameter = getInitialDiameter() + (2.0f * N * TANGENT * Avancement / 1000.0f);
    float lengthThisRev = 3.14159265f * diameter;

    beamLength += lengthThisRev;
    minuteLength += lengthThisRev;
  }
}
// ================== SPEED ==================
void handleSpeed() {
  static unsigned long prevPulseTime = 0;

  noInterrupts();
  unsigned long pulseTime = lastPulseTime;
  interrupts();

  if (pulseTime != 0 && pulseTime != prevPulseTime) {
    float delta_t = (pulseTime - prevPulseTime) * 0.001f;

    if (delta_t > 0) {
      linearSpeed = DRUM_CIRCUMFERENCE_M / delta_t;

      if (machineRunning && abs(linearSpeed - lastLinearSpeed) >= SPEED_CHANGE_THRESHOLD) {
        int linearInt = (int)(linearSpeed * 100 + 0.5f);

        mySerial.print("S");
        mySerial.println(linearInt);

        float angularSpeed = 60.0f * linearSpeed / DRUM_CIRCUMFERENCE_M;
        int angularInt = (int)(angularSpeed * 100 + 0.5f);

        mySerial.print("A");
        mySerial.println(angularInt);

        lastLinearSpeed = linearSpeed;
        zeroSpeedSent = false;
      }
    }

    prevPulseTime = pulseTime;
  }
}
// ================== STOP LOGIC ==================
void handleStopLogic() {
  unsigned long now = millis();

  bool stopDetected = ((now - lastPulseTime) >= STOP_TIMEOUT_MS);

  // ================= STOP DETECTED =================
  if (stopDetected && machineRunning) {

    // Detect zones
    bool nearTarget = false;
    bool atBeginning = (drumRevolutions <= 1);

    if (targetRevolutions > 0) {
      int remaining = targetRevolutions - drumRevolutions;
      if (remaining <= 2) nearTarget = true;
    }

    // Ignore fake stops
    if (nearTarget || atBeginning) {
      recentPulses = 0;
      return;
    }

    // REAL transition to stopped
    machineRunning = false;
    stopHandled = false;  // allow handling once
    recentPulses = 0;
  }

  // ================= HANDLE STOP ONCE =================
  if (stopDetected && !machineRunning && !stopHandled) {

    stopHandled = true;

    if (!digitalRead(PIN_CHAIN_STOP)) {
      if (prodStage == OURDISSAGE) {
        mySerial.println("H102");
        Serial.println("CASSE FIL");
      }
    } else {
      if (prodStage == OURDISSAGE && targetRevolutions > 0) {

        if (drumRevolutions >= targetRevolutions - 1) {
          windSection++;

          mySerial.print("Y");
          mySerial.println(windSection);

          Serial.print("Section completed → ");
          Serial.println(windSection);

          beamLength = 0;
          drumRevolutions = 0;
          lastProcessedRevs = 0;
        } else {
          mySerial.println("H101");
          Serial.println("NORMAL STOP");
        }
      }
    }

    if (!zeroSpeedSent) {
      mySerial.println("S0.00");
      mySerial.println("A0.00");
      zeroSpeedSent = true;
      lastLinearSpeed = 0.0f;
    }
  }

  // ================= AUTO RESUME =================
  if (!machineRunning && recentPulses >= MIN_PULSES_TO_RESUME) {
    machineRunning = true;
    stopHandled = false;  // reset for next stop
    recentPulses = 0;

    if (prodStage == OURDISSAGE) {
      mySerial.println("H100");
      Serial.println("RUNNING");
    }
  }
}
// ================== LENGTH ==================
void handleLengthBatch() {
  unsigned long now = millis();

  if (now - lastLengthTime >= LENGTH_INTERVAL_MS) {
    mySerial.print("L");
    mySerial.println(minuteLength, 2);

    minuteLength = 0;
    lastLengthTime = now;
  }
}
// ================== STATUS ==================
void handleStatus() {
  unsigned long now = millis();

  if (machineRunning && now - lastStatusTime >= STATUS_INTERVAL_MS) {
    mySerial.print("R");
    mySerial.println(drumRevolutions);

    mySerial.print("M");
    mySerial.println(beamLength, 1);

    lastStatusTime = now;
  }
}
// ================== SERIAL ==================
void handleSerial() {
  while (mySerial.available()) {
    char c = mySerial.read();

    if (c == '\n') {
      mySerialBuffer[mySerialIndex] = '\0';
      processCommand(mySerialBuffer);
      mySerialIndex = 0;
    } else if (mySerialIndex < sizeof(mySerialBuffer) - 1) {
      mySerialBuffer[mySerialIndex++] = c;
    }
  }
}
// ================== COMMAND ==================
void processCommand(char* cmd) {
  char command = cmd[0];
  int value = atoi(cmd + 1);

  switch (command) {
    case 'R':
      beamLength = 0;
      drumRevolutions = 0;
      lastProcessedRevs = 0;
      windSection = 1;
      break;

    case 'A':
      Avancement = value / 1000.0f;
      break;
    case 'C':
      {
        float newCirc = value / 1000.0f;
        updateCircumference(newCirc);
      }
      break;
    case 'L':
      targetLength = value;
      targetRevolutions = computeTargetRevolutions(targetLength);
      break;

    case 'P':
      if (value == 109) {
        prodStage = OURDISSAGE;

        beamLength = 0;
        drumRevolutions = 0;
        lastProcessedRevs = 0;
        minuteLength = 0;
        windSection = 1;

        mySerial.print("Y");
        mySerial.println(windSection);
      }
      break;
  }

  STOP_TIMEOUT_MS = (prodStage == ENSOUPLAGE) ? 6000 : 3000;
}
// ================== TARGET CALC ==================
uint32_t computeTargetRevolutions(float targetL) {
  float L = 0.0f;
  uint32_t N = 0;

  while (L < targetL) {
    N++;

    float diameter = getInitialDiameter() + (2.0f * N * TANGENT * Avancement / 1000.0f);
    float lengthThisRev = 3.14159265f * diameter;

    L += lengthThisRev;

    if (N > 9000) break;
  }

  return N;
}
void updateCircumference(float newValue) {
  float currentValue;
  EEPROM.get(EEPROM_ADDR_CIRC, currentValue);

  // Validate stored value
  bool valid = (currentValue > 0.25f && currentValue < 10.0f);

  if (!valid || fabs(currentValue - newValue) > 0.0005f) {

    EEPROM.put(EEPROM_ADDR_CIRC, newValue);
    DRUM_CIRCUMFERENCE_M = newValue;

    Serial.println("Circumference updated");
  } else {
    Serial.println("No change");
  }
}
void loadCircumference() {
  float stored;
  EEPROM.get(EEPROM_ADDR_CIRC, stored);
  bool valid = (stored > 0.25f && stored < 10.0f);
  if (valid) {
    DRUM_CIRCUMFERENCE_M = stored;
    Serial.println("EEPROM circumference loaded");
  } else {
    DRUM_CIRCUMFERENCE_M = 3.125f;
    EEPROM.put(EEPROM_ADDR_CIRC, DRUM_CIRCUMFERENCE_M);
    Serial.println("EEPROM initialized with default circumference");
  }
}

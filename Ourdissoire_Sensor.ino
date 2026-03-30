#include <NeoSWSerial.h>

NeoSWSerial mySerial(11, 12);

#define REVOLUTION_PIN 2
#define PIN_CHAIN_STOP 4

char mySerialBuffer[16];
uint8_t mySerialIndex = 0;
int16_t STOP_TIMEOUT_MS = 4000;

// ================== CONSTANTS ==================
const unsigned long SPEED_INTERVAL_MS = 1000;
const unsigned long LENGTH_INTERVAL_MS = 60000;
const unsigned long STATUS_INTERVAL_MS = 500;
const uint8_t MIN_PULSES_TO_RESUME = 2;

const float INITIAL_CIRCUMFERENCE = 3.125f;
const float DIAMETER_INCREASE_PER_50_REVS = 0.02f;
const float CIRC_INCREASE_PER_REV = (3.1415926535f * DIAMETER_INCREASE_PER_50_REVS) / 50.0f;

// ===== MECHANICS =====
const float DRUM_CIRCUMFERENCE_M = 3.15;
const float SPEED_CHANGE_THRESHOLD = 0.5f;

// ================== VARIABLES ==================
volatile uint32_t speedPulses = 0;
volatile uint32_t drumRevolutions = 0;
volatile float beamLength = 0.0f;

volatile uint16_t stopTime = 0;
volatile uint16_t speedTimer = 0;
volatile uint32_t lengthTimer = 0;
volatile uint32_t statusTimer = 0;
volatile uint8_t recentPulses = 0;
volatile unsigned long lastPulseTime = 0;
volatile float lastLinearSpeed = 0.0f;
volatile bool zeroSpeedSent = false;

bool machineRunning = true;
bool sendData = false;
bool mainprogramready = false;
// batch length
float minuteLength = 0;
float linearSpeed = 0;


enum ProductionStages {
  ENCOUTRAGE,
  NOUAGE,
  PIQUAGE,
  OURDISSAGE,
  ENSOUPLAGE,
  REPARATION
};

ProductionStages prodStage = NOUAGE;
// ================== SETUP ==================
void setup() {

  Serial.begin(115200);
  mySerial.begin(38400);

  pinMode(REVOLUTION_PIN, INPUT_PULLUP);
  pinMode(PIN_CHAIN_STOP, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(REVOLUTION_PIN), isrPulse, RISING);
  setupTimers();

  lastPulseTime = 0;
  lastLinearSpeed = 0.0f;
  linearSpeed = 0.0f;
  zeroSpeedSent = false;

  Serial.println("System started");
}
// ================== MAIN LOOP ==================
void loop() {
  if (mySerial.available()) {
    char c = mySerial.read();
    if (c == '\n') {
      mySerialBuffer[mySerialIndex] = '\0';
      processMySerialCommand(mySerialBuffer);
      mySerialIndex = 0;
    } else if (mySerialIndex < sizeof(mySerialBuffer) - 1) {
      mySerialBuffer[mySerialIndex++] = c;
    }
  }
  if (machineRunning && stopTime >= STOP_TIMEOUT_MS && prodStage != REPARATION) {

    // Protection: if we had a pulse very recently → don't stop (glitch/timer lag)
    if ((millis() - lastPulseTime) < 300) {  // had pulse in last 300 ms → ignore
      stopTime = 500;                        // reset partially to give breathing room
      Serial.println("Stop ignored - recent pulse detected");
    } else {
      // real stop
      if (!digitalRead(PIN_CHAIN_STOP)) {
        mySerial.println("H102");
        Serial.println("CASSE FIL");
      } else {
        mySerial.println("H101");
        Serial.println("NORMAL STOP");
      }

      machineRunning = false;
      recentPulses = 0;

      // Optional: force zero speed on stop
      if (lastLinearSpeed != 0.0f) {
        mySerial.println("S0.00");
        Serial.println("STOP → S0.00 m/s");
        lastLinearSpeed = 0.0f;
        linearSpeed = 0.0f;
        zeroSpeedSent = true;
      }
    }
  }
  // ===== AUTO RESUME =====
  if (!machineRunning) {
    if (recentPulses >= MIN_PULSES_TO_RESUME) {  // just need enough recent pulses
      machineRunning = true;
      mySerial.println("H100");
      Serial.println("RUNNING");
      recentPulses = 0;        // reset counter
      zeroSpeedSent = false;   // also allow speed updates again
      lastLinearSpeed = 0.0f;  // force resend of new speed
    }
  }
  // ===== LINEAR SPEED TIMEOUT ZERO =====
  if ((millis() - lastPulseTime) >= STOP_TIMEOUT_MS) {
    if (!zeroSpeedSent && lastLinearSpeed != 0.0f) {
      mySerial.println("S0.00");
      // Serial.println("LINEAR SPEED TIMEOUT → S0.00 m/s");

      lastLinearSpeed = 0.0f;
      linearSpeed = 0.0f;
      zeroSpeedSent = true;
    }
  }
  // ===== LENGTH EVERY MINUTE =====
  if (lengthTimer >= LENGTH_INTERVAL_MS) {
    sendLengthBatch();
  }

  if (machineRunning && statusTimer >= STATUS_INTERVAL_MS) {
    noInterrupts();
    uint32_t revs = drumRevolutions;
    float blen = beamLength;
    interrupts();

    mySerial.print("R");
    mySerial.println(revs);
    mySerial.print("M");
    mySerial.println(blen, 1);

    // Serial.print("total revolutions =>R");
    // Serial.println(drumRevolutions);
    // Serial.print("beam length => M");
    // Serial.println(beamLength, 1);

    statusTimer = 0;
  }
}
// ================== FUNCTIONS ==================
void sendLengthBatch() {

  mySerial.print("L");
  mySerial.println(minuteLength, 2);

  // Serial.print("LENGH FEED this MINUTE => L");
  // Serial.println(minuteLength, 2);

  minuteLength = 0;
  lengthTimer = 0;
}
void processMySerialCommand(char* cmd) {
  Serial.print("ESP32: ");
  Serial.println(cmd);
  char command = cmd[0];
  int value = atoi(cmd + 1);
  switch (command) {
    case 'R':
      beamLength = 0;
      drumRevolutions = 0;
      break;
    case 'S':
      Serial.println("Received 'S' command, starting program");
      if (!mainprogramready) {
        mySerial.println("F1");
        Serial.println("Sent 'F1' to ESP32-S3");
        mainprogramready = true;
      }
      break;
    case 'P':
      switch (value) {
        case 103: prodStage = ENCOUTRAGE; break;
        case 105: prodStage = NOUAGE; break;
        case 107: prodStage = PIQUAGE; break;
        case 109: prodStage = OURDISSAGE; break;
        case 111: prodStage = ENSOUPLAGE; break;
        case 115:
        case 146:
        case 147: prodStage = REPARATION; break;
        default: break;
      }
      break;
    default:
      Serial.println("Invalid data from esp32");
  }
  if (prodStage == ENSOUPLAGE) {
    STOP_TIMEOUT_MS = 6000;
  } else {
    STOP_TIMEOUT_MS = 4000;
  }
}
// ================== INTERRUPTS ==================
void isrPulse() {
  stopTime = 0;
  if (recentPulses < 3) recentPulses++;

  unsigned long currentTime = millis();  // Get current time

  speedPulses++;  // Optional: Keep if you still want pulse counting elsewhere
  noInterrupts();
  drumRevolutions++;
  float lengthThisRev = max(INITIAL_CIRCUMFERENCE, INITIAL_CIRCUMFERENCE + CIRC_INCREASE_PER_REV * (drumRevolutions - 1));
  beamLength += lengthThisRev;
  minuteLength += lengthThisRev;
  // beamLength = drumRevolutions * ((drumRevolutions * 0.02 / 50) + 3.1);
  interrupts();

  zeroSpeedSent = false;
  // Calculate speed only if we have a previous pulse
  if (lastPulseTime != 0) {
    float delta_t = (currentTime - lastPulseTime) / 1000.0f;  // Seconds between pulses
    if (delta_t > 0) {                                        // Avoid divide-by-zero
      linearSpeed = DRUM_CIRCUMFERENCE_M / delta_t;

      // Send only if changed enough (and machine is running)
      if (machineRunning && abs(linearSpeed - lastLinearSpeed) >= SPEED_CHANGE_THRESHOLD) {
        mySerial.print("S");
        mySerial.println(linearSpeed, 2);

        // Serial.print("LINEAR SPEED => S");
        // Serial.print(linearSpeed, 2);
        // Serial.println("m/s");

        lastLinearSpeed = linearSpeed;
      }
    }
  }

  lastPulseTime = currentTime;  // Update for next time
}
void setupTimers() {

  noInterrupts();

  // ---- Timer2 10ms tick ----
  TCCR2A = 0;
  TCCR2B = 0;
  OCR2A = 155;
  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
  TIMSK2 |= (1 << OCIE2A);

  interrupts();
}
// ===== 10ms tick =====
ISR(TIMER2_COMPA_vect) {

  if (machineRunning) {
    stopTime += 10;
    speedTimer += 10;
    lengthTimer += 10;
    statusTimer += 10;
  }
}

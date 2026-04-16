#include <NeoSWSerial.h>

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
const float INITIAL_DIAMETER = 0.9947f;
const float TANGENT = 0.1756f;
const float DRUM_CIRCUMFERENCE_M = 3.15f;
const float SPEED_CHANGE_THRESHOLD = 0.5f;

// ================== VARIABLES ==================
volatile uint32_t drumRevolutions = 0;
volatile uint32_t pulseCount = 0;
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
  pulseCount++;
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
    float diameter = INITIAL_DIAMETER + (2.0f * N * TANGENT * Avancement / 1000.0f);
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

  if ((now - lastPulseTime) >= STOP_TIMEOUT_MS) {
    if (machineRunning) {

      if (!digitalRead(PIN_CHAIN_STOP)) {
        if (prodStage == OURDISSAGE) {
          mySerial.println("H102");
          Serial.println("CASSE FIL");
        }
      } else {
        // ===== SECTION COMPLETE =====
        if (prodStage == OURDISSAGE && targetRevolutions > 0) {
          // Optional validation (you can relax this if needed)
          if (drumRevolutions >= targetRevolutions * 0.98f) {
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

      machineRunning = false;
      recentPulses = 0;
    }

    if (!zeroSpeedSent) {
      mySerial.println("S0.00");
      mySerial.println("A0.00");
      zeroSpeedSent = true;
      lastLinearSpeed = 0.0f;
    }
  }

  // AUTO RESUME
  if (!machineRunning && recentPulses >= MIN_PULSES_TO_RESUME) {
    machineRunning = true;
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
// ================== COMMAND PROCESS ==================
void processCommand(char* cmd) {
  Serial.println(cmd);
  char command = cmd[0];
  int value = atoi(cmd + 1);

  switch (command) {
    case 'R':
      beamLength = 0;
      drumRevolutions = 0;
      lastProcessedRevs = 0;
      windSection = 1;
      break;
    case 'A': Avancement = value / 1000.0f; break;
    case 'L':
      targetLength = value;  // in meters
      targetRevolutions = computeTargetRevolutions(targetLength);

      Serial.print("Target Length: ");
      Serial.println(targetLength);

      Serial.print("Target Revs: ");
      Serial.println(targetRevolutions);
      break;
    case 'S':
      Serial.println("Received 'S' command, starting program");
      if (!mainprogramready) {
        mySerial.println("F1");
        Serial.println("Sent 'F1' to ESP32-S3");
        mainprogramready = true;
      }
      Serial.println("D1");
      break;

    case 'P':
      switch (value) {
        case 103: prodStage = ENCOUTRAGE; break;
        case 104: prodStage = FIN_ENCOUTRAGE; break;
        case 106: prodStage = ENCOUTRAGE_PARTIEL; break;
        case 105: prodStage = NOUAGE; break;
        case 107: prodStage = PIQUAGE; break;
        case 109:
          prodStage = OURDISSAGE;
          beamLength = 0;
          drumRevolutions = 0;
          lastProcessedRevs = 0;
          minuteLength = 0;
          windSection = 1;
          mySerial.println("D1");
          mySerial.print("Y");
          mySerial.println(windSection);
          break;
        case 111: prodStage = ENSOUPLAGE; break;
        case 115:
        case 146:
        case 147: prodStage = REPARATION; break;
        default: break;
      }
      break;

    default:
      Serial.println("Invalid command");
  }

  STOP_TIMEOUT_MS = (prodStage == ENSOUPLAGE) ? 6000 : 3000;
}
uint32_t computeTargetRevolutions(float targetL) {
  float L = 0.0f;
  uint32_t N = 0;

  while (L < targetL) {
    N++;

    // 👇 KEY FIX: 2.0f × because TANGENT is radial growth, not diameter growth
    float diameter = INITIAL_DIAMETER + (2.0f * N * TANGENT * Avancement / 1000.0f);
    float lengthThisRev = 3.14159265f * diameter;

    L += lengthThisRev;

    // Safety limit
    if (N > 9000) break;
  }

  return N;
}

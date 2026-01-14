#include <AccelStepper.h>
#include <HX711_ADC.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// ========== PIN CONFIGURATION ==========
const int HX711_dout = 3;
const int HX711_sck = 2;
#define EN_PIN 7
#define STEP_PIN 6
#define DIR_PIN 5

// ========== HARDWARE OBJECTS ==========
HX711_ADC LoadCell(HX711_dout, HX711_sck);
AccelStepper stepper(1, STEP_PIN, DIR_PIN);

// ========== LOAD CELL SETTINGS ==========
const int SAMPLE_SIZE = 10;
const float NOISE_FILTER = 0.1;
const float GRAVITY = 9.81;

// Moving Average Filter
float readings[SAMPLE_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;
float previousWeight = 0;

// ========== MOTOR SETTINGS ==========
const long testDistance = 24000;  // 15mm
const float homeSpeed = 3200;
const float homeAccel = 1600;

// ========== TEST SPEED PROFILES ==========
struct SpeedTest {
  const char* name;
  float speed;           // steps/sec
  float speed_mms;       // mm/sec
};

SpeedTest tests[8] = {
  {"ULTRA_FAST", 1600,  1.000},
  {"FAST_UP",    1200,  0.750},
  {"FAST_DOWN",  400,   0.250},
  {"MEASURE_F",  50.0,  0.03125},
  {"MEASURE_M",  20.0,  0.0125},
  {"MEASURE_U",  10.0,  0.00625},
  {"MEASURE_X",  5.0,   0.003125},
  {"MEASURE_Z",  2.0,   0.00125}
};

// ========== SYSTEM STATE ==========
enum TestState { IDLE, MOVING_DOWN, RETURNING_HOME, COMPLETE };
TestState testState = IDLE;

long homePosition = 0;
long targetPosition = 0;
int currentTestIndex = -1;
bool emergencyStop = false;

// ========== FORCE MEASUREMENT ==========
float lastForce = 0;
const float CONTACT_THRESHOLD = 0.05;
const float OVERLOAD_LIMIT = 5.0;
bool contactDetected = false;

// ========== DATA STREAMING ==========
bool isStreaming = false;
unsigned long startTime = 0;
unsigned long lastStreamTime = 0;
const int STREAM_INTERVAL = 50; // 20Hz

// ========== TIMING ==========
unsigned long lastDisplayUpdate = 0;
const int DISPLAY_INTERVAL = 1000;

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Motor setup
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  stepper.setMaxSpeed(homeSpeed);
  stepper.setAcceleration(homeAccel);
  
  // HX711 LoadCell setup
  Serial.println("========================================");
  Serial.println("Surface Tension Tester v3.0");
  Serial.println("Python-Controlled Mode");
  Serial.println("========================================");
  Serial.println("Initializing Load Cell...");
  
  LoadCell.begin();
  float calibrationValue = 16800.0;
  
  unsigned long stabilizingtime = 3000;
  boolean _tare = true;
  
  LoadCell.start(stabilizingtime, _tare);
  
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("ERROR: HX711 Timeout!");
    while (1);
  }
  
  LoadCell.setCalFactor(calibrationValue);
  
  // Initialize moving average
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    readings[i] = 0;
  }
  
  Serial.println("✓ System Ready!");
  Serial.println();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  LoadCell.update();
  handleSerialCommand();
  
  if (emergencyStop) {
    handleEmergencyStop();
    return;
  }
  
  // State machine
  switch (testState) {
    case MOVING_DOWN:
      handleMovingDown();
      break;
    case RETURNING_HOME:
      handleReturningHome();
      break;
    default:
      break;
  }
  
  // Stream data continuously when testing
  if (isStreaming && millis() - lastStreamTime >= STREAM_INTERVAL) {
    streamData();
    lastStreamTime = millis();
  }
  
  // Check tare status
  if (LoadCell.getTareStatus() == true) {
    Serial.println("✓ Tare complete");
    resetFilters();
  }
}

// =====================================================
// STATE HANDLERS
// =====================================================
void handleMovingDown() {
  stepper.run();
  float currentForce = getCurrentForce();
  
  // Detect contact
  if (!contactDetected && currentForce > CONTACT_THRESHOLD) {
    contactDetected = true;
    Serial.println(">>> CONTACT DETECTED!");
  }
  
  // Overload protection
  if (currentForce > OVERLOAD_LIMIT) {
    Serial.println("⚠️ OVERLOAD DETECTED!");
    emergencyStop = true;
    return;
  }
  
  // Check if reached target
  if (stepper.distanceToGo() == 0) {
    stopStreaming();
    testState = RETURNING_HOME;
    stepper.setMaxSpeed(homeSpeed);
    stepper.setAcceleration(homeAccel);
    stepper.moveTo(homePosition);
    Serial.println(">>> Returning to HOME...");
  }
}

void handleReturningHome() {
  stepper.run();
  
  if (stepper.distanceToGo() == 0) {
    Serial.println("✓ HOME reached");
    testState = IDLE;
  }
}

void handleEmergencyStop() {
  stepper.stop();
  stepper.setSpeed(0);
  stopStreaming();
  
  Serial.println("❌ EMERGENCY STOP ACTIVE");
  
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'R' || cmd == 'r') {
      emergencyStop = false;
      testState = IDLE;
      Serial.println("✓ System reset");
    }
  }
}

// =====================================================
// SERIAL COMMANDS
// =====================================================
void handleSerialCommand() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    while (Serial.available() > 0) Serial.read();
    
    if (testState != IDLE && cmd != 'S' && cmd != 'R') {
      return;
    }
    
    // Test commands 1-8
    if (cmd >= '1' && cmd <= '8') {
      int testIndex = cmd - '1';
      startTest(testIndex);
    }
    
    // Other commands
    switch (cmd) {
      case 'T':
      case 't':
        Serial.println(">>> Taring scale...");
        LoadCell.tareNoDelay();
        break;
        
      case '0':
        homePosition = stepper.currentPosition();
        Serial.println(">>> HOME POSITION SET");
        break;
        
      case 'H':
      case 'h':
        Serial.println(">>> Going HOME...");
        stepper.setMaxSpeed(homeSpeed);
        stepper.setAcceleration(homeAccel);
        stepper.moveTo(homePosition);
        while (stepper.distanceToGo() != 0) {
          stepper.run();
        }
        Serial.println("✓ HOME reached");
        break;
        
      case 'S':
      case 's':
        if (testState != IDLE) {
          stepper.stop();
          stopStreaming();
          testState = IDLE;
          Serial.println("❌ TEST STOPPED");
        }
        break;
    }
  }
}

// =====================================================
// TEST CONTROL
// =====================================================
void startTest(int testIndex) {
  if (testIndex < 0 || testIndex >= 8) return;
  
  currentTestIndex = testIndex;
  SpeedTest test = tests[testIndex];
  
  Serial.println();
  Serial.println("=====================================");
  Serial.print("TEST: ");
  Serial.println(test.name);
  Serial.println("=====================================");
  
  // Reset
  contactDetected = false;
  
  // Setup motor
  targetPosition = homePosition + testDistance;
  stepper.setMaxSpeed(test.speed);
  stepper.setAcceleration(0);
  stepper.moveTo(targetPosition);
  
  // Start streaming
  startStreaming();
  
  testState = MOVING_DOWN;
  Serial.println(">>> Moving DOWN...");
}

// =====================================================
// FORCE MEASUREMENT
// =====================================================
float getCurrentForce() {
  LoadCell.update();
  
  float rawValue = LoadCell.getData();
  float filteredValue = movingAverageFilter(rawValue);
  filteredValue = noiseFilter(filteredValue);
  
  float force_N = (filteredValue / 1000.0) * GRAVITY;
  if (force_N < 0) force_N = 0;
  
  lastForce = force_N;
  return force_N;
}

float movingAverageFilter(float newReading) {
  total = total - readings[readIndex];
  readings[readIndex] = newReading;
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % SAMPLE_SIZE;
  average = total / SAMPLE_SIZE;
  return average;
}

float noiseFilter(float value) {
  if (abs(value - previousWeight) < NOISE_FILTER) {
    return previousWeight;
  }
  previousWeight = value;
  return value;
}

void resetFilters() {
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    readings[i] = 0;
  }
  total = 0;
  average = 0;
  previousWeight = 0;
}

// =====================================================
// DATA STREAMING (JSON FORMAT)
// =====================================================
void startStreaming() {
  isStreaming = true;
  startTime = millis();
  lastStreamTime = startTime;
  Serial.println("START_STREAM");
}

void stopStreaming() {
  if (!isStreaming) return;
  isStreaming = false;
  Serial.println("END_STREAM");
}

void streamData() {
  float time_sec = (millis() - startTime) / 1000.0;
  float motor_pos_mm = stepper.currentPosition() / 1600.0;
  float motor_speed_mms = tests[currentTestIndex].speed_mms;
  float force_N = getCurrentForce();
  
  // JSON format
  Serial.print("{\"t\":");
  Serial.print(time_sec, 3);
  Serial.print(",\"p\":");
  Serial.print(motor_pos_mm, 4);
  Serial.print(",\"s\":");
  Serial.print(motor_speed_mms, 6);
  Serial.print(",\"f\":");
  Serial.print(force_N, 5);
  Serial.println("}");
}
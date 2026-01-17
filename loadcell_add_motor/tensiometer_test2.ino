/*
 * Surface Tension Tester v5.0 - Mode-Based Architecture
 * 
 * MODES:
 * - IDLE: Waiting for user command (default)
 * - LIVE_MONITOR: Display force continuously (M command)
 * - NORMAL_TEST: Single test at selected speed (1-8 commands)
 * - AUTO_SEQUENCE: Run all 8 speeds automatically (A command)
 * - CALIBRATION: Calibrate load cell (C command)
 * 
 * Each mode is independent with clean state machine.
 * Position-based sampling, constant velocity, and calibration untouched.
 */

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
const int SAMPLE_SIZE = 5;
float readings[SAMPLE_SIZE];
int readIndex = 0;
float total = 0.0;

float getFilteredForce() {
  LoadCell.update();
  float rawValue = LoadCell.getData();
  
  total = total - readings[readIndex];
  readings[readIndex] = rawValue;
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % SAMPLE_SIZE;
  
  float gramValue = total / SAMPLE_SIZE;
  float force_N = (gramValue / 1000.0) * 9.81;
  
  return (force_N > 0) ? force_N : 0;
}

void resetFilters() {
  for (int i = 0; i < SAMPLE_SIZE; i++) readings[i] = 0;
  total = 0.0;
  readIndex = 0;
}

// ========== EEPROM CALIBRATION ==========
const int EEPROM_CAL_ADDR = 0;
const float DEFAULT_CAL_FACTOR = 16800.0;
float calibrationFactor = DEFAULT_CAL_FACTOR;

void saveCalibrationToEEPROM() {
  byte* ptr = (byte*) &calibrationFactor;
  for (int i = 0; i < 4; i++) {
    EEPROM.write(EEPROM_CAL_ADDR + i, ptr[i]);
  }
  #ifdef ESP32
    EEPROM.commit();
  #endif
  Serial.print("✓ Saved to EEPROM at address ");
  Serial.println(EEPROM_CAL_ADDR);
}

void loadCalibrationFromEEPROM() {
  byte* ptr = (byte*) &calibrationFactor;
  for (int i = 0; i < 4; i++) {
    ptr[i] = EEPROM.read(EEPROM_CAL_ADDR + i);
  }
  
  if (calibrationFactor < 5000 || calibrationFactor > 50000) {
    Serial.println("⚠ EEPROM contains invalid calibration factor.");
    Serial.print("Using default: ");
    Serial.println(DEFAULT_CAL_FACTOR);
    calibrationFactor = DEFAULT_CAL_FACTOR;
  }
}

// ========== MOTOR SETTINGS ==========
const long testDistance = 24000;  // 15 mm
const float homeSpeed = 3200;
const float homeAccel = 1600;

// ========== POSITION-BASED SAMPLING ==========
const long POSITION_SAMPLE_INTERVAL = 8;  // 5 micrometers
long lastSampledPosition = 0;

// ========== TEST SPEED PROFILES ==========
struct SpeedTest {
  const char* name;
  float speed;      // steps/sec
  float speed_mms;  // mm/sec
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

// ========== MODE STATE MACHINE ==========
enum SystemMode {
  MODE_IDLE,
  MODE_LIVE_MONITOR,
  MODE_NORMAL_TEST,
  MODE_AUTO_SEQUENCE,
  MODE_CALIBRATION
};

enum MotorState {
  MOTOR_IDLE,
  MOTOR_MOVING_DOWN,
  MOTOR_RETURNING_HOME
};

SystemMode currentMode = MODE_IDLE;
MotorState motorState = MOTOR_IDLE;

// ========== SYSTEM STATE ==========
long homePosition = 0;
long targetPosition = 0;
int currentTestIndex = -1;
int nextAutoTestIndex = -1;  // For auto sequence
bool emergencyStop = false;

// ========== FORCE MEASUREMENT ==========
float lastForce = 0;
const float CONTACT_THRESHOLD = 0.05;
const float OVERLOAD_LIMIT = 5.0;
bool contactDetected = false;

// ========== DATA STREAMING ==========
bool isStreaming = false;
unsigned long startTime = 0;

// ========== LIVE MONITOR STATE ==========
unsigned long lastMonitorDisplayTime = 0;
const unsigned long MONITOR_UPDATE_INTERVAL = 200;  // ms

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  stepper.setMaxSpeed(homeSpeed);
  stepper.setAcceleration(homeAccel);
  
  Serial.println("========================================");
  Serial.println("Surface Tension Tester v5.0");
  Serial.println("Mode-Based Architecture");
  Serial.println("========================================");
  
  loadCalibrationFromEEPROM();
  Serial.print("Calibration Factor (loaded): ");
  Serial.println(calibrationFactor);
  
  Serial.println("Initializing Load Cell...");
  
  LoadCell.begin();
  LoadCell.start(3000, true);
  
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("ERROR: HX711 Timeout!");
    while (1);
  }
  
  LoadCell.setCalFactor(calibrationFactor);
  resetFilters();
  
  Serial.println("✓ System Ready!");
  printMainMenu();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  LoadCell.update();
  
  // Handle serial commands (always active except during calibration)
  if (currentMode != MODE_CALIBRATION) {
    handleSerialCommand();
  }
  
  // Emergency stop check
  if (emergencyStop) {
    handleEmergencyStop();
    return;
  }
  
  // Dispatch to appropriate mode handler
  switch (currentMode) {
    case MODE_IDLE:
      handleIdleMode();
      break;
      
    case MODE_LIVE_MONITOR:
      handleLiveMonitorMode();
      break;
      
    case MODE_NORMAL_TEST:
      handleNormalTestMode();
      break;
      
    case MODE_AUTO_SEQUENCE:
      handleAutoSequenceMode();
      break;
      
    case MODE_CALIBRATION:
      handleCalibrationMode();
      break;
  }
  
  // Check tare status (independent of mode)
  if (LoadCell.getTareStatus()) {
    Serial.println("✓ Tare complete");
    resetFilters();
  }
}

// =====================================================
// MODE: IDLE
// =====================================================
void handleIdleMode() {
  // Motor stopped, no action
  // Waiting for user command
}

void printMainMenu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("MAIN MENU - SELECT MODE");
  Serial.println("========================================");
  Serial.println();
  Serial.println("MODES:");
  Serial.println("  M     - LIVE MONITOR (display force)");
  Serial.println("  1-8   - NORMAL TEST (single speed)");
  Serial.println("  A     - AUTO SEQUENCE (all 8 speeds)");
  Serial.println("  C     - CALIBRATION");
  Serial.println();
  Serial.println("UTILITIES:");
  Serial.println("  T     - Tare (zero force)");
  Serial.println("  0     - Set home position");
  Serial.println("  H     - Go home");
  Serial.println("  Q     - Return to IDLE (from any mode)");
  Serial.println();
  Serial.println("========================================");
  Serial.println();
}

// =====================================================
// MODE: LIVE MONITOR
// =====================================================
void handleLiveMonitorMode() {
  // Motor stopped, continuously display force
  
  LoadCell.update();
  unsigned long currentTime = millis();
  
  if (currentTime - lastMonitorDisplayTime >= MONITOR_UPDATE_INTERVAL) {
    float force = getFilteredForce();
    
    // Display on single line with overwrite
    Serial.print("\rForce: ");
    Serial.print(force, 5);
    Serial.print(" N  ");  // Padding to overwrite old values
    
    lastMonitorDisplayTime = currentTime;
  }
}

void enterLiveMonitorMode() {
  currentMode = MODE_LIVE_MONITOR;
  motorState = MOTOR_IDLE;
  isStreaming = false;
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("LIVE FORCE MONITOR MODE");
  Serial.println("========================================");
  Serial.println("Motor is paused.");
  Serial.println("Force updated every 200 ms.");
  Serial.println("Press 'Q' to exit and return to IDLE.");
  Serial.println();
  
  lastMonitorDisplayTime = millis();
}

void exitLiveMonitorMode() {
  Serial.println("\n\nExiting Live Monitor Mode...");
  currentMode = MODE_IDLE;
  motorState = MOTOR_IDLE;
  printMainMenu();
}

// =====================================================
// MODE: NORMAL TEST (Single Test)
// =====================================================
void handleNormalTestMode() {
  LoadCell.update();
  
  switch (motorState) {
    case MOTOR_IDLE:
      // Should not happen, but safety fallback
      break;
      
    case MOTOR_MOVING_DOWN:
      handleMovingDown();
      break;
      
    case MOTOR_RETURNING_HOME:
      handleReturningHome();
      break;
  }
}

void enterNormalTestMode(int testIndex) {
  if (testIndex < 0 || testIndex >= 8) {
    Serial.println("✗ Invalid test index");
    return;
  }
  
  currentMode = MODE_NORMAL_TEST;
  motorState = MOTOR_MOVING_DOWN;
  currentTestIndex = testIndex;
  SpeedTest test = tests[testIndex];
  
  Serial.println();
  Serial.println("=====================================");
  Serial.print("TEST: ");
  Serial.println(test.name);
  Serial.println("=====================================");
  Serial.print("Speed: ");
  Serial.print(test.speed_mms * 1000, 3);
  Serial.println(" µm/s");
  Serial.println("Sampling: every 5 µm");
  Serial.println();
  
  contactDetected = false;
  lastSampledPosition = homePosition;
  
  targetPosition = homePosition + testDistance;
  stepper.setMaxSpeed(test.speed);
  stepper.setAcceleration(0);  // Constant velocity
  stepper.moveTo(targetPosition);
  
  startStreaming();
  Serial.println(">>> Moving DOWN (position-based sampling)...");
}

void exitNormalTestMode() {
  stopStreaming();
  currentMode = MODE_IDLE;
  motorState = MOTOR_IDLE;
  isStreaming = false;
  Serial.println();
  Serial.println("Exiting Normal Test Mode...");
  printMainMenu();
}

// =====================================================
// MODE: AUTO SEQUENCE (Run All Tests)
// =====================================================
void handleAutoSequenceMode() {
  LoadCell.update();
  
  switch (motorState) {
    case MOTOR_IDLE:
      // Auto sequence idle state - wait for next test setup
      if (nextAutoTestIndex < 8) {
        setupNextAutoTest();
      } else {
        // All tests complete
        Serial.println();
        Serial.println("=====================================");
        Serial.println("🎉 AUTO SEQUENCE COMPLETE!");
        Serial.println("All 8 speeds tested.");
        Serial.println("=====================================");
        exitAutoSequenceMode();
      }
      break;
      
    case MOTOR_MOVING_DOWN:
      handleMovingDown();
      break;
      
    case MOTOR_RETURNING_HOME:
      handleReturningHome();
      break;
  }
}

void enterAutoSequenceMode() {
  currentMode = MODE_AUTO_SEQUENCE;
  motorState = MOTOR_IDLE;
  nextAutoTestIndex = 0;
  
  Serial.println();
  Serial.println("=====================================");
  Serial.println("🚀 AUTO SEQUENCE MODE");
  Serial.println("Running all 8 speeds automatically");
  Serial.println("=====================================");
  Serial.println();
}

void setupNextAutoTest() {
  if (nextAutoTestIndex >= 8) return;
  
  int testIndex = nextAutoTestIndex;
  currentTestIndex = testIndex;
  SpeedTest test = tests[testIndex];
  
  Serial.println();
  Serial.print("━━ TEST ");
  Serial.print(nextAutoTestIndex + 1);
  Serial.print("/8: ");
  Serial.println(test.name);
  Serial.print("Speed: ");
  Serial.print(test.speed_mms * 1000, 3);
  Serial.println(" µm/s");
  
  contactDetected = false;
  lastSampledPosition = homePosition;
  
  motorState = MOTOR_MOVING_DOWN;
  targetPosition = homePosition + testDistance;
  stepper.setMaxSpeed(test.speed);
  stepper.setAcceleration(0);
  stepper.moveTo(targetPosition);
  
  startStreaming();
  nextAutoTestIndex++;
}

void exitAutoSequenceMode() {
  currentMode = MODE_IDLE;
  motorState = MOTOR_IDLE;
  isStreaming = false;
  nextAutoTestIndex = 0;
  Serial.println();
  printMainMenu();
}

// =====================================================
// MOTOR STATE HANDLERS (Shared by NORMAL_TEST and AUTO_SEQUENCE)
// =====================================================
void handleMovingDown() {
  stepper.run();
  
  // POSITION-BASED SAMPLING
  long currentPosition = stepper.currentPosition();
  long positionDelta = abs(currentPosition - lastSampledPosition);
  
  if (positionDelta >= POSITION_SAMPLE_INTERVAL) {
    float currentForce = getFilteredForce();
    lastForce = currentForce;
    lastSampledPosition = currentPosition;
    
    // Detect surface contact
    if (!contactDetected && currentForce > CONTACT_THRESHOLD) {
      contactDetected = true;
      Serial.println(">>> CONTACT DETECTED!");
    }
    
    // Safety overload check
    if (currentForce > OVERLOAD_LIMIT) {
      Serial.println("⚠️ OVERLOAD DETECTED!");
      emergencyStop = true;
      return;
    }
    
    // Stream data point
    if (isStreaming) {
      streamDataPoint(currentForce, currentPosition);
    }
  }
  
  // Check if target reached
  if (stepper.distanceToGo() == 0) {
    stopStreaming();
    motorState = MOTOR_RETURNING_HOME;
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
    Serial.println();
    
    motorState = MOTOR_IDLE;
    
    // Return to appropriate parent mode
    if (currentMode == MODE_NORMAL_TEST) {
      exitNormalTestMode();
    } else if (currentMode == MODE_AUTO_SEQUENCE) {
      // Stay in auto sequence mode; will trigger next test
    }
  }
}

void handleEmergencyStop() {
  stepper.stop();
  stepper.setSpeed(0);
  stopStreaming();
  
  Serial.println("❌ EMERGENCY STOP ACTIVE");
  Serial.println("Press 'R' to reset");
  
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'R' || cmd == 'r') {
      emergencyStop = false;
      motorState = MOTOR_IDLE;
      Serial.println("✓ System reset");
    }
  }
}

// =====================================================
// SERIAL COMMAND HANDLING
// =====================================================
void handleSerialCommand() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    while (Serial.available() > 0) Serial.read();
    
    // Exit command (Q) is always available
    if (cmd == 'Q' || cmd == 'q') {
      if (currentMode != MODE_IDLE) {
        stopStreaming();
        stepper.stop();
        motorState = MOTOR_IDLE;
        currentMode = MODE_IDLE;
        Serial.println();
        Serial.println("Returning to IDLE...");
        printMainMenu();
      }
      return;
    }
    
    // During tests, only allow stop and mode commands
    if ((motorState != MOTOR_IDLE) && 
        !(cmd == 'S' || cmd == 's' || cmd == 'R' || cmd == 'r')) {
      return;
    }
    
    // Command dispatcher
    if (cmd >= '1' && cmd <= '8') {
      // Enter normal test mode
      int testIndex = cmd - '1';
      enterNormalTestMode(testIndex);
    } else {
      switch (cmd) {
        case 'M':
        case 'm':
          enterLiveMonitorMode();
          break;
          
        case 'A':
        case 'a':
          enterAutoSequenceMode();
          break;
          
        case 'C':
        case 'c':
          currentMode = MODE_CALIBRATION;
          break;
          
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
          if (motorState != MOTOR_IDLE) {
            stepper.stop();
            stopStreaming();
            motorState = MOTOR_IDLE;
            Serial.println("❌ TEST STOPPED");
          }
          break;
      }
    }
  }
}

// =====================================================
// DATA STREAMING
// =====================================================
void startStreaming() {
  isStreaming = true;
  startTime = millis();
  lastSampledPosition = stepper.currentPosition();
  Serial.println("START_STREAM");
}

void stopStreaming() {
  if (!isStreaming) return;
  isStreaming = false;
  Serial.println("END_STREAM");
}

void streamDataPoint(float force_N, long motorSteps) {
  float time_sec = (millis() - startTime) / 1000.0;
  float position_mm = motorSteps / 1600.0;
  float speed_mms = tests[currentTestIndex].speed_mms;
  
  Serial.print("{\"t\":");
  Serial.print(time_sec, 3);
  Serial.print(",\"p\":");
  Serial.print(position_mm, 4);
  Serial.print(",\"s\":");
  Serial.print(speed_mms, 6);
  Serial.print(",\"f\":");
  Serial.print(force_N, 5);
  Serial.println("}");
}

// =====================================================
// MODE: CALIBRATION (Existing Code)
// =====================================================
void handleCalibrationMode() {
  Serial.println();
  Serial.println("CALIBRATION MODE ACTIVE");
  Serial.println("Motor is paused. Position-based sampling is disabled.");
  Serial.println();
  
  // Step 1: Tare
  Serial.println("Step 1: REMOVE ALL WEIGHT from the load cell");
  Serial.println("Waiting for user input...");
  Serial.println("Send: 'T' to tare");
  
  bool taredOK = false;
  unsigned long tareTimeout = millis() + 60000;
  
  while (!taredOK && millis() < tareTimeout) {
    LoadCell.update();
    
    if (Serial.available() > 0) {
      char cmd = Serial.read();
      while (Serial.available() > 0) Serial.read();
      
      if (cmd == 'T' || cmd == 't') {
        Serial.println(">>> Taring...");
        LoadCell.tareNoDelay();
        taredOK = true;
      }
    }
  }
  
  if (!taredOK) {
    Serial.println("✗ Tare timeout. Exiting calibration.");
    exitCalibrationMode();
    return;
  }
  
  Serial.println("Waiting for tare to complete...");
  unsigned long tare_start = millis();
  while (!LoadCell.getTareStatus() && millis() - tare_start < 5000) {
    LoadCell.update();
    delay(10);
  }
  
  if (!LoadCell.getTareStatus()) {
    Serial.println("✗ Tare failed. Exiting calibration.");
    exitCalibrationMode();
    return;
  }
  
  Serial.println("✓ Tare complete!");
  Serial.println();
  
  // Step 2: Get known mass
  Serial.println("Step 2: ENTER KNOWN CALIBRATION MASS");
  Serial.println("Please enter mass in grams (e.g., 100)");
  Serial.println("Waiting for input...");
  
  float knownMass = getUserInputMass(60000);
  
  if (knownMass < 0) {
    Serial.println("✗ Invalid input. Exiting calibration.");
    exitCalibrationMode();
    return;
  }
  
  Serial.print("Calibration mass entered: ");
  Serial.print(knownMass, 1);
  Serial.println(" grams");
  Serial.println();
  
  // Step 3: Compute factor
  Serial.println("Step 3: PLACE KNOWN MASS ON LOAD CELL");
  Serial.println("Waiting for stable reading...");
  
  delay(2000);
  
  float newCalFactor = LoadCell.getNewCalibration(knownMass);
  
  Serial.println();
  Serial.println("=====================================");
  Serial.print("NEW CALIBRATION FACTOR: ");
  Serial.println(newCalFactor);
  Serial.println("=====================================");
  Serial.println();
  
  // Step 4: Save
  Serial.println("Saving calibration to EEPROM...");
  calibrationFactor = newCalFactor;
  LoadCell.setCalFactor(newCalFactor);
  saveCalibrationToEEPROM();
  
  Serial.println("✓ Calibration complete and saved!");
  Serial.println();
  
  exitCalibrationMode();
}

void exitCalibrationMode() {
  Serial.println("Exiting calibration mode...");
  Serial.println("Motor is re-enabled. Ready for normal measurement.");
  Serial.println();
  
  currentMode = MODE_IDLE;
  motorState = MOTOR_IDLE;
  resetFilters();
  printMainMenu();
}

float getUserInputMass(unsigned long timeout) {
  String inputString = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeout) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        if (inputString.length() > 0) {
          float mass = inputString.toFloat();
          if (mass > 0) {
            return mass;
          } else {
            Serial.println("✗ Invalid value. Please enter a positive number.");
            inputString = "";
            return -1;
          }
        }
      } else if (isDigit(c) || c == '.') {
        inputString += c;
        Serial.write(c);
      }
    }
  }
  
  Serial.println("\n✗ Timeout waiting for input.");
  return -1;
}
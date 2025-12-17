/*
 * Automatic Speed Test System (FULLY FIXED)
 * Arduino Mega 2560
 * 
 * แก้ไข:
 * 1. เพิ่มความเร็ว ULTRA_FAST กลับมา (รวม 8 ความเร็ว)
 * 2. ✅ แก้ logic กลับ Home ให้ทำงานอัตโนมัติ (FIX: ใช้ moveTo แทน runSpeed)
 */

#include <Wire.h>
#include <AccelStepper.h>

// ===== Motor Pins =====
#define EN_PIN 7
#define STEP_PIN 6
#define DIR_PIN 5

// ===== MPU-6500 =====
const int MPU_ADDR = 0x68;
int16_t AcX, AcY, AcZ;
float baseline_magnitude = 0;

// ===== Motor Settings =====
AccelStepper stepper(1, STEP_PIN, DIR_PIN);
const long testDistance = 56000;  // 35mm
const float homeSpeed = 3200;
const float homeAccel = 1600;

// ===== Auto Test System =====
bool autoTestMode = false;
int currentTestIndex = 0;
enum TestState { IDLE, MOVING, RETURNING, WAITING };
TestState testState = IDLE;

// ⭐ 8 ความเร็วทดสอบ (เพิ่ม ULTRA_FAST กลับมา)
struct SpeedTest {
  const char* name;
  float speed;
  float speed_mms;
};

SpeedTest tests[8] = {
  {"ULTRA_FAST",  1600,  1000.0},    // 1000 mm/s (เร็วที่สุด!)
  {"FAST_UP",     1200,  750.0},     // ✅ เปลี่ยนเป็น 750 mm/s
  {"FAST_DOWN",   400,   250.0},
  {"MEASURE_F",   50.0,  0.03125},
  {"MEASURE_M",   20.0,  0.0125},
  {"MEASURE_U",   10.0,  0.00625},
  {"MEASURE_X",   5.0,   0.003125},
  {"MEASURE_Z",   2.0,   0.00125}
};

long homePosition = 0;
long targetPosition = 0;  // ✅ ตัวแปรใหม่ สำหรับ MOVING state

// ===== Data Logging =====
bool isLogging = false;
unsigned long startTime = 0;
unsigned long lastLogTime = 0;
const int LOG_INTERVAL = 20;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  
  stepper.setMaxSpeed(homeSpeed);
  stepper.setAcceleration(homeAccel);
  
  // Initialize MPU-6500
  Wire.begin();
  Wire.setClock(400000);
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission(true);
  
  delay(100);
  
  // Calibrate
  Serial.println("=== Auto Speed Test System ===");
  Serial.println("Calibrating...");
  
  float sum = 0;
  for(int i = 0; i < 100; i++) {
    readAccel();
    float mag = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
    sum += mag;
    delay(20);
  }
  baseline_magnitude = sum / 100.0;
  
  Serial.println("Ready!");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  A = Start AUTO TEST (8 speeds)");
  Serial.println("  S = Stop test");
  Serial.println("  0 = Set HOME position");
  Serial.println("  H = Go HOME manually");
  Serial.println();
  Serial.println("Test sequence (8 speeds):");
  Serial.println("  1. ULTRA_FAST (1000 mm/s)");
  Serial.println("  2. FAST_UP (250 mm/s)");
  Serial.println("  3. FAST_DOWN (250 mm/s)");
  Serial.println("  4. MEASURE_F (31.25 µm/s)");
  Serial.println("  5. MEASURE_M (12.5 µm/s)");
  Serial.println("  6. MEASURE_U (6.25 µm/s)");
  Serial.println("  7. MEASURE_X (3.125 µm/s)");
  Serial.println("  8. MEASURE_Z (1.25 µm/s)");
  Serial.println("================");
}

void loop() {
  // ===== Commands =====
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    while (Serial.available() > 0) Serial.read();
    
    if (cmd == 'A' || cmd == 'a') {
      startAutoTest();
    }
    else if (cmd == 'S' || cmd == 's') {
      stopAutoTest();
    }
    else if (cmd == '0') {
      homePosition = stepper.currentPosition();
      Serial.println(">>> HOME SET");
      Serial.print("Position: ");
      Serial.print(homePosition);
      Serial.print(" (");
      Serial.print(homePosition / 1600.0);
      Serial.println(" mm)");
    }
    else if (cmd == 'H' || cmd == 'h') {
      if (!autoTestMode) {
        goHomeManual();
      }
    }
  }
  
  // ===== Auto Test State Machine =====
  if (autoTestMode) {
    switch (testState) {
      case MOVING:
        // ✅ FIX: ใช้ moveTo แทน runSpeed เพื่อให้เช็คระยะทางอัติโนมัติ
        stepper.run();
        
        // เช็คว่าเดินครบ 35mm แล้วหรือยัง
        if (stepper.distanceToGo() == 0) {
          // หยุดบันทึกข้อมูล
          stopLogging();
          
          Serial.println(">>> Target reached!");
          Serial.print("Position: ");
          Serial.print(stepper.currentPosition());
          Serial.print(" (");
          Serial.print(stepper.currentPosition() / 1600.0);
          Serial.println(" mm)");
          
          delay(500);
          
          // ✅ เปลี่ยนสถานะเป็น RETURNING
          testState = RETURNING;
          
          // ตั้งค่ากลับ Home
          stepper.setMaxSpeed(homeSpeed);
          stepper.setAcceleration(homeAccel);
          stepper.moveTo(homePosition);
          
          Serial.println(">>> Returning HOME...");
          Serial.print("From: ");
          Serial.print(stepper.currentPosition());
          Serial.print(" -> To: ");
          Serial.println(homePosition);
        }
        break;
        
      case RETURNING:
        // ✅ ใช้ run() สำหรับกลับ Home (ใช้ acceleration)
        stepper.run();
        
        // Debug ทุก 1 วินาที
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 1000) {
          Serial.print("Returning... Pos: ");
          Serial.print(stepper.currentPosition());
          Serial.print(" / Target: ");
          Serial.print(homePosition);
          Serial.print(" | Distance: ");
          Serial.print(abs(stepper.distanceToGo()));
          Serial.println(" steps");
          lastDebug = millis();
        }
        
        // ✅ เช็คว่าถึง Home แล้ว
        if (stepper.distanceToGo() == 0) {
          Serial.println("✓ HOME reached");
          Serial.print("Final position: ");
          Serial.print(stepper.currentPosition());
          Serial.print(" (");
          Serial.print(stepper.currentPosition() / 1600.0);
          Serial.println(" mm)");
          Serial.println();
          
          currentTestIndex++;
          
          if (currentTestIndex >= 8) {
            // เสร็จทั้งหมด
            autoTestMode = false;
            testState = IDLE;
            Serial.println("=====================================");
            Serial.println("🎉 ALL 8 TESTS COMPLETED!");
            Serial.println("=====================================");
          } else {
            // เริ่ม test ถัดไป
            testState = WAITING;
            delay(2000);
            startNextTest();
          }
        }
        break;
        
      case WAITING:
        // รอ...
        break;
        
      default:
        break;
    }
  }
  
  // ===== Data Logging =====
  if (isLogging) {
    unsigned long currentTime = millis();
    if (currentTime - lastLogTime >= LOG_INTERVAL) {
      lastLogTime = currentTime;
      logData();
    }
  }
}

void startAutoTest() {
  if (autoTestMode) {
    Serial.println("⚠️ Test already running!");
    return;
  }
  
  Serial.println("=====================================");
  Serial.println("🚀 STARTING AUTO TEST");
  Serial.println("Testing 8 speeds × 35mm each");
  Serial.println("=====================================");
  
  autoTestMode = true;
  currentTestIndex = 0;
  startNextTest();
}

void startNextTest() {
  SpeedTest test = tests[currentTestIndex];
  
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.print("Test ");
  Serial.print(currentTestIndex + 1);
  Serial.print("/8: ");
  Serial.println(test.name);
  Serial.print("Speed: ");
  Serial.print(test.speed_mms, 6);
  Serial.print(" mm/s (");
  Serial.print(test.speed);
  Serial.println(" steps/s)");
  Serial.println("Distance: 35 mm");
  Serial.print("Current position: ");
  Serial.print(stepper.currentPosition());
  Serial.print(" (");
  Serial.print(stepper.currentPosition() / 1600.0);
  Serial.println(" mm)");
  
  // ✅ FIX: ใช้ moveTo เพื่อให้เคลื่อนไป 35mm อัติโนมัติ
  targetPosition = homePosition + testDistance;
  stepper.setMaxSpeed(test.speed);
  stepper.setAcceleration(0);  // ตั้ง acceleration = 0 เพื่อให้เคลื่อนด้วยความเร็วคงที่
  stepper.moveTo(targetPosition);
  
  // เริ่มบันทึก
  startLogging();
  
  testState = MOVING;
  Serial.println(">>> Moving...");
}

void stopAutoTest() {
  if (!autoTestMode) {
    Serial.println("⚠️ No test running!");
    return;
  }
  
  autoTestMode = false;
  testState = IDLE;
  stopLogging();
  stepper.stop();
  
  Serial.println();
  Serial.println("❌ TEST STOPPED");
}

void goHomeManual() {
  stepper.setMaxSpeed(homeSpeed);
  stepper.setAcceleration(homeAccel);
  stepper.moveTo(homePosition);
  Serial.println(">>> Going HOME manually...");
  
  // รอจนกว่าจะถึง Home
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  
  Serial.println("✓ Manual HOME reached");
}

void startLogging() {
  isLogging = true;
  startTime = millis();
  lastLogTime = startTime;
  
  Serial.println("DATA_START");
  Serial.println("Time(sec),MotorPos(mm),MotorSpeed(mm/s),Vibration(m/s²),TestName");
}

void stopLogging() {
  if (!isLogging) return;
  
  isLogging = false;
  Serial.println("DATA_END");
  Serial.println("✓ Test completed");
}

void logData() {
  readAccel();
  float magnitude = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
  float vibration_raw = abs(magnitude - baseline_magnitude);
  if(vibration_raw < 50) vibration_raw = 0;
  float vibration_mss = (vibration_raw / 4096.0) * 9.81;
  
  float time_sec = (millis() - startTime) / 1000.0;
  float motor_pos_mm = stepper.currentPosition() / 1600.0;
  float motor_speed_mms = tests[currentTestIndex].speed_mms;
  
  Serial.print(time_sec, 3);
  Serial.print(",");
  Serial.print(motor_pos_mm, 4);
  Serial.print(",");
  Serial.print(motor_speed_mms, 6);
  Serial.print(",");
  Serial.print(vibration_mss, 4);
  Serial.print(",");
  Serial.println(tests[currentTestIndex].name);
}

void readAccel() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
}

/*
 * การใช้งาน:
 * 1. กด 0 เพื่อ Set HOME
 * 2. กด A เพื่อเริ่มทดสอบ 8 ความเร็ว (กลับอัติโนมัติ ✅)
 * 3. กด S เพื่อหยุดกลางคัน
 * 4. กด H เพื่อกลับ Home ด้วยตนเอง
 * 
 * ⏱️ เวลาทดสอบทั้งหมด (ประมาณ):
 * - ULTRA_FAST: ~0.04 วินาที
 * - FAST_UP/DOWN: ~0.14 วินาที × 2
 * - MEASURE_F: ~19 นาที
 * - MEASURE_M: ~47 นาที
 * - MEASURE_U: ~93 นาที
 * - MEASURE_X: ~187 นาที
 * - MEASURE_Z: ~467 นาที
 * รวม: ~814 นาที (13.5 ชั่วโมง)
 */
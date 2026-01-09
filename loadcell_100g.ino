/*
   -------------------------------------------------------------------------------------
   HX711_ADC - Enhanced Version
   ปรับปรุงความแม่นยำและเสถียรภาพในการอ่านน้ำหนัก
   -------------------------------------------------------------------------------------
*/

#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// ========== การตั้งค่าพิน ==========
const int HX711_dout = 3;
const int HX711_sck = 2;

// ========== HX711 Constructor ==========
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// ========== ค่าคงที่สำหรับการปรับปรุง ==========
const int calVal_eepromAdress = 0;
const int SAMPLE_SIZE = 10;              // จำนวนตัวอย่างสำหรับ moving average
const float STABILITY_THRESHOLD = 0.5;   // ค่า threshold สำหรับตรวจสอบความเสถียร (กรัม)
const float NOISE_FILTER = 0.1;          // กรองสัญญาณรบกวนที่น้อยกว่าค่านี้ (กรัม)
const unsigned long PRINT_INTERVAL = 100; // อัพเดตทุก 100ms

// ========== ตัวแปรสำหรับ Moving Average Filter ==========
float readings[SAMPLE_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;

// ========== ตัวแปรสำหรับตรวจสอบความเสถียร ==========
float lastStableValue = 0;
unsigned long lastChangeTime = 0;
bool isStable = false;

// ========== ตัวแปรทั่วไป ==========
unsigned long t = 0;
float previousWeight = 0;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("========================================");
  Serial.println("HX711 Enhanced Weight Scale");
  Serial.println("========================================");
  
  // เริ่มต้น LoadCell
  LoadCell.begin();
  
  // ตั้งค่า calibration value
  float calibrationValue = 16800.0;
  
  #if defined(ESP8266)|| defined(ESP32)
  //EEPROM.begin(512);
  #endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue);
  
  // เวลาในการ stabilize (เพิ่มเป็น 3 วินาที)
  unsigned long stabilizingtime = 3000;
  boolean _tare = true;
  
  Serial.println("Initializing scale...");
  LoadCell.start(stabilizingtime, _tare);
  
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("ERROR: Timeout!");
    Serial.println("Check wiring: MCU>HX711");
    Serial.println("DOUT -> Pin 3");
    Serial.println("SCK  -> Pin 2");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue);
    Serial.println("Startup complete!");
    Serial.println("========================================");
    Serial.println("Commands:");
    Serial.println("  't' - Tare (zero the scale)");
    Serial.println("  'c' - Calibrate");
    Serial.println("========================================\n");
  }
  
  // เริ่มต้นค่า moving average
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    readings[i] = 0;
  }
}

void loop() {
  static boolean newDataReady = 0;

  // ตรวจสอบข้อมูลใหม่
  if (LoadCell.update()) newDataReady = true;

  // ประมวลผลข้อมูล
  if (newDataReady) {
    if (millis() > t + PRINT_INTERVAL) {
      
      // อ่านค่าดิบจาก LoadCell
      float rawValue = LoadCell.getData();
      
      // ใช้ Moving Average Filter
      float filteredValue = movingAverageFilter(rawValue);
      
      // กรองสัญญาณรบกวนเล็กน้อย
      filteredValue = noiseFilter(filteredValue);
      
      // ตรวจสอบความเสถียร
      checkStability(filteredValue);
      
      // แสดงผล
      displayWeight(filteredValue);
      
      newDataReady = 0;
      t = millis();
    }
  }

  // รับคำสั่งจาก Serial
  handleSerialCommand();

  // ตรวจสอบสถานะ Tare
  if (LoadCell.getTareStatus() == true) {
    Serial.println("✓ Tare complete - Scale zeroed");
    resetFilters();
  }
}

// ========== ฟังก์ชัน Moving Average Filter ==========
float movingAverageFilter(float newReading) {
  // ลบค่าเก่าออกจาก total
  total = total - readings[readIndex];
  // เพิ่มค่าใหม่
  readings[readIndex] = newReading;
  total = total + readings[readIndex];
  // เลื่อน index
  readIndex = (readIndex + 1) % SAMPLE_SIZE;
  // คำนวณค่าเฉลี่ย
  average = total / SAMPLE_SIZE;
  return average;
}

// ========== ฟังก์ชันกรองสัญญาณรบกวน ==========
float noiseFilter(float value) {
  // ถ้าค่าเปลี่ยนแปลงน้อยกว่า threshold ให้ถือว่าไม่เปลี่ยน
  if (abs(value - previousWeight) < NOISE_FILTER) {
    return previousWeight;
  }
  previousWeight = value;
  return value;
}

// ========== ฟังก์ชันตรวจสอบความเสถียร ==========
void checkStability(float value) {
  if (abs(value - lastStableValue) > STABILITY_THRESHOLD) {
    // น้ำหนักเปลี่ยนแปลง
    lastStableValue = value;
    lastChangeTime = millis();
    isStable = false;
  }
  else if (!isStable && (millis() - lastChangeTime > 1000)) {
    // น้ำหนักคงที่เกิน 1 วินาที
    isStable = true;
  }
}

// ========== ฟังก์ชันแสดงผล ==========
void displayWeight(float weight) {
  Serial.print("Weight: ");
  Serial.print(weight, 2);
  Serial.print(" g");
  
  // แสดงสถานะเสถียรภาพ
  if (isStable) {
    Serial.print("  [STABLE]");
  } else {
    Serial.print("  [...]");
  }
  
  // แสดงค่าดิบสำหรับ debug (ถ้าต้องการ)
  // Serial.print("  (Raw: ");
  // Serial.print(LoadCell.getData(), 2);
  // Serial.print(")");
  
  Serial.println();
}

// ========== ฟังก์ชันจัดการคำสั่ง Serial ==========
void handleSerialCommand() {
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    
    if (inByte == 't' || inByte == 'T') {
      Serial.println("\n→ Taring scale...");
      LoadCell.tareNoDelay();
    }
    else if (inByte == 'c' || inByte == 'C') {
      Serial.println("\n→ Starting calibration mode...");
      calibrate();
    }
  }
}

// ========== ฟังก์ชัน Reset Filters ==========
void resetFilters() {
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    readings[i] = 0;
  }
  total = 0;
  average = 0;
  previousWeight = 0;
  lastStableValue = 0;
  isStable = false;
}

// ========== ฟังก์ชัน Calibration ==========
void calibrate() {
  Serial.println("========================================");
  Serial.println("CALIBRATION MODE");
  Serial.println("========================================");
  Serial.println("1. Remove all weight from scale");
  Serial.println("2. Send 't' to tare");
  Serial.println("3. Place known weight on scale");
  Serial.println("4. Send weight value in grams (e.g., '500')");
  Serial.println("========================================");
  
  // รอคำสั่งจากผู้ใช้
  boolean calibrationComplete = false;
  String inputString = "";
  
  while (!calibrationComplete) {
    if (LoadCell.update()) {
      // อ่านค่าอย่างต่อเนื่อง
      float rawValue = LoadCell.getData();
      Serial.print("Current raw value: ");
      Serial.println(rawValue, 2);
    }
    
    if (Serial.available() > 0) {
      char inChar = Serial.read();
      
      if (inChar == 't') {
        LoadCell.tareNoDelay();
        Serial.println("✓ Tared");
      }
      else if (inChar == '\n') {
        if (inputString.length() > 0) {
          float knownWeight = inputString.toFloat();
          if (knownWeight > 0) {
            float newCalValue = LoadCell.getNewCalibration(knownWeight);
            Serial.print("New calibration value: ");
            Serial.println(newCalValue);
            Serial.println("Update calibrationValue in code with this value");
            calibrationComplete = true;
          }
          inputString = "";
        }
      }
      else if (isDigit(inChar) || inChar == '.') {
        inputString += inChar;
      }
    }
    
    delay(100);
  }
  
  Serial.println("Calibration complete!");
  Serial.println("========================================\n");
}
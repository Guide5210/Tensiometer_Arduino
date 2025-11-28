#include <AccelStepper.h>
// #include <TMCStepper.h> // ถ้าต้องการควบคุม TMC2209 ผ่าน UART

#define EN_PIN 7
#define STEP_PIN 6
#define DIR_PIN  5

// ถ้าใช้ UART กับ TMC2209 (ถ้าไม่ได้ใช้ ให้คอมเมนต์ทิ้ง)
// #define SERIAL_PORT Serial1  // Hardware Serial บน Mega
// #define DRIVER_ADDRESS 0b00  // TMC2209 Driver Address
// #define R_SENSE 0.11f        // R_SENSE ของ TMC2209 (ตรวจสอบจาก Datasheet)
// TMC2209Stepper driver(&SERIAL_PORT, R_SENSE, DRIVER_ADDRESS);

AccelStepper stepper(1, STEP_PIN, DIR_PIN);

// ระยะทางที่ต้องการเคลื่อนที่ (8000 steps = 5mm สำหรับ 1/8 microstep)
const long stepsPerMove = 8000;

// ตัวแปรสำหรับโหมดวัด
bool isMeasuring = false;

// *** ตัวแปรเก็บตำแหน่ง Home ***
long homePosition = 0; // ตำแหน่ง Home ที่ผู้ใช้กำหนด

// *** ตั้งค่าความเร็วต่างๆ ***
const float normalSpeed = 400;      // ความเร็วปกติ (t/y)
const float homeSpeed = 2000;       // ความเร็วกลับ Home (เร็วกว่า 5 เท่า)
const float normalAccel = 1600;     // Acceleration ปกติ
const float softAccel = 800;        // Soft Start/Stop (นิ่มกว่า)

// *** ปรับความเร็วโหมดวัดตรงนี้ ***
// +100 steps/sec = 0.0625 mm/s (เร็วมาตรฐาน) - สลับทิศทางแล้ว
// +50 steps/sec = 0.03125 mm/s (ช้ากว่า - แนะนำ)
// +20 steps/sec = 0.0125 mm/s (ช้ามาก - แม่นยำสูง)
// +10 steps/sec = 0.00625 mm/s (ช้าที่สุด - แม่นยำสูงสุด)
float measureSpeed = 20.0; // เปลี่ยนจาก -20 เป็น +20 สำหรับความแม่นยำสูง

void setup() {
  Serial.begin(9600);
  
  // *** ไม่รอ Serial เพื่อให้ทำงานได้ทันทีแม้ไม่เปิด Serial Monitor ***
  delay(1000); // รอ 1 วินาทีแทน
  
  Serial.println("=== System Ready ===");
  Serial.println("Commands:");
  Serial.println("  t = Move UP 5mm (Fast)");
  Serial.println("  y = Move DOWN 5mm (Fast)");
  Serial.println("  m = Measuring SLOW (0.0125 mm/s)");
  Serial.println("  f = Measuring FAST (0.03125 mm/s)");
  Serial.println("  u = Measuring ULTRA SLOW (0.00625 mm/s)");
  Serial.println("  x = Measuring EXTREME SLOW (0.003125 mm/s)");
  Serial.println("  z = Measuring SUPER SLOW (0.00125 mm/s)");
  Serial.println("  s = STOP");
  Serial.println("  h = Go to Home (Fast & Soft)");
  Serial.println("  0 (zero) = SET current position as Home");
  Serial.println("  p = Show current position");
  Serial.println("  r = Reset position to absolute 0");
  Serial.println("====================");

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW); // เปิด Driver

  // *** ถ้าใช้ TMC2209 ผ่าน UART (ไม่บังคับ) ***
  // SERIAL_PORT.begin(115200);
  // driver.begin();
  // driver.toff(5);                 // Enable driver
  // driver.rms_current(600);        // กระแส 600mA (ปรับตามมอเตอร์)
  // driver.microsteps(8);           // 1/8 microstepping
  // driver.pwm_autoscale(true);     // เปิด StealthChop
  // driver.en_spreadCycle(false);   // ปิด SpreadCycle (ใช้ StealthChop)

  // *** ใช้ค่าเดียวกับโค้ดที่ทำงานได้ ***
  // สำหรับการเคลื่อนที่แบบเร็ว (t/y)
  stepper.setMaxSpeed(normalSpeed);
  stepper.setAcceleration(normalAccel);
  
  // *** ถ้าต้องการให้โหมดปกติช้าลงด้วย ***
  // stepper.setMaxSpeed(200);  // ลองค่านี้ = 0.125 mm/s
  // stepper.setMaxSpeed(100);  // หรือค่านี้ = 0.0625 mm/s
  
  Serial.println("Motor Ready!");
  Serial.print("Max Speed: ");
  Serial.println(stepper.maxSpeed());
  Serial.print("Acceleration: ");
  Serial.println(stepper.acceleration());
}

void loop() {
  // รับคำสั่งจาก Serial
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    // ทำความสะอาด Buffer
    while (Serial.available() > 0) {
      Serial.read();
    }
    
    // ประมวลผลคำสั่ง
    if (cmd == 't' || cmd == 'T') {
      // หยุดโหมดวัดก่อน
      isMeasuring = false;
      
      // ตั้งค่าความเร็วปกติ
      stepper.setMaxSpeed(normalSpeed);
      stepper.setAcceleration(normalAccel);
      
      // คำนวณตำแหน่งใหม่ (ขึ้น) - สลับเครื่องหมาย
      long newTarget = stepper.currentPosition() - stepsPerMove; // เปลี่ยนจาก + เป็น -
      stepper.moveTo(newTarget);
      
      Serial.println(">>> Moving UP 5mm");
      Serial.print("From: ");
      Serial.print(stepper.currentPosition());
      Serial.print(" -> To: ");
      Serial.println(newTarget);
    }
    else if (cmd == 'y' || cmd == 'Y') {
      // หยุดโหมดวัดก่อน
      isMeasuring = false;
      
      // ตั้งค่าความเร็วปกติ
      stepper.setMaxSpeed(normalSpeed);
      stepper.setAcceleration(normalAccel);
      
      // คำนวณตำแหน่งใหม่ (ลง) - สลับเครื่องหมาย
      long newTarget = stepper.currentPosition() + stepsPerMove; // เปลี่ยนจาก - เป็น +
      stepper.moveTo(newTarget);
      
      Serial.println(">>> Moving DOWN 5mm");
      Serial.print("From: ");
      Serial.print(stepper.currentPosition());
      Serial.print(" -> To: ");
      Serial.println(newTarget);
    }
    else if (cmd == 'm' || cmd == 'M') {
      // โหมดวัดแบบช้า (แนะนำ)
      isMeasuring = true;
      measureSpeed = 20.0; // เปลี่ยนจาก -20 เป็น +20 (สลับทิศทาง)
      stepper.setSpeed(measureSpeed);
      
      Serial.println(">>> MEASURING MODE: SLOW");
      Serial.println("Speed: 0.0125 mm/s (Accurate)");
      Serial.println("(Press 's' to stop)");
    }
    else if (cmd == 'f' || cmd == 'F') {
      // โหมดวัดแบบเร็ว
      isMeasuring = true;
      measureSpeed = 50.0; // เปลี่ยนจาก -50 เป็น +50
      stepper.setSpeed(measureSpeed);
      
      Serial.println(">>> MEASURING MODE: FAST");
      Serial.println("Speed: 0.03125 mm/s");
      Serial.println("(Press 's' to stop)");
    }
    else if (cmd == 'u' || cmd == 'U') {
      // โหมดวัดแบบช้าที่สุด
      isMeasuring = true;
      measureSpeed = 10.0; // เปลี่ยนจาก -10 เป็น +10
      stepper.setSpeed(measureSpeed);
      
      Serial.println(">>> MEASURING MODE: ULTRA SLOW");
      Serial.println("Speed: 0.00625 mm/s (Most Accurate)");
      Serial.println("(Press 's' to stop)");
    }
    else if (cmd == 'x' || cmd == 'X') {
      // โหมดวัดแบบช้ามากพิเศษ
      isMeasuring = true;
      measureSpeed = 5.0; // เปลี่ยนจาก -5 เป็น +5
      stepper.setSpeed(measureSpeed);
      
      Serial.println(">>> MEASURING MODE: EXTREME SLOW");
      Serial.println("Speed: 0.003125 mm/s (2x Slower)");
      Serial.println("Time: ~5 min per 1mm");
      Serial.println("(Press 's' to stop)");
    }
    else if (cmd == 'z' || cmd == 'Z') {
      // โหมดวัดแบบช้าสุดๆ
      isMeasuring = true;
      measureSpeed = 2.0; // เปลี่ยนจาก -2 เป็น +2
      stepper.setSpeed(measureSpeed);
      
      Serial.println(">>> MEASURING MODE: SUPER SLOW");
      Serial.println("Speed: 0.00125 mm/s (5x Slower)");
      Serial.println("Time: ~13 min per 1mm");
      Serial.println("(Press 's' to stop)");
    }
    else if (cmd == 's' || cmd == 'S') {
      // หยุดทั้งหมด
      isMeasuring = false;
      
      // *** เลือกวิธีหยุด ***
      // วิธีที่ 1: หยุดทันที (แนะนำสำหรับการวัด)
      stepper.setCurrentPosition(stepper.currentPosition());
      
      // วิธีที่ 2: หยุดแบบมี deceleration (นิ่มนวล)
      // stepper.stop();
      
      Serial.println(">>> STOPPED IMMEDIATELY");
      Serial.print("Position: ");
      Serial.println(stepper.currentPosition());
    }
    else if (cmd == 'h' || cmd == 'H') {
      // กลับไป Home ด้วยความเร็วสูง + Soft Start/Stop
      isMeasuring = false;
      
      // *** ตั้งค่าความเร็วสูงและ Soft Acceleration ***
      stepper.setMaxSpeed(homeSpeed);      // เร็วกว่าปกติ 5 เท่า
      stepper.setAcceleration(softAccel);  // Soft Start/Stop
      
      stepper.moveTo(homePosition);
      
      Serial.println(">>> Returning to HOME (Fast & Soft)");
      Serial.print("From: ");
      Serial.print(stepper.currentPosition());
      Serial.print(" -> To: ");
      Serial.println(homePosition);
      
      long distance = abs(stepper.currentPosition() - homePosition);
      float estimatedTime = (float)distance / homeSpeed;
      Serial.print("Est. Time: ");
      Serial.print(estimatedTime);
      Serial.println(" sec");
    }
    else if (cmd == '0') {
      // ตั้งตำแหน่งปัจจุบันเป็น Home ใหม่
      homePosition = stepper.currentPosition();
      
      Serial.println(">>> HOME POSITION SET!");
      Serial.print("New Home = ");
      Serial.println(homePosition);
      Serial.println("(Press 'h' to return to this position)");
    }
    else if (cmd == 'p' || cmd == 'P') {
      // แสดงตำแหน่งปัจจุบัน
      Serial.println("--- Position Info ---");
      Serial.print("Current Position: ");
      Serial.print(stepper.currentPosition());
      Serial.print(" steps (");
      Serial.print((float)stepper.currentPosition() / 1600.0);
      Serial.println(" mm)");
      
      Serial.print("Home Position: ");
      Serial.print(homePosition);
      Serial.print(" steps (");
      Serial.print((float)homePosition / 1600.0);
      Serial.println(" mm)");
      
      Serial.print("Distance from Home: ");
      long distance = stepper.currentPosition() - homePosition;
      Serial.print(distance);
      Serial.print(" steps (");
      Serial.print((float)distance / 1600.0);
      Serial.println(" mm)");
      Serial.println("--------------------");
    }
    else if (cmd == 'r' || cmd == 'R') {
      // รีเซ็ตตำแหน่งเป็น 0 สัมบูรณ์
      stepper.setCurrentPosition(0);
      homePosition = 0;
      
      Serial.println(">>> POSITION RESET!");
      Serial.println("Current = 0, Home = 0");
    }
    else if (cmd == 10 || cmd == 13) {
      // เพิกเฉย newline/carriage return
    }
    else {
      Serial.print("Unknown: '");
      Serial.print(cmd);
      Serial.print("' (ASCII: ");
      Serial.print((int)cmd);
      Serial.println(")");
    }
  }

  // --- ควบคุมมอเตอร์ ---
  if (isMeasuring) {
    // โหมดวัด: ความเร็วคงที่
    stepper.runSpeed();
    
    // แสดงตำแหน่งทุก 2 วินาที
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
      Serial.print("Measuring... Pos: ");
      Serial.println(stepper.currentPosition());
      lastPrint = millis();
    }
  } else {
    // โหมดปกติ: มีความเร่ง
    stepper.run();
  }
  
  // แจ้งเตือนเมื่อเคลื่อนที่เสร็จ (เฉพาะโหมดปกติ)
  static bool wasRunning = false;
  if (!isMeasuring) {
    if (!wasRunning && stepper.isRunning()) {
      Serial.println("⏳ Moving...");
      wasRunning = true;
    }
    if (wasRunning && !stepper.isRunning()) {
      Serial.println("✓ Completed!");
      Serial.print("Position: ");
      Serial.println(stepper.currentPosition());
      wasRunning = false;
    }
  }
}
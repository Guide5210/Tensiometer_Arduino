#include <Wire.h>
#include <Adafruit_MLX90614.h>

// สร้างอ็อบเจกต์สำหรับเซนเซอร์
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

void setup() {
  Serial.begin(9600); // เริ่มต้น Serial Monitor
  Serial.println("Adafruit MLX90614 test");

  // เริ่มต้นการทำงานของเซนเซอร์
  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX sensor. Check wiring.");
    while (1); // ถ้าเชื่อมต่อไม่ได้ ให้ค้างไว้ตรงนี้
  }
  
  Serial.println("Sensor connected!");
}

void loop() {
  // อ่านค่าอุณหภูมิรอบข้าง (Ambient)
  Serial.print("Ambient: "); 
  Serial.print(mlx.readAmbientTempC()); 
  Serial.print(" C\t");
  
  // อ่านค่าอุณหภูมิวัตถุ (Object)
  Serial.print("Object: "); 
  Serial.print(mlx.readObjectTempC()); 
  Serial.println(" C");

  // ถ้าต้องการหน่วยฟาเรนไฮต์ ให้ใช้ .readAmbientTempF() และ .readObjectTempF()

  delay(500); // หน่วงเวลา 0.5 วินาที ก่อนวัดครั้งถัดไป
}
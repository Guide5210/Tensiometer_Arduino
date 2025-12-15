/*
 * MPU-6500 Vibration Monitor for Linear Motor
 * Arduino Mega 2560
 * 
 * Wiring:
 * MPU-6500 VCC -> 3.3V
 * MPU-6500 GND -> GND
 * MPU-6500 SCL -> Pin 21 (SCL)
 * MPU-6500 SDA -> Pin 20 (SDA)
 * 
 * Output: CSV format เหมาะสำหรับทำกราฟ
 * Format: Time(sec), Vibration(m/s²)
 */

#include <Wire.h>

const int MPU_ADDR = 0x68;

// ข้อมูล Accelerometer (raw values)
int16_t AcX, AcY, AcZ;

// ค่า baseline สำหรับ calibration
float baseline_magnitude = 0;

// เวลา
unsigned long startTime = 0;
unsigned long lastReadTime = 0;
const int READ_INTERVAL = 20; // อ่านทุก 20ms (50Hz)

// ตัวแปรควบคุมการทำงาน
bool isRunning = false;
bool isPaused = false;

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  
  Wire.begin();
  Wire.setClock(100000); // 100kHz I2C
  
  Serial.println("=== Vibration Monitor ===");
  
  // Wake up MPU-6500
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0);
  Wire.endTransmission(true);
  
  // Configure Accelerometer (±8g range)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);  // ACCEL_CONFIG
  Wire.write(0x10);  // ±8g
  Wire.endTransmission(true);
  
  delay(100);
  
  // Calibrate - วัดค่าเฉลี่ยเมื่อไม่มีการสั่น
  Serial.println("Calibrating... Keep sensor still for 2 seconds!");
  float sum = 0;
  for(int i = 0; i < 100; i++) {
    readAccel();
    float mag = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
    sum += mag;
    delay(20);
  }
  baseline_magnitude = sum / 100.0;
  
  Serial.println("Calibration complete!");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s = Start recording");
  Serial.println("  p = Pause");
  Serial.println("  r = Resume");
  Serial.println("  x = Stop and reset");
  Serial.println();
  Serial.println("Send 's' to start...");
}

void loop() {
  // ตรวจสอบคำสั่งจาก Serial
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    if (cmd == 's' || cmd == 'S') {
      if (!isRunning) {
        isRunning = true;
        isPaused = false;
        startTime = millis();
        Serial.println();
        Serial.println("Time(sec),Vibration(m/s²)");
        Serial.println("-------------------------");
      }
    }
    else if (cmd == 'p' || cmd == 'P') {
      if (isRunning && !isPaused) {
        isPaused = true;
        Serial.println();
        Serial.println("=== PAUSED ===");
        Serial.println("Send 'r' to resume or 'x' to stop");
      }
    }
    else if (cmd == 'r' || cmd == 'R') {
      if (isPaused) {
        isPaused = false;
        Serial.println("=== RESUMED ===");
      }
    }
    else if (cmd == 'x' || cmd == 'X') {
      if (isRunning) {
        isRunning = false;
        isPaused = false;
        Serial.println();
        Serial.println("=== STOPPED ===");
        Serial.println("Send 's' to start again");
      }
    }
  }
  
  // บันทึกข้อมูลถ้าอยู่ในโหมดทำงาน
  if (!isRunning || isPaused) {
    return;
  }
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentTime;
    
    // อ่านค่า Accelerometer
    readAccel();
    
    // คำนวณขนาดของ acceleration vector
    float magnitude = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
    
    // คำนวณการสั่น = ส่วนต่างจาก baseline
    float vibration_raw = abs(magnitude - baseline_magnitude);
    
    // กรอง noise ที่ต่ำกว่า 50 (ประมาณ 0.12 m/s²)
    if(vibration_raw < 50) {
      vibration_raw = 0;
    }
    
    // แปลงเป็น m/s² (±8g → 4096 LSB/g)
    float vibration_mss = (vibration_raw / 4096.0) * 9.81;
    
    // แปลง time เป็นวินาที
    float time_sec = (currentTime - startTime) / 1000.0;
    
    // แสดงผลแบบ CSV
    Serial.print(time_sec, 3);
    Serial.print(",");
    Serial.println(vibration_mss, 4);
  }
}

// ฟังก์ชันอ่านค่า Accelerometer
void readAccel() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
}

/* 
 * วิธีบันทึกข้อมูลและทำกราฟ:
 * 
 * ========================================
 * 1. บันทึกด้วย Arduino Serial Monitor:
 * ========================================
 *    - เปิด Serial Monitor (115200 baud)
 *    - คัดลอกข้อมูลทั้งหมด (Ctrl+A, Ctrl+C)
 *    - วางใน Excel/Google Sheets
 *    - ลบบรรทัดหัวข้อออก เหลือแค่ตัวเลข
 *    - เลือกข้อมูล → Insert → Chart → Line Chart
 * 
 * ========================================
 * 2. บันทึกด้วย Python (แนะนำ):
 * ========================================
 * 
import serial
import csv
import matplotlib.pyplot as plt
from datetime import datetime
import numpy as np

# เชื่อมต่อ Arduino (เปลี่ยน 'COM3' ตามพอร์ตของคุณ)
port = 'COM3'  # Windows: COM3, Mac: /dev/tty.usbmodem, Linux: /dev/ttyUSB0
ser = serial.Serial(port, 115200, timeout=1)

# รอให้ Arduino พร้อม
print("Waiting for Arduino...")
while True:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    print(line)
    if line.startswith('Time'):
        break

# เก็บข้อมูล
times = []
vibrations = []
filename = f'vibration_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv'

print(f"\nRecording to {filename}...")
print("Press Ctrl+C to stop and plot")

try:
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Time(sec)', 'Vibration(m/s²)'])
        
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # กรองเฉพาะข้อมูล
                if ',' in line and not line.startswith('='):
                    try:
                        data = line.split(',')
                        time_val = float(data[0])
                        vib_val = float(data[1])
                        
                        times.append(time_val)
                        vibrations.append(vib_val)
                        
                        writer.writerow([time_val, vib_val])
                        f.flush()
                        
                        print(f"Time: {time_val:.2f}s, Vibration: {vib_val:.4f} m/s²")
                    except:
                        pass

except KeyboardInterrupt:
    print(f"\n\nRecording stopped!")
    print(f"Data saved to {filename}")
    ser.close()
    
    # สร้างกราฟ
    if len(times) > 0:
        plt.figure(figsize=(12, 6))
        
        # กราฟหลัก
        plt.subplot(2, 1, 1)
        plt.plot(times, vibrations, linewidth=1)
        plt.xlabel('Time (seconds)')
        plt.ylabel('Vibration (m/s²)')
        plt.title('Vibration vs Time')
        plt.grid(True, alpha=0.3)
        
        # Histogram
        plt.subplot(2, 1, 2)
        plt.hist(vibrations, bins=50, edgecolor='black')
        plt.xlabel('Vibration (m/s²)')
        plt.ylabel('Frequency')
        plt.title('Vibration Distribution')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # บันทึกกราฟ
        plot_filename = filename.replace('.csv', '.png')
        plt.savefig(plot_filename, dpi=300)
        print(f"Plot saved to {plot_filename}")
        
        # แสดงกราฟ
        plt.show()
        
        # สถิติ
        print("\n=== Statistics ===")
        print(f"Duration: {max(times):.2f} seconds")
        print(f"Samples: {len(times)}")
        print(f"Max vibration: {max(vibrations):.4f} m/s²")
        print(f"Mean vibration: {np.mean(vibrations):.4f} m/s²")
        print(f"Std deviation: {np.std(vibrations):.4f} m/s²")
 * 
 * ========================================
 * 3. วิเคราะห์ข้อมูลใน Excel:
 * ========================================
 *    - เปิดไฟล์ CSV ใน Excel
 *    - เลือกคอลัมน์ Vibration
 *    - Insert → Line Chart
 *    - เพิ่ม Trendline หากต้องการ
 *    - คำนวณ: =MAX(), =AVERAGE(), =STDEV()
 * 
 * ========================================
 * 4. วิเคราะห์ FFT (ความถี่การสั่น):
 * ========================================
 * 
import numpy as np
from scipy import signal
import pandas as pd

# อ่านข้อมูล
df = pd.read_csv('vibration_20241215_120000.csv')
vibration = df['Vibration(m/s²)'].values
time = df['Time(sec)'].values

# คำนวณ sampling rate
dt = np.mean(np.diff(time))
fs = 1 / dt  # sampling frequency

# FFT
fft_values = np.fft.fft(vibration)
fft_freq = np.fft.fftfreq(len(vibration), dt)

# เอาแค่ positive frequencies
positive_freq = fft_freq[:len(fft_freq)//2]
positive_fft = np.abs(fft_values[:len(fft_values)//2])

# Plot
plt.figure(figsize=(12, 4))
plt.plot(positive_freq, positive_fft)
plt.xlabel('Frequency (Hz)')
plt.ylabel('Amplitude')
plt.title('Vibration Frequency Spectrum')
plt.grid(True)
plt.xlim(0, 25)  # แสดงความถี่ 0-25 Hz
plt.show()

# หา dominant frequency
dominant_idx = np.argmax(positive_fft[1:]) + 1  # ข้าม DC component
dominant_freq = positive_freq[dominant_idx]
print(f"Dominant frequency: {dominant_freq:.2f} Hz")
 */
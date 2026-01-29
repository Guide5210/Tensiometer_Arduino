/*
 * ====================================================================
 * I2C Device Test - Vibration & Temperature with Animated Eyes
 * MPU-6500 + MLX90614 + OLED 0.96" on I2C Bus
 * Arduino Mega 2560
 * ====================================================================
 * 
 * WIRING:
 * -------
 * Arduino Pin 20 (SDA) → MPU-6500 SDA, MLX90614 SDA, OLED SDA
 * Arduino Pin 21 (SCL) → MPU-6500 SCL, MLX90614 SCL, OLED SCL
 * Arduino 3.3V → MPU-6500 VCC, MLX90614 VCC
 * Arduino 5V → OLED VCC
 * Arduino GND → All GND pins
 * 
 * PURPOSE: Test vibration & temperature with animated blinking eyes
 * STARTUP: Shows KMITL logo 5s, then SIITec, then begins monitoring
 * MODIFIED: New eye animation style with multiple expressions
 * ====================================================================
 */

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// ==================== KMITL Logo Bitmap ====================
const unsigned char epd_bitmap_KMITL_Sublogo [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x03, 0xff, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xc0, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x83, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0x80, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x83, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xcf, 0x80, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x07, 0xff, 0xff, 0xff, 0xef, 0xff, 0x8f, 0x00, 0x00, 0x03, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x07, 0xf8, 0xff, 0xff, 0xef, 0xff, 0x0f, 0xff, 0x03, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfe, 0x0f, 0xfc, 0xff, 0xff, 0xcf, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfe, 0x0f, 0xfc, 0xff, 0xff, 0xcf, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfc, 0x1f, 0xfc, 0x7f, 0xff, 0xcf, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfc, 0x1f, 0xfc, 0x7f, 0xff, 0x83, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf8, 0x3f, 0xfc, 0x7f, 0xff, 0x87, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf8, 0x3f, 0xf8, 0x3f, 0xff, 0x87, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf0, 0x7f, 0xf8, 0x3f, 0xff, 0x07, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf0, 0x7f, 0xf8, 0x3f, 0xff, 0x07, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xe0, 0xff, 0xf8, 0x1f, 0xff, 0x07, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xe0, 0xff, 0xf8, 0x1f, 0xff, 0x03, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xc1, 0xff, 0xe0, 0x1f, 0xfe, 0x03, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xc1, 0xff, 0xf0, 0x07, 0xfe, 0x03, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0x83, 0xff, 0xf0, 0x0f, 0xfe, 0x03, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0x83, 0xff, 0xf0, 0x0f, 0xfc, 0x03, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc3, 0x07, 0xff, 0xf0, 0x07, 0xfc, 0x01, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x07, 0xff, 0xe0, 0x07, 0xfc, 0x01, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x0f, 0xff, 0xe0, 0x07, 0xf8, 0x01, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x0f, 0xff, 0xe0, 0x03, 0xf8, 0x01, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x0f, 0xff, 0xe0, 0x83, 0xf8, 0x01, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x0f, 0xff, 0xe1, 0xc3, 0xf0, 0xe1, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x07, 0xff, 0xe0, 0xe3, 0xf0, 0xf0, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc0, 0x07, 0xff, 0x83, 0xe1, 0xf1, 0xf0, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0x83, 0xff, 0xc3, 0xe1, 0xe1, 0xf0, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0x83, 0xff, 0xc3, 0xf1, 0xe1, 0xf0, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xc1, 0xff, 0xc3, 0xf0, 0x03, 0xe0, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xc1, 0xff, 0xc3, 0xf0, 0x03, 0xf8, 0xff, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xe1, 0xff, 0x87, 0xf8, 0x03, 0xf8, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xe0, 0xff, 0x87, 0xf8, 0x03, 0xf8, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf0, 0xff, 0x87, 0xf8, 0x07, 0xf8, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf0, 0x7f, 0x87, 0xfc, 0x07, 0xf8, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf8, 0x7f, 0x87, 0xfc, 0x07, 0xfc, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xf8, 0x3f, 0x8f, 0xfc, 0x0f, 0xfc, 0x7f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfc, 0x3f, 0x0f, 0xfe, 0x0f, 0xfc, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfc, 0x1f, 0x0f, 0xfe, 0x07, 0xfc, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfe, 0x1f, 0x0f, 0xfe, 0x1f, 0xfc, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfe, 0x0f, 0x0f, 0xff, 0x1f, 0xfc, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xfe, 0x0f, 0x9f, 0xff, 0x1f, 0xfe, 0x3f, 0x0f, 0xff, 0x07, 0xff, 0xe3, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x07, 0x9f, 0xff, 0x3f, 0xfe, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe1, 0xff, 0xff, 
	0xff, 0xc7, 0xff, 0x07, 0xdf, 0xff, 0x3f, 0xfe, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0x3f, 
	0xff, 0xc7, 0xff, 0x83, 0xdf, 0xff, 0xbf, 0xfe, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0x7f, 
	0xff, 0xc7, 0xff, 0x83, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0x7f, 
	0xff, 0xc7, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0xff, 
	0xff, 0xc7, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0xff, 
	0xff, 0xc7, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x0f, 0xff, 0x07, 0xff, 0xe0, 0x00, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// ==================== OLED Configuration ====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==================== MPU-6500 Configuration ====================
const int MPU_ADDR = 0x68;
int16_t AcX, AcY, AcZ;

// ==================== MLX90614 Sensor ====================
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ==================== Vibration Detection ====================
float baseline_magnitude = 0;
float vibration_threshold = 0.5;
bool is_vibrating = false;
unsigned long vibration_start_time = 0;
bool vibration_display_active = false;

// ==================== Status Variables ====================
bool mpu_ok = false;
bool mlx_ok = false;
bool oled_ok = false;
unsigned long animation_timer = 0;
int blink_state = 0;
unsigned long expression_timer = 0;
int current_expression = 0;  // 0-9 for different expressions

// ==================== Current Sensor Values ====================
float current_vibration = 0;
float current_ambient = 0;
float current_object = 0;

// ==================== Eye Animation Parameters ====================
#define LEFT_EYE_X 32
#define LEFT_EYE_Y 40
#define RIGHT_EYE_X 96
#define RIGHT_EYE_Y 40
#define EYE_WIDTH 20
#define EYE_HEIGHT 24
#define PUPIL_WIDTH 8
#define PUPIL_HEIGHT 12

// Expression definitions
#define EXPR_FRONT 0
#define EXPR_MIDDLE 1
#define EXPR_NARROW 2
#define EXPR_WIDE 3
#define EXPR_DOWN 4
#define EXPR_UP 5
#define EXPR_RIGHT 6
#define EXPR_LEFT 7
#define EXPR_CRY 8
#define EXPR_GLARE 9

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Vibration & Temperature Test");
  Serial.println("  With Animated Blinking Eyes");
  Serial.println("========================================\n");
  
  Wire.begin();
  Wire.setClock(100000);
  
  Serial.println("[1/4] Initializing I2C...");
  delay(500);
  
  Serial.println("[2/4] Testing OLED Display (0x3C)...");
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("  ✗ OLED NOT FOUND at 0x3C");
    Serial.println("  Trying alternate address 0x3D...");
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("  ✗ OLED NOT FOUND at 0x3D");
      oled_ok = false;
    } else {
      Serial.println("  ✓ OLED FOUND at 0x3D");
      oled_ok = true;
    }
  } else {
    Serial.println("  ✓ OLED FOUND at 0x3C");
    oled_ok = true;
  }
  
  if(oled_ok) {
    displaySplashScreen();
    delay(5000);
    displaySIITecScreen();
    delay(3000);
  }
  
  Serial.println("[3/4] Testing MLX90614 (0x5A)...");
  
  if (!mlx.begin(0x5A)) {
    Serial.println("  ✗ MLX90614 NOT FOUND");
    mlx_ok = false;
  } else {
    Serial.println("  ✓ MLX90614 FOUND");
    mlx_ok = true;
  }
  
  Serial.println("[4/4] Testing MPU-6500 (0x68)...");
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission(true);
  
  delay(100);
  
  Serial.println("Calibrating...");
  if(oled_ok) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("CALIBRATING");
    display.println("Keep still!");
    display.display();
  }
  
  float sum = 0;
  for(int i = 0; i < 100; i++) {
    readAccel();
    float mag = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
    sum += mag;
    delay(20);
  }
  baseline_magnitude = sum / 100.0;
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  
  if(Wire.requestFrom(MPU_ADDR, 6, true) == 6) {
    Serial.println("  ✓ MPU-6500 FOUND");
    mpu_ok = true;
    Wire.read(); Wire.read();
    Wire.read(); Wire.read();
    Wire.read(); Wire.read();
  } else {
    Serial.println("  ✗ MPU-6500 NOT FOUND");
    mpu_ok = false;
  }
  
  Serial.println("\n========================================");
  Serial.println("DEVICE DETECTION SUMMARY:");
  Serial.println("========================================");
  
  if(oled_ok) {
    Serial.println("✓ OLED 0.96\" (0x3C/0x3D) .......... OK");
  } else {
    Serial.println("✗ OLED 0.96\" ....................... FAILED");
  }
  
  if(mpu_ok) {
    Serial.println("✓ MPU-6500 (0x68) .................. OK");
  } else {
    Serial.println("✗ MPU-6500 (0x68) .................. FAILED");
  }
  
  if(mlx_ok) {
    Serial.println("✓ MLX90614 (0x5A) .................. OK");
  } else {
    Serial.println("✗ MLX90614 (0x5A) .................. FAILED");
  }
  
  Serial.println("========================================\n");
  Serial.println("Monitoring vibration & temperature...\n");
  
  if(oled_ok) {
    display.clearDisplay();
    display.display();
    delay(500);
  }
  
  animation_timer = millis();
  expression_timer = millis();
}

void loop() {
  if(!mpu_ok || !mlx_ok) {
    if(oled_ok) {
      display.clearDisplay();
      display.setCursor(0, 0);
      if(!mpu_ok) display.println("ERROR!");
      if(!mpu_ok) display.println("MPU not found");
      if(!mlx_ok) display.println("MLX not found");
      display.display();
    }
    delay(1000);
    return;
  }
  
  // Read sensors
  readAccel();
  
  float magnitude = sqrt((float)AcX*AcX + (float)AcY*AcY + (float)AcZ*AcZ);
  float vibration_raw = abs(magnitude - baseline_magnitude);
  
  if(vibration_raw < 50) {
    vibration_raw = 0;
  }
  
  current_vibration = (vibration_raw / 4096.0) * 9.81;
  current_ambient = mlx.readAmbientTempC();
  current_object = mlx.readObjectTempC();
  
  // Vibration detection logic with 3-second hold
  if(current_vibration > vibration_threshold) {
    if(!is_vibrating) {
      // New vibration detected
      is_vibrating = true;
      vibration_start_time = millis();
      vibration_display_active = true;
    }
  } else {
    is_vibrating = false;
  }
  
  // Check if 3 seconds have passed since vibration ended
  if(vibration_display_active && !is_vibrating) {
    if(millis() - vibration_start_time >= 3000) {
      vibration_display_active = false;
    }
  }
  
  // Update vibration start time if still vibrating
  if(is_vibrating) {
    vibration_start_time = millis();
  }
  
  Serial.print("Vib: ");
  Serial.print(current_vibration, 4);
  Serial.print(" m/s²  |  Amb: ");
  Serial.print(current_ambient, 1);
  Serial.print("°C  |  Obj: ");
  Serial.print(current_object, 1);
  Serial.println("°C");
  
  // Update display based on vibration state
  if(oled_ok) {
    if(vibration_display_active) {
      displayVibrationAlert();
    } else {
      updateEyeExpression();
      displayAnimatedEyes();
    }
  }
  
  delay(50);
}

void displaySplashScreen() {
  display.clearDisplay();
  display.drawBitmap(0, 0, epd_bitmap_KMITL_Sublogo, 128, 64, SSD1306_WHITE);
  display.display();
}

void displaySIITecScreen() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 20);
  display.println("SIITec");
  display.display();
}

void updateEyeExpression() {
  unsigned long current_time = millis();
  
  // Change expression every 2.5 seconds
  if(current_time - expression_timer >= 2500) {
    expression_timer = current_time;
    current_expression = random(0, 10);  // 10 different expressions
  }
  
  // Blink animation (happens independently)
  unsigned long elapsed = (current_time - animation_timer) % 3500;
  
  if(elapsed < 3000) {
    blink_state = 0;  // Eyes open
  } else if(elapsed < 3150) {
    blink_state = 1;  // Closing
  } else if(elapsed < 3300) {
    blink_state = 2;  // Closed
  } else {
    blink_state = 3;  // Opening
  }
}

void drawEye(int centerX, int centerY, int pupilOffsetX, int pupilOffsetY, int eyeSize) {
  // Eye size: 0=normal, 1=narrow, 2=wide
  int eyeHeight = EYE_HEIGHT;
  
  if(eyeSize == 1) {  // Narrow
    eyeHeight = EYE_HEIGHT / 2;
  } else if(eyeSize == 2) {  // Wide
    eyeHeight = EYE_HEIGHT + 4;
  }
  
  if(blink_state == 0) {  // Eyes open
    // Draw white filled oval for eye
    display.fillRoundRect(centerX - EYE_WIDTH/2, centerY - eyeHeight/2, 
                         EYE_WIDTH, eyeHeight, eyeHeight/2, SSD1306_WHITE);
    
    // Draw black pupil
    int pupilX = centerX + pupilOffsetX;
    int pupilY = centerY + pupilOffsetY;
    display.fillRoundRect(pupilX - PUPIL_WIDTH/2, pupilY - PUPIL_HEIGHT/2,
                         PUPIL_WIDTH, PUPIL_HEIGHT, PUPIL_HEIGHT/2, SSD1306_BLACK);
    
    // Add small white highlight in pupil
    display.fillCircle(pupilX - 2, pupilY - 3, 2, SSD1306_WHITE);
    
  } else if(blink_state == 1) {  // Closing
    int h = eyeHeight / 2;
    display.fillRoundRect(centerX - EYE_WIDTH/2, centerY - h/2, 
                         EYE_WIDTH, h, h/2, SSD1306_WHITE);
  } else if(blink_state == 2) {  // Closed
    display.drawLine(centerX - EYE_WIDTH/2, centerY, 
                    centerX + EYE_WIDTH/2, centerY, SSD1306_WHITE);
    display.drawLine(centerX - EYE_WIDTH/2, centerY + 1, 
                    centerX + EYE_WIDTH/2, centerY + 1, SSD1306_WHITE);
  } else {  // Opening
    int h = eyeHeight * 2 / 3;
    display.fillRoundRect(centerX - EYE_WIDTH/2, centerY - h/2, 
                         EYE_WIDTH, h, h/2, SSD1306_WHITE);
  }
}

void displayAnimatedEyes() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Draw large temperature display at top
  display.setCursor(10, 5);
  display.print(current_object, 1);
  display.print("C");
  
  // Set pupil positions and eye size based on expression
  int leftPupilX = 0, leftPupilY = 0;
  int rightPupilX = 0, rightPupilY = 0;
  int eyeSize = 0;  // 0=normal, 1=narrow, 2=wide
  
  switch(current_expression) {
    case EXPR_FRONT:  // Front
      leftPupilX = 0; leftPupilY = 0;
      rightPupilX = 0; rightPupilY = 0;
      break;
    case EXPR_MIDDLE:  // Middle (slightly together)
      leftPupilX = 2; leftPupilY = 0;
      rightPupilX = -2; rightPupilY = 0;
      break;
    case EXPR_NARROW:  // Narrow eyes
      leftPupilX = 0; leftPupilY = 0;
      rightPupilX = 0; rightPupilY = 0;
      eyeSize = 1;
      break;
    case EXPR_WIDE:  // Wide eyes
      leftPupilX = 0; leftPupilY = 0;
      rightPupilX = 0; rightPupilY = 0;
      eyeSize = 2;
      break;
    case EXPR_DOWN:  // Looking down
      leftPupilX = 0; leftPupilY = 5;
      rightPupilX = 0; rightPupilY = 5;
      break;
    case EXPR_UP:  // Looking up
      leftPupilX = 0; leftPupilY = -5;
      rightPupilX = 0; rightPupilY = -5;
      break;
    case EXPR_RIGHT:  // Looking right
      leftPupilX = 5; leftPupilY = 0;
      rightPupilX = 5; rightPupilY = 0;
      break;
    case EXPR_LEFT:  // Looking left
      leftPupilX = -5; leftPupilY = 0;
      rightPupilX = -5; rightPupilY = 0;
      break;
    case EXPR_CRY:  // Sad/Cry
      leftPupilX = -3; leftPupilY = 2;
      rightPupilX = 3; rightPupilY = 2;
      break;
    case EXPR_GLARE:  // Angry glare
      leftPupilX = 2; leftPupilY = -2;
      rightPupilX = -2; rightPupilY = -2;
      eyeSize = 1;
      break;
  }
  
  // Draw both eyes
  drawEye(LEFT_EYE_X, LEFT_EYE_Y, leftPupilX, leftPupilY, eyeSize);
  drawEye(RIGHT_EYE_X, RIGHT_EYE_Y, rightPupilX, rightPupilY, eyeSize);
  
  // Add expression-specific features
  if(current_expression == EXPR_CRY) {
    // Draw tears
    display.drawLine(LEFT_EYE_X - 8, LEFT_EYE_Y + 15, LEFT_EYE_X - 8, LEFT_EYE_Y + 22, SSD1306_WHITE);
    display.drawLine(RIGHT_EYE_X + 8, RIGHT_EYE_Y + 15, RIGHT_EYE_X + 8, RIGHT_EYE_Y + 22, SSD1306_WHITE);
  } else if(current_expression == EXPR_GLARE) {
    // Draw angry eyebrows
    display.drawLine(LEFT_EYE_X - 12, LEFT_EYE_Y - 15, LEFT_EYE_X + 8, LEFT_EYE_Y - 18, SSD1306_WHITE);
    display.drawLine(LEFT_EYE_X - 12, LEFT_EYE_Y - 16, LEFT_EYE_X + 8, LEFT_EYE_Y - 19, SSD1306_WHITE);
    display.drawLine(RIGHT_EYE_X - 8, RIGHT_EYE_Y - 18, RIGHT_EYE_X + 12, RIGHT_EYE_Y - 15, SSD1306_WHITE);
    display.drawLine(RIGHT_EYE_X - 8, RIGHT_EYE_Y - 19, RIGHT_EYE_X + 12, RIGHT_EYE_Y - 16, SSD1306_WHITE);
  } else {
    // Draw neutral mouth (smile)
    display.drawLine(50, 55, 78, 55, SSD1306_WHITE);
    display.drawLine(50, 56, 78, 56, SSD1306_WHITE);
  }
  
  display.display();
}

void displayVibrationAlert() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println("!!! VIBRATION !!!");
  display.println();
  
  // Display vibration value on the left
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(current_vibration, 1);
  display.setTextSize(1);
  display.print(" m/s2");
  
  // Display temperature on the right side, same line
  display.setCursor(75, 20);
  display.setTextSize(2);
  display.print(current_object, 1);
  display.setTextSize(1);
  display.print("C");
  
  // Draw shocked/surprised eyes
  // Left eye
  display.fillCircle(LEFT_EYE_X, LEFT_EYE_Y + 5, 10, SSD1306_WHITE);
  display.fillCircle(LEFT_EYE_X, LEFT_EYE_Y + 5, 5, SSD1306_BLACK);
  
  // Right eye
  display.fillCircle(RIGHT_EYE_X, RIGHT_EYE_Y + 5, 10, SSD1306_WHITE);
  display.fillCircle(RIGHT_EYE_X, RIGHT_EYE_Y + 5, 5, SSD1306_BLACK);
  
  // Draw surprised mouth (oval)
  display.drawCircle(64, 58, 6, SSD1306_WHITE);
  display.drawCircle(64, 57, 6, SSD1306_WHITE);
  
  display.display();
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
 * ====================================================================
 * UPDATES v2.0:
 * - Completely redesigned eye animation (filled white eyes + black pupils)
 * - 10 different eye expressions based on reference image
 * - Vibration alert stays for 3 seconds after vibration stops
 * - Larger, more expressive eyes with highlights
 * - Expression-specific features (tears, angry eyebrows)
 * - Smooth blinking animation
 * - Random expression changes every 2.5 seconds
 * ====================================================================
 */

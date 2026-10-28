//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

//#define REMOTEXY__DEBUGLOG    

#define REMOTEXY_MODE__WIFI_POINT

#include <WiFi.h>
#include <RemoteXY.h>
#include <ESP32Servo.h> // Library untuk kontrol servo di ESP32

// RemoteXY connection settings 
#define REMOTEXY_WIFI_SSID "RemoteXY"
#define REMOTEXY_WIFI_PASSWORD "12345678"
#define REMOTEXY_SERVER_PORT 6377

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   
  { 255,10,0,0,0,14,1,19,0,0,0,0,31,2,106,200,200,84,1,1,
  13,0,130,9,21,14,160,127,32,69,22,9,17,130,7,29,21,95,16,7,
  26,67,9,17,4,16,50,7,86,23,9,12,63,32,2,26,4,41,7,6,
  150,128,38,67,11,160,2,26,10,12,10,57,57,70,4,14,14,48,4,26,
  31,49,0,31,79,70,70,0,10,30,24,33,33,84,4,14,14,48,4,26,
  31,50,0,31,79,70,70,0,10,30,14,33,33,98,4,14,14,48,4,26,
  31,51,0,31,79,70,70,0,10,31,19,33,33,112,4,14,14,48,4,26,
  31,52,0,31,79,70,70,0,10,20,124,33,33,69,64,14,14,48,4,26,
  31,78,65,73,75,0,31,79,70,70,0,10,27,124,33,33,83,64,14,14,
  48,4,26,31,84,85,82,85,78,0,31,79,70,70,0,10,35,124,33,33,
  97,64,14,14,48,4,26,31,66,85,75,65,32,67,65,80,73,84,0,31,
  79,70,70,0,10,42,124,33,33,111,64,14,14,48,4,26,31,84,85,84,
  85,80,32,67,65,80,73,84,0,31,79,70,70,0,129,14,57,71,29,77,
  20,43,7,64,24,83,80,69,69,68,32,76,69,86,69,76,0 };
  
struct {
  // input variables
  int8_t slider_01;       // Slider Maju/Mundur (-100 s/d 100)
  int8_t slider_02;       // Slider Kiri/Kanan (-100 s/d 100)
  uint8_t pushSwitch_01;  // Speed 1 (25%)
  uint8_t pushSwitch_02;  // Speed 2 (50%)
  uint8_t pushSwitch_03;  // Speed 3 (75%)
  uint8_t pushSwitch_04;  // Speed 4 (100%)
  uint8_t pushSwitch_05;  // NAIK (Servo 1)
  uint8_t pushSwitch_06;  // TURUN (Servo 1)
  uint8_t pushSwitch_07;  // BUKA CAPIT (Servo 2)
  uint8_t pushSwitch_08;  // TUTUP CAPIT (Servo 2)

  // other variable
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

// ==========================================
// DEFINISI PIN MOTOR & SERVO (AMAN UNTUK ESP32-C3)
// ==========================================
#define PIN_IN1 0 // Motor Kiri Maju
#define PIN_IN2 1 // Motor Kiri Mundur
#define PIN_IN3 3 // Motor Kanan Maju
#define PIN_IN4 4 // Motor Kanan Mundur

#define PIN_SERVO1 5 // Servo Arm (Naik/Turun)
#define PIN_SERVO2 6 // Servo Capit (Buka/Tutup)

Servo servo1;
Servo servo2;

// Variabel Pengatur Kecepatan & Posisi Servo
float speed_multiplier = 0.25; // Default Speed Level 1 (25%)
int posServo1 = 90;            // Posisi Awal Servo 1 (0 - 180 derajat)
int posServo2 = 90;            // Posisi Awal Servo 2 (0 - 180 derajat)

unsigned long previousMillis = 0;
const long servoInterval = 20; // Kecepatan pergerakan halus servo (ms)

void setup() 
{
  RemoteXY_Init();  
  
  // Konfigurasi Pin Motor sebagai Output
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // Inisialisasi Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50); // Servo standar 50Hz
  servo2.setPeriodHertz(50);
  
  servo1.attach(PIN_SERVO1, 500, 2400);
  servo2.attach(PIN_SERVO2, 500, 2400);

  servo1.write(posServo1);
  servo2.write(posServo2);
}

void loop() 
{ 
  RemoteXYEngine.handler();   

  // ==========================================
  // 1. KONTROL SPEED LEVEL (25%, 50%, 75%, 100%)
  // ==========================================
  if (RemoteXY.pushSwitch_01 == 1) speed_multiplier = 0.25;
  else if (RemoteXY.pushSwitch_02 == 1) speed_multiplier = 0.50;
  else if (RemoteXY.pushSwitch_03 == 1) speed_multiplier = 0.75;
  else if (RemoteXY.pushSwitch_04 == 1) speed_multiplier = 1.00;

  // ==========================================
  // 2. KONTROL GERAK MOTOR (DIFFERENTIAL STEERING)
  // ==========================================
  int throttle = RemoteXY.slider_01; // Slider Kiri (Maju/Mundur)
  int steering = RemoteXY.slider_02; // Slider Kanan (Kiri/Kanan)

  // Mixing sinyal
  int speedKiri  = throttle + steering;
  int speedKanan = throttle - steering;

  // Batasi nilai mixing pada range -100 s/d 100
  speedKiri  = constrain(speedKiri, -100, 100);
  speedKanan = constrain(speedKanan, -100, 100);

  // Hitung nilai PWM akhir (0 - 255) sesuai Speed Level
  int pwmKiri  = map(abs(speedKiri)  * speed_multiplier, 0, 100, 0, 255);
  int pwmKanan = map(abs(speedKanan) * speed_multiplier, 0, 100, 0, 255);

  // Eksekusi Motor Kiri
  if (speedKiri > 5) {
    analogWrite(PIN_IN1, pwmKiri);
    analogWrite(PIN_IN2, 0);
  } else if (speedKiri < -5) {
    analogWrite(PIN_IN1, 0);
    analogWrite(PIN_IN2, pwmKiri);
  } else {
    analogWrite(PIN_IN1, 0);
    analogWrite(PIN_IN2, 0);
  }

  // Eksekusi Motor Kanan
  if (speedKanan > 5) {
    analogWrite(PIN_IN3, pwmKanan);
    analogWrite(PIN_IN4, 0);
  } else if (speedKanan < -5) {
    analogWrite(PIN_IN3, 0);
    analogWrite(PIN_IN4, pwmKanan);
  } else {
    analogWrite(PIN_IN3, 0);
    analogWrite(PIN_IN4, 0);
  }

  // ==========================================
  // 3. KONTROL SERVO (NAIK/TURUN & BUKA/TUTUP)
  // Non-blocking timer agar tidak mengganggu kontrol motor
  // ==========================================
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= servoInterval) {
    previousMillis = currentMillis;

    // Servo 1: NAIK / TURUN
    if (RemoteXY.pushSwitch_05 == 1 && posServo1 < 180) { // NAIK
      posServo1 += 2;
      servo1.write(posServo1);
    } 
    else if (RemoteXY.pushSwitch_06 == 1 && posServo1 > 0) { // TURUN
      posServo1 -= 2;
      servo1.write(posServo1);
    }

    // Servo 2: BUKA / TUTUP CAPIT
    if (RemoteXY.pushSwitch_07 == 1 && posServo2 < 180) { // BUKA CAPIT
      posServo2 += 2;
      servo2.write(posServo2);
    } 
    else if (RemoteXY.pushSwitch_08 == 1 && posServo2 > 0) { // TUTUP CAPIT
      posServo2 -= 2;
      servo2.write(posServo2);
    }
  }
}
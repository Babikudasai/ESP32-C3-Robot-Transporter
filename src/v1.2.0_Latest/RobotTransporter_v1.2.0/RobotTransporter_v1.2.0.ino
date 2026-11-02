#define REMOTEXY_MODE__WIFI_POINT
#include <WiFi.h>
#include <RemoteXY.h>
#include <ESP32Servo.h>

#define REMOTEXY_WIFI_SSID "RemoteXY"
#define REMOTEXY_WIFI_PASSWORD "12345678"
#define REMOTEXY_SERVER_PORT 6377

// GANTI ARRAY INI JIKA ANDA MENGUBAH SETTINGAN SLIDER DI WEB REMOTEXY
#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   
  { 255,10,0,0,0,14,1,19,0,0,0,0,31,2,106,200,200,84,1,1,
  13,0,130,23,19,12,143,130,33,63,20,9,17,130,4,36,21,95,12,13,
  23,60,9,17,4,12,55,7,86,18,13,11,60,48,2,26,4,35,247,6,
  138,129,37,64,12,176,2,26,10,9,19,57,57,62,8,15,15,48,4,26,
  31,49,0,31,79,70,70,0,10,26,74,36,36,77,8,15,15,48,4,26,
  31,50,0,31,79,70,70,0,10,28,24,36,36,92,8,15,15,48,4,26,
  31,51,0,31,79,70,70,0,10,29,28,36,36,107,8,15,15,48,4,26,
  31,52,0,31,79,70,70,0,10,20,21,36,36,62,60,15,15,48,4,26,
  31,78,65,73,75,0,31,79,70,70,0,10,28,24,36,36,77,60,15,15,
  48,4,26,31,84,85,82,85,78,0,31,79,70,70,0,10,36,24,36,36,
  92,60,15,15,48,4,26,31,66,85,75,65,32,67,65,80,73,84,0,31,
  79,70,70,0,10,44,24,36,36,107,60,15,15,48,4,26,31,84,85,84,
  85,80,32,67,65,80,73,84,0,31,79,70,70,0,129,8,69,71,29,68,
  25,49,8,64,8,83,80,69,69,68,32,76,69,86,69,76,0 };
  
struct {
  int8_t slider_01; int8_t slider_02;
  uint8_t pushSwitch_01; uint8_t pushSwitch_02; uint8_t pushSwitch_03; uint8_t pushSwitch_04;
  uint8_t pushSwitch_05; uint8_t pushSwitch_06; uint8_t pushSwitch_07; uint8_t pushSwitch_08;
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

#define PIN_IN1 0
#define PIN_IN2 1
#define PIN_IN3 3
#define PIN_IN4 4
#define PIN_SERVO1 5
#define PIN_SERVO2 6

Servo servo1; Servo servo2;
float speed_multiplier = 0.25; 
uint8_t current_speed_level = 1; 

int posServo1 = 90; int posServo2 = 90;            
unsigned long previousMillis = 0; const long servoInterval = 20;

// Konstanta Deadzone untuk Slider (mencegah motor mendengung di center point)
const int DEADZONE = 10; 

void setup() {
  RemoteXY_Init();  
  
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);

  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  servo1.setPeriodHertz(50); servo2.setPeriodHertz(50);
  servo1.attach(PIN_SERVO1, 500, 2400); servo2.attach(PIN_SERVO2, 500, 2400);
  servo1.write(posServo1); servo2.write(posServo2);

  // Auto Reset: Speed Level 1 ON, yang lain OFF
  RemoteXY.pushSwitch_01 = 1; RemoteXY.pushSwitch_02 = 0;
  RemoteXY.pushSwitch_03 = 0; RemoteXY.pushSwitch_04 = 0;
  
  // Auto Reset: Semua pergerakan capit OFF
  RemoteXY.pushSwitch_05 = 0; RemoteXY.pushSwitch_06 = 0;
  RemoteXY.pushSwitch_07 = 0; RemoteXY.pushSwitch_08 = 0;
}

void loop() { 
  RemoteXYEngine.handler();   

  // ============================================================
  // LOGIKA 1: MUTUAL EXCLUSION SPEED LEVEL
  // ============================================================
  if (RemoteXY.pushSwitch_01 == 1 && current_speed_level != 1) {
    RemoteXY.pushSwitch_02 = 0; RemoteXY.pushSwitch_03 = 0; RemoteXY.pushSwitch_04 = 0;
    speed_multiplier = 0.25; current_speed_level = 1;
  } 
  else if (RemoteXY.pushSwitch_02 == 1 && current_speed_level != 2) {
    RemoteXY.pushSwitch_01 = 0; RemoteXY.pushSwitch_03 = 0; RemoteXY.pushSwitch_04 = 0;
    speed_multiplier = 0.50; current_speed_level = 2;
  } 
  else if (RemoteXY.pushSwitch_03 == 1 && current_speed_level != 3) {
    RemoteXY.pushSwitch_01 = 0; RemoteXY.pushSwitch_02 = 0; RemoteXY.pushSwitch_04 = 0;
    speed_multiplier = 0.75; current_speed_level = 3;
  } 
  else if (RemoteXY.pushSwitch_04 == 1 && current_speed_level != 4) {
    RemoteXY.pushSwitch_01 = 0; RemoteXY.pushSwitch_02 = 0; RemoteXY.pushSwitch_03 = 0;
    speed_multiplier = 1.00; current_speed_level = 4;
  }

  // Fallback pengaman Level 1
  if (RemoteXY.pushSwitch_01 == 0 && RemoteXY.pushSwitch_02 == 0 && 
      RemoteXY.pushSwitch_03 == 0 && RemoteXY.pushSwitch_04 == 0) {
    RemoteXY.pushSwitch_01 = 1; speed_multiplier = 0.25; current_speed_level = 1;
  }

  // ============================================================
  // LOGIKA 2: INTERLOCK SERVO (Naik vs Turun, Buka vs Tutup)
  // ============================================================
  if (RemoteXY.pushSwitch_05 == 1) RemoteXY.pushSwitch_06 = 0; 
  if (RemoteXY.pushSwitch_06 == 1) RemoteXY.pushSwitch_05 = 0; 
  if (RemoteXY.pushSwitch_07 == 1) RemoteXY.pushSwitch_08 = 0; 
  if (RemoteXY.pushSwitch_08 == 1) RemoteXY.pushSwitch_07 = 0; 

  // ============================================================
  // LOGIKA 3: DIFFERENTIAL STEERING MOTOR + DEADZONE (AUTO-OFF)
  // ============================================================
  int throttle = RemoteXY.slider_01;
  int steering = RemoteXY.slider_02;

  // Terapkan Deadzone: Paksa 0 jika slider di sekitar center point
  if (throttle > -DEADZONE && throttle < DEADZONE) throttle = 0;
  if (steering > -DEADZONE && steering < DEADZONE) steering = 0;

  int speedKiri  = constrain(throttle + steering, -100, 100);
  int speedKanan = constrain(throttle - steering, -100, 100);
  
  int pwmKiri  = map(abs(speedKiri)  * speed_multiplier, 0, 100, 0, 255);
  int pwmKanan = map(abs(speedKanan) * speed_multiplier, 0, 100, 0, 255);

  // Auto-off jika speed mutlak 0 (berada di center point / deadzone)
  if (speedKiri > 0) { analogWrite(PIN_IN1, pwmKiri); analogWrite(PIN_IN2, 0); }
  else if (speedKiri < 0) { analogWrite(PIN_IN1, 0); analogWrite(PIN_IN2, pwmKiri); }
  else { analogWrite(PIN_IN1, 0); analogWrite(PIN_IN2, 0); }

  if (speedKanan > 0) { analogWrite(PIN_IN3, pwmKanan); analogWrite(PIN_IN4, 0); }
  else if (speedKanan < 0) { analogWrite(PIN_IN3, 0); analogWrite(PIN_IN4, pwmKanan); }
  else { analogWrite(PIN_IN3, 0); analogWrite(PIN_IN4, 0); }

  // ============================================================
  // LOGIKA 4: PERGERAKAN SERVO NON-BLOCKING
  // ============================================================
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= servoInterval) {
    previousMillis = currentMillis;
    if (RemoteXY.pushSwitch_05 == 1 && posServo1 < 180) { posServo1 += 2; servo1.write(posServo1); } 
    else if (RemoteXY.pushSwitch_06 == 1 && posServo1 > 0) { posServo1 -= 2; servo1.write(posServo1); }

    if (RemoteXY.pushSwitch_07 == 1 && posServo2 < 180) { posServo2 += 2; servo2.write(posServo2); } 
    else if (RemoteXY.pushSwitch_08 == 1 && posServo2 > 0) { posServo2 -= 2; servo2.write(posServo2); }
  }
}
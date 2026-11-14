/*
   -- Transporter 2026 ESP32-C3 (Isolated PWM Timer: Tank + Lift & Capit) --
   Board  : ESP32C3 Dev Module
   Driver : MX1650 / MX1508 Dual DC Motor Driver
   Library: RemoteXY (>= 4.1.10) & ESP32Servo
*/

//////////////////////////////////////////////
//      1. KONFIGURASI & TUNING ROBOT       //
//////////////////////////////////////////////

// --- Pinout ESP32-C3 ---
#define PIN_IN1         4   // Motor Kiri A
#define PIN_IN2         3   // Motor Kiri B
#define PIN_IN3         1   // Motor Kanan A
#define PIN_IN4         0   // Motor Kanan B

#define PIN_SERVO_LIFT  5   // Servo Angkat (Naik/Turun)
#define PIN_SERVO_CAPIT 6   // Servo Capit (Buka/Tutup)

// --- Kalibrasi Arah Gearbox Tank ---
const bool INVERT_MOTOR_KIRI  = false;
const bool INVERT_MOTOR_KANAN = true;  // Gearbox kanan posisi mirror
const bool SWAP_KIRI_KANAN    = false;

// --- Tuning Kontrol & Kecepatan (PWM 0 - 255) ---
const int   SLIDER_DEADZONE   = 10;    // Abaikan geseran kecil (-10 s/d 10)
const float TURN_SENSITIVITY  = 0.85;  // Sensitivitas belok (0.5 = halus, 1.0 = agresif)

const int SPEED_LVL_1 = 110;  // Level 1: Mode presisi ambil/taruh objek
const int SPEED_LVL_2 = 160;  // Level 2: Kecepatan normal (Default)
const int SPEED_LVL_3 = 210;  // Level 3: Kecepatan cepat
const int SPEED_LVL_4 = 255;  // Level 4: Full power

// --- Kalibrasi Sudut & Kecepatan Servo (0 - 180 derajat) ---
const int LIFT_MIN_ANGLE     = 20;   // Batas bawah arm lift
const int LIFT_MAX_ANGLE     = 140;  // Batas atas arm lift
const int LIFT_START_ANGLE   = 30;   // Posisi awal lift saat nyala
const int LIFT_STEP_DEG      = 2;    // Langkah derajat per update (1-5)
const int LIFT_SPEED_MS      = 15;   // Delay antar langkah (ms)

const int CAPIT_OPEN_ANGLE   = 120;  // Sudut capit terbuka maksimal
const int CAPIT_CLOSE_ANGLE  = 35;   // Sudut capit menjepit maksimal
const int CAPIT_START_ANGLE  = 110;  // Posisi awal capit (terbuka)
const int CAPIT_STEP_DEG     = 2;    // Langkah derajat per update (1-5)
const int CAPIT_SPEED_MS     = 12;   // Delay antar langkah (ms)

//////////////////////////////////////////////
//        2. REMOTEXY CONFIGURATION         //
//////////////////////////////////////////////

//#define REMOTEXY__DEBUGLOG
#define REMOTEXY_MODE__WIFI_POINT

#include <WiFi.h>
#include <RemoteXY.h>
#include <ESP32Servo.h>

#define REMOTEXY_WIFI_SSID "TRANSPORTER 1"
#define REMOTEXY_WIFI_PASSWORD "12345678"
#define REMOTEXY_SERVER_PORT 6377

#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 277 bytes V19 , PRO license required
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
  int8_t slider_01;      // Atas (+100 = Maju) / Bawah (-100 = Mundur)
  int8_t slider_02;      // Kanan (+100 = Kanan) / Kiri (-100 = Kiri)
  uint8_t pushSwitch_01; // Speed Level 1
  uint8_t pushSwitch_02; // Speed Level 2
  uint8_t pushSwitch_03; // Speed Level 3
  uint8_t pushSwitch_04; // Speed Level 4
  uint8_t pushSwitch_05; // NAIK
  uint8_t pushSwitch_06; // TURUN
  uint8_t pushSwitch_07; // BUKA CAPIT
  uint8_t pushSwitch_08; // TUTUP CAPIT
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

//////////////////////////////////////////////
//      3. OBJEK PWM TERISOLASI & STATE     //
//////////////////////////////////////////////

Servo servoLift;
Servo servoCapit;

// Objek PWM khusus motor agar tidak memakai analogWrite()
ESP32PWM pwmIN1;
ESP32PWM pwmIN2;
ESP32PWM pwmIN3;
ESP32PWM pwmIN4;

int activeSpeedLevel = 2;
int currentMaxPWM    = SPEED_LVL_2;

int lastAppliedLeft  = -999;
int lastAppliedRight = -999;

int currentLiftAngle  = LIFT_START_ANGLE;
int currentCapitAngle = CAPIT_START_ANGLE;

unsigned long lastLiftTimer  = 0;
unsigned long lastCapitTimer = 0;

uint8_t prevNaik = 0, prevTurun = 0;
uint8_t prevBuka = 0, prevTutup = 0;

//////////////////////////////////////////////
//         4. FUNGSI KONTROL MOTOR          //
//////////////////////////////////////////////

void driveChannelA(int pwm) {
  if (pwm > 0) {
    pwmIN1.write(pwm);
    pwmIN2.write(0);
  } else if (pwm < 0) {
    pwmIN1.write(0);
    pwmIN2.write(-pwm);
  } else {
    pwmIN1.write(0);
    pwmIN2.write(0);
  }
}

void driveChannelB(int pwm) {
  if (pwm > 0) {
    pwmIN3.write(pwm);
    pwmIN4.write(0);
  } else if (pwm < 0) {
    pwmIN3.write(0);
    pwmIN4.write(-pwm);
  } else {
    pwmIN3.write(0);
    pwmIN4.write(0);
  }
}

void setTankMotors(int leftSpeed, int rightSpeed) {
  // Hanya tulis ulang ke hardware jika nilai PWM benar-benar berubah
  if (leftSpeed == lastAppliedLeft && rightSpeed == lastAppliedRight) {
    return;
  }
  lastAppliedLeft  = leftSpeed;
  lastAppliedRight = rightSpeed;

  int outLeft  = INVERT_MOTOR_KIRI  ? -leftSpeed  : leftSpeed;
  int outRight = INVERT_MOTOR_KANAN ? -rightSpeed : rightSpeed;

  if (SWAP_KIRI_KANAN) {
    driveChannelA(outRight);
    driveChannelB(outLeft);
  } else {
    driveChannelA(outLeft);
    driveChannelB(outRight);
  }
}

void updateTankDrive() {
  if (RemoteXY.connect_flag == 0) {
    setTankMotors(0, 0);
    return;
  }

  int throttle = RemoteXY.slider_01;
  int steer    = RemoteXY.slider_02;

  if (abs(throttle) < SLIDER_DEADZONE) throttle = 0;
  if (abs(steer)    < SLIDER_DEADZONE) steer    = 0;

  steer = (int)(steer * TURN_SENSITIVITY);

  int mixLeft  = constrain(throttle + steer, -100, 100);
  int mixRight = constrain(throttle - steer, -100, 100);

  int pwmLeft  = map(mixLeft,  -100, 100, -currentMaxPWM, currentMaxPWM);
  int pwmRight = map(mixRight, -100, 100, -currentMaxPWM, currentMaxPWM);

  setTankMotors(pwmLeft, pwmRight);
}

//////////////////////////////////////////////
//      5. FUNGSI SPEED LEVEL (RADIO)       //
//////////////////////////////////////////////

void updateSpeedLevel() {
  if (RemoteXY.pushSwitch_01 && activeSpeedLevel != 1) activeSpeedLevel = 1;
  else if (RemoteXY.pushSwitch_02 && activeSpeedLevel != 2) activeSpeedLevel = 2;
  else if (RemoteXY.pushSwitch_03 && activeSpeedLevel != 3) activeSpeedLevel = 3;
  else if (RemoteXY.pushSwitch_04 && activeSpeedLevel != 4) activeSpeedLevel = 4;

  RemoteXY.pushSwitch_01 = (activeSpeedLevel == 1) ? 1 : 0;
  RemoteXY.pushSwitch_02 = (activeSpeedLevel == 2) ? 1 : 0;
  RemoteXY.pushSwitch_03 = (activeSpeedLevel == 3) ? 1 : 0;
  RemoteXY.pushSwitch_04 = (activeSpeedLevel == 4) ? 1 : 0;

  switch (activeSpeedLevel) {
    case 1: currentMaxPWM = SPEED_LVL_1; break;
    case 2: currentMaxPWM = SPEED_LVL_2; break;
    case 3: currentMaxPWM = SPEED_LVL_3; break;
    case 4: currentMaxPWM = SPEED_LVL_4; break;
  }
}

//////////////////////////////////////////////
//       6. FUNGSI SERVO LIFT & CAPIT       //
//////////////////////////////////////////////

void updateServos() {
  unsigned long now = millis();

  // --- Kontrol Servo Lift (Pin 5): NAIK & TURUN ---
  if (RemoteXY.pushSwitch_05 && !prevNaik)  RemoteXY.pushSwitch_06 = 0;
  if (RemoteXY.pushSwitch_06 && !prevTurun) RemoteXY.pushSwitch_05 = 0;

  if (now - lastLiftTimer >= LIFT_SPEED_MS) {
    lastLiftTimer = now;

    if (RemoteXY.pushSwitch_05 == 1) {
      if (currentLiftAngle < LIFT_MAX_ANGLE) {
        currentLiftAngle = min(currentLiftAngle + LIFT_STEP_DEG, LIFT_MAX_ANGLE);
        servoLift.write(currentLiftAngle);
      } else {
        RemoteXY.pushSwitch_05 = 0; // Otomatis OFF saat mentok atas
      }
    } 
    else if (RemoteXY.pushSwitch_06 == 1) {
      if (currentLiftAngle > LIFT_MIN_ANGLE) {
        currentLiftAngle = max(currentLiftAngle - LIFT_STEP_DEG, LIFT_MIN_ANGLE);
        servoLift.write(currentLiftAngle);
      } else {
        RemoteXY.pushSwitch_06 = 0; // Otomatis OFF saat mentok bawah
      }
    }
  }
  prevNaik  = RemoteXY.pushSwitch_05;
  prevTurun = RemoteXY.pushSwitch_06;

  // --- Kontrol Servo Capit (Pin 6): BUKA CAPIT & TUTUP CAPIT ---
  if (RemoteXY.pushSwitch_07 && !prevBuka)  RemoteXY.pushSwitch_08 = 0;
  if (RemoteXY.pushSwitch_08 && !prevTutup) RemoteXY.pushSwitch_07 = 0;

  if (now - lastCapitTimer >= CAPIT_SPEED_MS) {
    lastCapitTimer = now;

    if (RemoteXY.pushSwitch_07 == 1) {
      if (currentCapitAngle < CAPIT_OPEN_ANGLE) {
        currentCapitAngle = min(currentCapitAngle + CAPIT_STEP_DEG, CAPIT_OPEN_ANGLE);
        servoCapit.write(currentCapitAngle);
      } else {
        RemoteXY.pushSwitch_07 = 0; // Otomatis OFF saat terbuka penuh
      }
    } 
    else if (RemoteXY.pushSwitch_08 == 1) {
      if (currentCapitAngle > CAPIT_CLOSE_ANGLE) {
        currentCapitAngle = max(currentCapitAngle - CAPIT_STEP_DEG, CAPIT_CLOSE_ANGLE);
        servoCapit.write(currentCapitAngle);
      } else {
        RemoteXY.pushSwitch_08 = 0; // Otomatis OFF saat menjepit penuh
      }
    }
  }
  prevBuka  = RemoteXY.pushSwitch_07;
  prevTutup = RemoteXY.pushSwitch_08;
}

//////////////////////////////////////////////
//             7. SETUP & LOOP              //
//////////////////////////////////////////////

void setup() {
  RemoteXY_Init();

  // 1. Alokasikan timer terpisah agar Servo (50Hz) & Motor (1000Hz) tidak bentrok
  ESP32PWM::allocateTimer(0); // Khusus Servo Pin 5 & Pin 6
  ESP32PWM::allocateTimer(1); // Khusus Motor Kiri (Pin 4 & 3)
  ESP32PWM::allocateTimer(2); // Khusus Motor Kanan (Pin 1 & 0)

  // 2. Attach kedua Servo terlebih dahulu ke Timer 0
  servoLift.setPeriodHertz(50);
  servoLift.attach(PIN_SERVO_LIFT, 500, 2400);
  servoLift.write(currentLiftAngle);

  servoCapit.setPeriodHertz(50);
  servoCapit.attach(PIN_SERVO_CAPIT, 500, 2400);
  servoCapit.write(currentCapitAngle);

  // 3. Attach keempat pin Motor ke Timer 1 & 2 (resolusi 8-bit: 0-255)
  pwmIN1.attachPin(PIN_IN1, 1000, 8);
  pwmIN2.attachPin(PIN_IN2, 1000, 8);
  pwmIN3.attachPin(PIN_IN3, 1000, 8);
  pwmIN4.attachPin(PIN_IN4, 1000, 8);
  setTankMotors(0, 0);

  // Set default Speed Level 2
  RemoteXY.pushSwitch_02 = 1;
}

void loop() {
  RemoteXYEngine.handler();

  updateSpeedLevel();
  updateTankDrive();
  updateServos();
}
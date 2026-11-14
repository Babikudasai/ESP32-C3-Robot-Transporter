/*
   -- Transporter 2026 ESP32-C3 (Tank Drive + Lift & Gripper) --
   Board  : ESP32C3 Dev Module (USB CDC On Boot: Enabled jika pakai Serial Monitor)
   Driver : MX1650 / MX1508 Dual DC Motor Driver
   Library: RemoteXY (>= 4.1.10) & ESP32Servo
*/

//////////////////////////////////////////////
//      1. KONFIGURASI & TUNING ROBOT       //
//   (Ubah nilai di bagian ini saja)        //
//////////////////////////////////////////////

// --- Pinout ESP32-C3 ---
#define PIN_IN1         4   // Motor Kiri A
#define PIN_IN2         3   // Motor Kiri B
#define PIN_IN3         1   // Motor Kanan A
#define PIN_IN4         0   // Motor Kanan B

#define PIN_SERVO_LIFT  5   // Servo Angkat (Naik/Turun)
#define PIN_SERVO_CAPIT 6   // Servo Capit (Buka/Tutup)

// --- Kalibrasi Arah Gearbox Tank ---
// Ubah ke 'true' jika putaran roda terbalik tanpa perlu bongkar kabel
const bool INVERT_MOTOR_KIRI  = false;
const bool INVERT_MOTOR_KANAN = true;
const bool SWAP_KIRI_KANAN    = false; // Set 'true' jika motor kiri & kanan tertukar

// --- Tuning Kontrol & Kecepatan (PWM 0 - 255) ---
const int   SLIDER_DEADZONE   = 10;    // Abaikan geseran slider kecil (-10 s/d 10)
const float TURN_SENSITIVITY  = 0.85;  // Sensitivitas belok (0.5 = halus, 1.0 = agresif)

const int SPEED_LVL_1 = 110;  // Level 1: Mode presisi ambil/taruh objek
const int SPEED_LVL_2 = 160;  // Level 2: Kecepatan normal (Default saat nyala)
const int SPEED_LVL_3 = 210;  // Level 3: Kecepatan cepat
const int SPEED_LVL_4 = 255;  // Level 4: Full power

// --- Kalibrasi Sudut & Kecepatan Servo (0 - 180 derajat) ---
const int LIFT_MIN_ANGLE     = 20;   // Batas bawah arm lift
const int LIFT_MAX_ANGLE     = 140;  // Batas atas arm lift
const int LIFT_START_ANGLE   = 30;   // Posisi awal saat robot menyala
const int LIFT_SPEED_MS      = 15;   // Makin kecil = gerak lift makin cepat

const int CAPIT_OPEN_ANGLE   = 120;  // Sudut capit terbuka maksimal
const int CAPIT_CLOSE_ANGLE  = 35;   // Sudut capit menjepit maksimal (hindari angka mentok agar servo tidak panas)
const int CAPIT_START_ANGLE  = 110;  // Posisi awal capit (terbuka)
const int CAPIT_SPEED_MS     = 12;   // Makin kecil = gerak capit makin cepat

//////////////////////////////////////////////
//        2. REMOTEXY CONFIGURATION         //
//////////////////////////////////////////////

//#define REMOTEXY__DEBUGLOG
#define REMOTEXY_MODE__WIFI_POINT

#include <WiFi.h>
#include <RemoteXY.h>
#include <ESP32Servo.h>

#define REMOTEXY_WIFI_SSID "TRANSPORTER 2"
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
  int8_t slider_01;      // Throttle: -100 (Mundur) s/d 100 (Maju)
  int8_t slider_02;      // Steering: -100 (Kiri) s/d 100 (Kanan)
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
//      3. VARIABEL INTERNAL & OBJEK        //
//////////////////////////////////////////////

Servo servoLift;
Servo servoCapit;

int activeSpeedLevel = 2;
int currentMaxPWM    = SPEED_LVL_2;

int currentLiftAngle  = LIFT_START_ANGLE;
int currentCapitAngle = CAPIT_START_ANGLE;

unsigned long lastLiftTimer  = 0;
unsigned long lastCapitTimer = 0;

// Simpan state tombol sebelumnya untuk deteksi perubahan (edge detection)
uint8_t prevNaik = 0, prevTurun = 0;
uint8_t prevBuka = 0, prevTutup = 0;

//////////////////////////////////////////////
//         4. FUNGSI KONTROL MOTOR          //
//////////////////////////////////////////////

void driveChannelA(int pwm) {
  if (pwm > 0) {
    analogWrite(PIN_IN1, pwm);
    analogWrite(PIN_IN2, 0);
  } else if (pwm < 0) {
    analogWrite(PIN_IN1, 0);
    analogWrite(PIN_IN2, -pwm);
  } else {
    analogWrite(PIN_IN1, 0);
    analogWrite(PIN_IN2, 0);
  }
}

void driveChannelB(int pwm) {
  if (pwm > 0) {
    analogWrite(PIN_IN3, pwm);
    analogWrite(PIN_IN4, 0);
  } else if (pwm < 0) {
    analogWrite(PIN_IN3, 0);
    analogWrite(PIN_IN4, -pwm);
  } else {
    analogWrite(PIN_IN3, 0);
    analogWrite(PIN_IN4, 0);
  }
}

void setTankMotors(int leftSpeed, int rightSpeed) {
  if (INVERT_MOTOR_KIRI)  leftSpeed  = -leftSpeed;
  if (INVERT_MOTOR_KANAN) rightSpeed = -rightSpeed;

  if (SWAP_KIRI_KANAN) {
    driveChannelA(rightSpeed);
    driveChannelB(leftSpeed);
  } else {
    driveChannelA(leftSpeed);
    driveChannelB(rightSpeed);
  }
}

void updateTankDrive() {
  // Failsafe: Hentikan motor otomatis jika koneksi WiFi terputus
  if (RemoteXY.connect_flag == 0) {
    setTankMotors(0, 0);
    return;
  }

  int throttle = RemoteXY.slider_01;
  int steer    = RemoteXY.slider_02;

  // Terapkan deadzone
  if (abs(throttle) < SLIDER_DEADZONE) throttle = 0;
  if (abs(steer)    < SLIDER_DEADZONE) steer    = 0;

  steer = (int)(steer * TURN_SENSITIVITY);

  // Differential Tank Mixing (Arcade Drive)
  int mixLeft  = constrain(throttle + steer, -100, 100);
  int mixRight = constrain(throttle - steer, -100, 100);

  // Konversi skala -100..100 ke batas PWM Speed Level aktif
  int pwmLeft  = map(mixLeft,  -100, 100, -currentMaxPWM, currentMaxPWM);
  int pwmRight = map(mixRight, -100, 100, -currentMaxPWM, currentMaxPWM);

  setTankMotors(pwmLeft, pwmRight);
}

//////////////////////////////////////////////
//      5. FUNGSI SPEED LEVEL (RADIO)       //
//////////////////////////////////////////////

void updateSpeedLevel() {
  // Deteksi jika ada tombol level baru yang ditekan
  if (RemoteXY.pushSwitch_01 && activeSpeedLevel != 1) activeSpeedLevel = 1;
  else if (RemoteXY.pushSwitch_02 && activeSpeedLevel != 2) activeSpeedLevel = 2;
  else if (RemoteXY.pushSwitch_03 && activeSpeedLevel != 3) activeSpeedLevel = 3;
  else if (RemoteXY.pushSwitch_04 && activeSpeedLevel != 4) activeSpeedLevel = 4;

  // Pastikan hanya 1 indikator tombol level yang menyala di UI (Mutual Exclusion)
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

  // --- Interlock NAIK vs TURUN (tidak bisa aktif bersamaan) ---
  if (RemoteXY.pushSwitch_05 && !prevNaik)  RemoteXY.pushSwitch_06 = 0;
  if (RemoteXY.pushSwitch_06 && !prevTurun) RemoteXY.pushSwitch_05 = 0;
  prevNaik  = RemoteXY.pushSwitch_05;
  prevTurun = RemoteXY.pushSwitch_06;

  if (now - lastLiftTimer >= LIFT_SPEED_MS) {
    lastLiftTimer = now;
    if (RemoteXY.pushSwitch_05 == 1 && currentLiftAngle < LIFT_MAX_ANGLE) {
      currentLiftAngle++;
      servoLift.write(currentLiftAngle);
    } else if (RemoteXY.pushSwitch_06 == 1 && currentLiftAngle > LIFT_MIN_ANGLE) {
      currentLiftAngle--;
      servoLift.write(currentLiftAngle);
    }
  }

  // --- Interlock BUKA CAPIT vs TUTUP CAPIT ---
  if (RemoteXY.pushSwitch_07 && !prevBuka)  RemoteXY.pushSwitch_08 = 0;
  if (RemoteXY.pushSwitch_08 && !prevTutup) RemoteXY.pushSwitch_07 = 0;
  prevBuka  = RemoteXY.pushSwitch_07;
  prevTutup = RemoteXY.pushSwitch_08;

  if (now - lastCapitTimer >= CAPIT_SPEED_MS) {
    lastCapitTimer = now;
    if (RemoteXY.pushSwitch_07 == 1 && currentCapitAngle < CAPIT_OPEN_ANGLE) {
      currentCapitAngle++;
      servoCapit.write(currentCapitAngle);
    } else if (RemoteXY.pushSwitch_08 == 1 && currentCapitAngle > CAPIT_CLOSE_ANGLE) {
      currentCapitAngle--;
      servoCapit.write(currentCapitAngle);
    }
  }
}

//////////////////////////////////////////////
//             7. SETUP & LOOP              //
//////////////////////////////////////////////

void setup() {
  RemoteXY_Init();

  // Inisialisasi pin motor
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  setTankMotors(0, 0);

  // Inisialisasi Servo (50Hz standar)
  servoLift.setPeriodHertz(50);
  servoLift.attach(PIN_SERVO_LIFT, 500, 2400);
  servoLift.write(currentLiftAngle);

  servoCapit.setPeriodHertz(50);
  servoCapit.attach(PIN_SERVO_CAPIT, 500, 2400);
  servoCapit.write(currentCapitAngle);

  // Set default indikator UI ke Speed Level 2 saat pertama nyala
  RemoteXY.pushSwitch_02 = 1;
}

void loop() {
  RemoteXYEngine.handler();

  updateSpeedLevel();
  updateTankDrive();
  updateServos();
}
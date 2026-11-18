/*
   -- Robot Transporter freemium (ESP32-C3 + MX1650 + Dual Servo) --
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

#define PIN_SERVO_LIFT  5   // Servo Angkat (switch_01: TURUN / NAIK)
#define PIN_SERVO_CAPIT 6   // Servo Capit  (switch_02: TUTUP / BUKA)

// --- Kalibrasi Arah Gearbox Tank ---
const bool INVERT_MOTOR_KIRI  = false;
const bool INVERT_MOTOR_KANAN = true;  // Gearbox kanan posisi mirror
const bool SWAP_KIRI_KANAN    = false;

// --- Tuning Kontrol & Kecepatan ---
const int   SLIDER_DEADZONE   = 10;    // Abaikan geseran kecil (-10 s/d 10)
const float TURN_SENSITIVITY  = 0.85;  // Sensitivitas belok (0.5 = halus, 1.0 = agresif)
const int   DEFAULT_SPEED_PCT = 65;    // Posisi awal slider_03 saat robot nyala (0 - 100%)

// --- Kalibrasi Sudut & Kecepatan Servo (0 - 180 derajat) ---
const int LIFT_MIN_ANGLE     = 20;   // Posisi TURUN (switch_01 = OFF)
const int LIFT_MAX_ANGLE     = 140;  // Posisi NAIK  (switch_01 = ON)
const int LIFT_STEP_DEG      = 2;    // Kehalusan gerak lift (derajat per langkah)
const int LIFT_SPEED_MS      = 15;   // Kecepatan gerak lift (ms per langkah)

const int CAPIT_CLOSE_ANGLE  = 35;   // Posisi TUTUP CAPIT (switch_02 = OFF)
const int CAPIT_OPEN_ANGLE   = 120;  // Posisi BUKA CAPIT  (switch_02 = ON)
const int CAPIT_STEP_DEG     = 2;    // Kehalusan gerak capit (derajat per langkah)
const int CAPIT_SPEED_MS     = 12;   // Kecepatan gerak capit (ms per langkah)

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
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 120 bytes V19 
  { 255,5,0,0,0,113,0,19,0,0,0,0,31,2,106,200,200,84,1,1,
  5,0,4,12,64,7,86,13,9,13,65,32,2,26,2,28,122,23,52,52,
  66,43,9,0,2,26,31,31,78,65,73,75,0,84,85,82,85,78,0,4,
  55,26,8,155,127,36,69,13,160,2,26,2,49,122,23,21,103,66,43,9,
  0,2,26,31,31,66,85,75,65,32,67,65,80,73,84,0,84,85,84,85,
  80,32,67,65,80,73,84,0,4,31,5,34,11,58,4,83,11,128,2,26 };
  
struct {
  int8_t slider_01;    // Maju (+100) / Mundur (-100)
  uint8_t switch_01;   // 1 = NAIK, 0 = TURUN
  int8_t slider_02;    // Kanan (+100) / Kiri (-100)
  uint8_t switch_02;   // 1 = BUKA CAPIT, 0 = TUTUP CAPIT
  int8_t slider_03;    // Speed Limit: 0 s/d 100
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

//////////////////////////////////////////////
//      3. OBJEK PWM TERISOLASI & STATE     //
//////////////////////////////////////////////

Servo servoLift;
Servo servoCapit;

ESP32PWM pwmIN1;
ESP32PWM pwmIN2;
ESP32PWM pwmIN3;
ESP32PWM pwmIN4;

int lastAppliedLeft  = -999;
int lastAppliedRight = -999;

int currentLiftAngle  = LIFT_MIN_ANGLE;
int currentCapitAngle = CAPIT_OPEN_ANGLE;

unsigned long lastLiftTimer  = 0;
unsigned long lastCapitTimer = 0;

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
  // Failsafe otomatis jika koneksi WiFi terputus
  if (RemoteXY.connect_flag == 0) {
    setTankMotors(0, 0);
    return;
  }

  // Ambil batas PWM maksimal dari slider_03 (0 - 100 -> 0 - 255)
  int speedPct = constrain(RemoteXY.slider_03, 0, 100);
  int maxPWM   = map(speedPct, 0, 100, 0, 255);

  int throttle = RemoteXY.slider_01;
  int steer    = RemoteXY.slider_02;

  if (abs(throttle) < SLIDER_DEADZONE) throttle = 0;
  if (abs(steer)    < SLIDER_DEADZONE) steer    = 0;

  steer = (int)(steer * TURN_SENSITIVITY);

  // Differential Tank Mixing
  int mixLeft  = constrain(throttle + steer, -100, 100);
  int mixRight = constrain(throttle - steer, -100, 100);

  int pwmLeft  = map(mixLeft,  -100, 100, -maxPWM, maxPWM);
  int pwmRight = map(mixRight, -100, 100, -maxPWM, maxPWM);

  setTankMotors(pwmLeft, pwmRight);
}

//////////////////////////////////////////////
//       5. FUNGSI SERVO LIFT & CAPIT       //
//////////////////////////////////////////////

void updateServos() {
  unsigned long now = millis();

  // --- Servo Lift (Pin 5): switch_01 (1 = NAIK, 0 = TURUN) ---
  int targetLift = (RemoteXY.switch_01 == 1) ? LIFT_MAX_ANGLE : LIFT_MIN_ANGLE;

  if (now - lastLiftTimer >= LIFT_SPEED_MS) {
    lastLiftTimer = now;
    if (currentLiftAngle < targetLift) {
      currentLiftAngle = min(currentLiftAngle + LIFT_STEP_DEG, targetLift);
      servoLift.write(currentLiftAngle);
    } else if (currentLiftAngle > targetLift) {
      currentLiftAngle = max(currentLiftAngle - LIFT_STEP_DEG, targetLift);
      servoLift.write(currentLiftAngle);
    }
  }

  // --- Servo Capit (Pin 6): switch_02 (1 = BUKA CAPIT, 0 = TUTUP CAPIT) ---
  int targetCapit = (RemoteXY.switch_02 == 1) ? CAPIT_OPEN_ANGLE : CAPIT_CLOSE_ANGLE;

  if (now - lastCapitTimer >= CAPIT_SPEED_MS) {
    lastCapitTimer = now;
    if (currentCapitAngle < targetCapit) {
      currentCapitAngle = min(currentCapitAngle + CAPIT_STEP_DEG, targetCapit);
      servoCapit.write(currentCapitAngle);
    } else if (currentCapitAngle > targetCapit) {
      currentCapitAngle = max(currentCapitAngle - CAPIT_STEP_DEG, targetCapit);
      servoCapit.write(currentCapitAngle);
    }
  }
}

//////////////////////////////////////////////
//             6. SETUP & LOOP              //
//////////////////////////////////////////////

void setup() {
  RemoteXY_Init();

  // 1. Isolasi Timer PWM agar Servo (50Hz) & Motor (1000Hz) tidak bentrok
  ESP32PWM::allocateTimer(0); // Khusus Servo Pin 5 & 6
  ESP32PWM::allocateTimer(1); // Khusus Motor Kiri (Pin 4 & 3)
  ESP32PWM::allocateTimer(2); // Khusus Motor Kanan (Pin 1 & 0)

  // 2. Attach kedua Servo terlebih dahulu ke Timer 0
  servoLift.setPeriodHertz(50);
  servoLift.attach(PIN_SERVO_LIFT, 500, 2400);
  servoLift.write(currentLiftAngle);

  servoCapit.setPeriodHertz(50);
  servoCapit.attach(PIN_SERVO_CAPIT, 500, 2400);
  servoCapit.write(currentCapitAngle);

  // 3. Attach keempat pin Motor ke Timer 1 & 2
  pwmIN1.attachPin(PIN_IN1, 1000, 8);
  pwmIN2.attachPin(PIN_IN2, 1000, 8);
  pwmIN3.attachPin(PIN_IN3, 1000, 8);
  pwmIN4.attachPin(PIN_IN4, 1000, 8);
  setTankMotors(0, 0);

  // 4. Sinkronkan posisi awal UI saat HP pertama kali terkoneksi
  RemoteXY.slider_03 = DEFAULT_SPEED_PCT; // Langsung siap jalan di 65% speed
  RemoteXY.switch_01 = 0;                 // Posisi awal: TURUN
  RemoteXY.switch_02 = 1;                 // Posisi awal: BUKA CAPIT
}

void loop() {
  RemoteXYEngine.handler();

  updateTankDrive();
  updateServos();
}
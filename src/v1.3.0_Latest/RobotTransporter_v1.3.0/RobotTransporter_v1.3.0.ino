#define REMOTEXY_MODE__WIFI_POINT
#include <WiFi.h>
#include <RemoteXY.h>

#define REMOTEXY_WIFI_SSID "TRANSPORTER 2"
#define REMOTEXY_WIFI_PASSWORD "12345678"
#define REMOTEXY_SERVER_PORT 6377

// --- PENGATURAN PIN (Gampang diubah kalau mau pindah pin GPIO) ---
#define PIN_MOTOR_KIRI_IN1   0
#define PIN_MOTOR_KIRI_IN2   1
#define PIN_MOTOR_KANAN_IN3  7
#define PIN_MOTOR_KANAN_IN4  4

// --- KONFIGURASI TAMPILAN REMOTEXY ---
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
  int8_t slider_01; // Slider Atas-Bawah (Maju-Mundur)
  int8_t slider_02; // Slider Kiri-Kanan (Diabaikan dulu)
  uint8_t pushSwitch_01; uint8_t pushSwitch_02; uint8_t pushSwitch_03; uint8_t pushSwitch_04;
  uint8_t pushSwitch_05; uint8_t pushSwitch_06; uint8_t pushSwitch_07; uint8_t pushSwitch_08;
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

// --- VARIABEL PENGATURAN ---
const int DEADZONE = 10; // Batas toleransi slider kembali ke tengah
float speedMultiplier = 0.50; // Kecepatan tetap 50% untuk kalibrasi aman

void setup() {
  RemoteXY_Init();  
  
  // Atur pin motor sebagai output
  pinMode(PIN_MOTOR_KIRI_IN1, OUTPUT); 
  pinMode(PIN_MOTOR_KIRI_IN2, OUTPUT);
  pinMode(PIN_MOTOR_KANAN_IN3, OUTPUT); 
  pinMode(PIN_MOTOR_KANAN_IN4, OUTPUT);
}

void loop() { 
  RemoteXYEngine.handler();   

  // Jalankan fungsi khusus pergerakan motor maju-mundur saja
  gerakkanMotorMajuMundur();
}

// ============================================================
// FUNGSI MODULAR: KONTROL MOTOR MAJU & MUNDUR SAJA
// ============================================================
void gerakkanMotorMajuMundur() {
  int throttle = RemoteXY.slider_01; // Membaca gerakan slider atas-bawah

  // Terapkan Deadzone: Jika slider di sekitar tengah, paksa nilai jadi 0 (Berhenti)
  if (throttle > -DEADZONE && throttle < DEADZONE) {
    throttle = 0;
  }

  // Hitung kekuatan kecepatan motor (menggunakan skala PWM 0 sampai 255)
  int pwmSpeed = map(abs(throttle) * speedMultiplier, 0, 100, 0, 255);

  // Logika pergerakan berdasarkan posisi slider
  if (throttle > 0) {
    // KONDISI: MAJU
    // Motor Kiri Putar Maju
    analogWrite(PIN_MOTOR_KIRI_IN1, 0); 
    analogWrite(PIN_MOTOR_KIRI_IN2, pwmSpeed); 
    // Motor Kanan Putar Maju
    analogWrite(PIN_MOTOR_KANAN_IN3, 0); 
    analogWrite(PIN_MOTOR_KANAN_IN4, pwmSpeed); 
  } 
  else if (throttle < 0) {
    // KONDISI: MUNDUR
    // Motor Kiri Putar Mundur
    analogWrite(PIN_MOTOR_KIRI_IN1, pwmSpeed); 
    analogWrite(PIN_MOTOR_KIRI_IN2, 0); 
    // Motor Kanan Putar Mundur
    analogWrite(PIN_MOTOR_KANAN_IN3, pwmSpeed); 
    analogWrite(PIN_MOTOR_KANAN_IN4, 0); 
  } 
  else {
    // KONDISI: BERHENTI TOTAL (AUTO-OFF)
    analogWrite(PIN_MOTOR_KIRI_IN1, 0); 
    analogWrite(PIN_MOTOR_KIRI_IN2, 0); 
    analogWrite(PIN_MOTOR_KANAN_IN3, 0); 
    analogWrite(PIN_MOTOR_KANAN_IN4, 0); 
  }
}
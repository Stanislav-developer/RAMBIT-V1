#include <AlfredoCRSF.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>

HardwareSerial crsfSerial(2);
AlfredoCRSF crsf;

// --- Піни Сервоприводів (Фліпери) ---
Servo rightFlipperServo;
Servo leftFlipperServo;
#define SERVO_R_PIN 18
#define SERVO_L_PIN 19

// --- Піни CRSF (ELRS) ---
#define RX_PIN 16
#define TX_PIN 17

// --- Піни Драйвера 1 (Правий) ---
#define R_EN_PIN 27
#define R_RPWM_PIN 26
#define R_LPWM_PIN 25

// --- Піни Драйвера 2 (Лівий) ---
#define L_EN_PIN 21
#define L_RPWM_PIN 32
#define L_LPWM_PIN 33

// --- Піни Периферії ---
#define LED_PIN 13
#define BUZZER_PIN 23

// Параметри ШІМ для моторів
const int pwmFreq = 20000;
const int pwmRes = 8;

unsigned long lastPacketTime = 0;
unsigned long lastPrint = 0;

// =======================================================
// ⚙️ ГОЛОВНІ НАЛАШТУВАННЯ КЕРУВАННЯ
// =======================================================

// 1. НАЛАШТУВАННЯ КАНАЛІВ
const int CH_MAIN_1 = 4;   // Стік 1 (Стандартно Газ)
const int CH_MAIN_2 = 2;   // Стік 2 (Стандартно Кермо)
const int CH_L_FLIP = 6;   // Канал лівого тумблера
const int CH_R_FLIP = 7;   // Канал правого тумблера
const int CH_MODE_SW = 8;  // ⚠️ Канал SD (Перемикання режимів Газ/Кермо)

// 2. ІНВЕРСІЯ МОТОРІВ ТА СТІКІВ
bool invertLeftMotor = false;
bool invertRightMotor = false;
bool invertSteering = true;

// 3. НАЛАШТУВАННЯ СЕРВОПРИВОДІВ
bool swapFlipperSides = true; // ⚠️ TRUE - Міняє місцями лівий і правий фліпери
bool invertServoR = false;
bool invertServoL = true;

// 4. СТАТУС РЕЖИМУ (False = Стандарт, True = Поміняні місцями)
bool isSwappedMode = false;
// =======================================================

void setServo(char side, int angle);

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // --- ЖОРСТКЕ БЛОКУВАННЯ ПРИ СТАРТІ ---
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, LOW);
  digitalWrite(L_EN_PIN, LOW);

  delay(1000);

  Serial.begin(115200);

  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);

  // --- НАЛАШТУВАННЯ СЕРВО ---
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  rightFlipperServo.setPeriodHertz(50);
  leftFlipperServo.setPeriodHertz(50);
  rightFlipperServo.attach(SERVO_R_PIN, 500, 2400);
  leftFlipperServo.attach(SERVO_L_PIN, 500, 2400);

  setServo('R', 0);
  setServo('L', 0);

  // --- НАЛАШТУВАННЯ МОТОРІВ ---
  ledcAttach(R_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(R_LPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_LPWM_PIN, pwmFreq, pwmRes);

  ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
  ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);

  // Мелодія готовності
  tone(BUZZER_PIN, 2000, 100);
  delay(150);
  tone(BUZZER_PIN, 2500, 150);
}

void loop() {
  crsf.update();

  // 1. Зчитуємо сирі дані з пульта
  int rawCh1 = crsf.getChannel(CH_MAIN_1);
  int rawCh2 = crsf.getChannel(CH_MAIN_2);
  int leftFlipVal = crsf.getChannel(CH_L_FLIP);
  int rightFlipVal = crsf.getChannel(CH_R_FLIP);
  bool requestedMode = crsf.getChannel(CH_MODE_SW) > 1500; // Читаємо тумблер SD

  // --- ВАЛІДАЦІЯ ДАНИХ ТА FAILSAFE ---
  if (rawCh1 > 800 && rawCh2 > 800) {
    lastPacketTime = millis();
  }

  if (millis() - lastPacketTime > 500) {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
    digitalWrite(R_EN_PIN, LOW); digitalWrite(L_EN_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    return;
  }

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  // === ЛОГІКА РЕЖИМІВ (ГАЛЗ ТА КЕРМО) ===
  int throttleVal, steerVal;

  if (isSwappedMode) {
    throttleVal = rawCh2; // Газ тепер на іншому стіку
    steerVal = rawCh1;    // Кермо також
  } else {
    throttleVal = rawCh1; // Стандартно
    steerVal = rawCh2;
  }

  // Переводимо у ШІМ і застосовуємо мертву зону
  int throttle = map(throttleVal, 1000, 2000, -255, 255);
  int steer = map(steerVal, 1000, 2000, -255, 255);
  if (abs(throttle) < 30) throttle = 0;
  if (abs(steer) < 30) steer = 0;

  // === БЕЗПЕЧНЕ ПЕРЕМИКАННЯ РЕЖИМІВ ===
  // Якщо тумблер SD перемкнули, але режим ще не змінився
  if (requestedMode != isSwappedMode) {
    // Дозволяємо перемикання ТІЛЬКИ якщо робот стоїть
    if (throttle == 0 && steer == 0) {
      isSwappedMode = requestedMode;

      // Звукова індикація режиму
      if (isSwappedMode) {
        tone(BUZZER_PIN, 3000, 200); // Високий пік - Альтернативний режим
      } else {
        tone(BUZZER_PIN, 1000, 200); // Низький пік - Стандартний режим
      }
    }
  }

  // === КЕРУВАННЯ РУХОМ ===
  if (invertSteering) steer = -steer;

  int leftMotorSpeed = throttle + steer;
  int rightMotorSpeed = throttle - steer;

  if (invertLeftMotor) leftMotorSpeed = -leftMotorSpeed;
  if (invertRightMotor) rightMotorSpeed = -rightMotorSpeed;

  leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
  rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);

  // Лівий мотор
  if (leftMotorSpeed > 0) {
    ledcWrite(L_LPWM_PIN, 0); ledcWrite(L_RPWM_PIN, leftMotorSpeed);
  } else if (leftMotorSpeed < 0) {
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, abs(leftMotorSpeed));
  } else {
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
  }

  // Правий мотор
  if (rightMotorSpeed > 0) {
    ledcWrite(R_LPWM_PIN, 0); ledcWrite(R_RPWM_PIN, rightMotorSpeed);
  } else if (rightMotorSpeed < 0) {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, abs(rightMotorSpeed));
  } else {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
  }

  // === КЕРУВАННЯ ФЛІПЕРАМИ ===
  // Свопінг сторін (лівий тумблер керує правим фліпером і навпаки)
  if (swapFlipperSides) {
    int temp = leftFlipVal;
    leftFlipVal = rightFlipVal;
    rightFlipVal = temp;
  }

  int targetLeftAngle = 0;
  if (leftFlipVal > 1700) targetLeftAngle = 82;
  else if (leftFlipVal > 1300) targetLeftAngle = 22;
  else targetLeftAngle = 0;

  int targetRightAngle = 0;
  if (rightFlipVal > 1700) targetRightAngle = 75;
  else if (rightFlipVal > 1300) targetRightAngle = 20;
  else targetRightAngle = 0;

  setServo('L', targetLeftAngle);
  setServo('R', targetRightAngle);

  // --- ТЕЛЕМЕТРІЯ ---
  if (millis() - lastPrint > 200) {
    Serial.print("Mode: "); Serial.print(isSwappedMode ? "SWAPPED" : "NORMAL");
    Serial.print(" | T:"); Serial.print(throttle);
    Serial.print(" | S:"); Serial.print(steer);
    Serial.print(" | FLIP_L:"); Serial.print(targetLeftAngle);
    Serial.print("° | FLIP_R:"); Serial.print(targetRightAngle);
    Serial.print("° => L_M:"); Serial.print(leftMotorSpeed);
    Serial.print(" R_M:"); Serial.println(rightMotorSpeed);
    lastPrint = millis();
  }
}

// =======================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// =======================================================
void setServo(char side, int angle) {
  if (side == 'R') {
    if (invertServoR) rightFlipperServo.write(180 - angle);
    else rightFlipperServo.write(angle);
  }
  else if (side == 'L') {
    if (invertServoL) leftFlipperServo.write(180 - angle);
    else leftFlipperServo.write(angle);
  }
}
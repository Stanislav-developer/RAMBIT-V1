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

unsigned long lastPrint = 0; 

// =======================================================
// ⚙️ ГОЛОВНІ НАЛАШТУВАННЯ КЕРУВАННЯ
// =======================================================

// 1. НАЛАШТУВАННЯ КАНАЛІВ (AETR)
const int CH_AIL = 1;      
const int CH_ELE = 2;      
const int CH_THR = 3;      
const int CH_RUD = 4;      
const int CH_ARM = 5;      
const int CH_L_FLIP = 6;   
const int CH_R_FLIP = 7;   
const int CH_DRIVE_MODE = 8; // SD: Зміна режиму їзди (Танк / Адаптив) ⚠️
const int CH_SPEED = 9;    
const int CH_SWAP = 10;    

// 2. ІНВЕРСІЯ МОТОРІВ ТА СТІКІВ (Не чіпаємо)
bool invertLeftMotor = true;  
bool invertRightMotor = false;  
bool invertSteering = true; 

// 3. НАЛАШТУВАННЯ СЕРВОПРИВОДІВ
bool swapFlipperSides = true; 
bool invertServoR = false; 
bool invertServoL = true;  

// 4. СТАНОВІ ЗМІННІ 
bool isElrsConnected = false;
bool isRobotLocked = true;    
int lastEncoderVal = 1000;    

// 5. ЗМІННІ ДЛЯ КНОПОК
bool isSwappedMode = false;   
bool lastSwapButtonState = false; 
bool isAdaptiveMode = false;      
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

  // 🔊 БРУТАЛЬНИЙ ЗВУК СТАРТУ
  tone(BUZZER_PIN, 800, 200); delay(250);
  tone(BUZZER_PIN, 1200, 200); delay(250);
  tone(BUZZER_PIN, 600, 400);  delay(400);
}

void loop() {
  crsf.update();

  int rawThr = crsf.getChannel(CH_THR); 
  int rawAil = crsf.getChannel(CH_AIL); 
  int rawEle = crsf.getChannel(CH_ELE); 
  int rawRud = crsf.getChannel(CH_RUD); 

  int armVal = crsf.getChannel(CH_ARM);
  int leftFlipVal = crsf.getChannel(CH_L_FLIP);   
  int rightFlipVal = crsf.getChannel(CH_R_FLIP);
  int speedEncoderVal = crsf.getChannel(CH_SPEED);
  int swapModeVal = crsf.getChannel(CH_SWAP); 
  int driveModeVal = crsf.getChannel(CH_DRIVE_MODE);

  // --- 1. ЛОГІКА ЗВ'ЯЗКУ ---
  bool isCurrentlyConnected = crsf.isLinkUp();

  if (isCurrentlyConnected && !isElrsConnected) {
    isElrsConnected = true;
    tone(BUZZER_PIN, 1000, 100); delay(150); 
    tone(BUZZER_PIN, 1500, 200);
  } 
  else if (!isCurrentlyConnected && isElrsConnected) {
    isElrsConnected = false;
    isRobotLocked = true; 
    tone(BUZZER_PIN, 800, 200); delay(250);
    tone(BUZZER_PIN, 400, 300);
  }

  if (!isCurrentlyConnected) {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
    digitalWrite(R_EN_PIN, LOW); digitalWrite(L_EN_PIN, LOW); 
    digitalWrite(LED_PIN, LOW); 
    return; 
  }

  // --- 2. ЛОГІКА ЗАПОБІЖНИКА (ARM) ---
  bool currentLockState = (armVal < 1500); 
  
  if (currentLockState != isRobotLocked) {
    isRobotLocked = currentLockState;
    if (isRobotLocked) {
      tone(BUZZER_PIN, 800, 150); delay(200); 
      tone(BUZZER_PIN, 400, 250);
    } else {
      tone(BUZZER_PIN, 1500, 100); delay(150); 
      tone(BUZZER_PIN, 2000, 100); delay(150); 
      tone(BUZZER_PIN, 2500, 250);
    }
  }

  if (isRobotLocked) {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
    digitalWrite(R_EN_PIN, LOW); digitalWrite(L_EN_PIN, LOW);
    digitalWrite(LED_PIN, HIGH); 
    return;
  }

  digitalWrite(R_EN_PIN, HIGH); 
  digitalWrite(L_EN_PIN, HIGH); 

  // --- 3. ЕНКОДЕР (ОБМЕЖЕННЯ ШВИДКОСТІ) ---
  if (abs(speedEncoderVal - lastEncoderVal) > 15) {
    lastEncoderVal = speedEncoderVal;
    tone(BUZZER_PIN, 4000, 15); 
  }
  
  int maxSpeedLimit = map(speedEncoderVal, 1000, 2000, 0, 255);
  maxSpeedLimit = constrain(maxSpeedLimit, 0, 255);


  // --- 4. ЗМІНА РОБОЧИХ СТІКІВ (КНОПКА SE) ---
  bool currentSwapButtonState = (swapModeVal > 1500);
  if (currentSwapButtonState && !lastSwapButtonState) {
    isSwappedMode = !isSwappedMode; 
    if (isSwappedMode) tone(BUZZER_PIN, 3000, 100); 
    else tone(BUZZER_PIN, 2000, 100); 
  }
  lastSwapButtonState = currentSwapButtonState; 

  // --- 5. РЕЖИМ ЇЗДИ ТАНК/АДАПТИВ (ТУМБЛЕР SD) ---
  bool currentDriveMode = (driveModeVal > 1500);
  if (currentDriveMode != isAdaptiveMode) {
    isAdaptiveMode = currentDriveMode;
    if (isAdaptiveMode) {
      tone(BUZZER_PIN, 3500, 150); // Високий пік - Адаптивний (Гоночний) режим
    } else {
      tone(BUZZER_PIN, 2500, 150); // Нижчий пік - Танковий (Бойовий) режим
    }
  }

  // === 6. КЕРУВАННЯ РУХОМ (ПОВНІСТЮ ОНОВЛЕНЕ) ===
  int activeThr = isSwappedMode ? rawEle : rawThr;
  int activeSteer = isSwappedMode ? rawRud : rawAil;

  int throttle = map(activeThr, 1000, 2000, -255, 255);
  int steer = map(activeSteer, 1000, 2000, -255, 255);

  if (abs(throttle) < 30) throttle = 0;
  if (abs(steer) < 30) steer = 0;

  if (invertSteering) steer = -steer;

  int leftMotorSpeed = 0;
  int rightMotorSpeed = 0;

  if (isAdaptiveMode) {
    // 🏎 АДАПТИВНИЙ РЕЖИМ (Гонка)
    if (throttle == 0) {
      // Розворот на місці (якщо газ відпущено)
      leftMotorSpeed = steer;
      rightMotorSpeed = -steer;
    } else {
      // Розраховуємо коефіцієнт повороту
      float turnFactor = abs(steer) / 255.0; 
      // Внутрішнє колесо скидає швидкість, але падає максимум до 25% (завдяки 0.75) від швидкості зовнішнього
      int innerSpeed = throttle * (1.0 - (turnFactor * 0.75)); 

      if (steer > 0) { // Праворуч
        leftMotorSpeed = throttle;       // Зовнішнє на максимумі
        rightMotorSpeed = innerSpeed;    // Внутрішнє пригальмовує
      } else {         // Ліворуч
        rightMotorSpeed = throttle;      // Зовнішнє на максимумі
        leftMotorSpeed = innerSpeed;     // Внутрішнє пригальмовує
      }
    }
  } else {
    // ⚔️ ТАНКОВИЙ РЕЖИМ (Бій)
    leftMotorSpeed = throttle + steer;
    rightMotorSpeed = throttle - steer;
  }

  // Інверсія моторів
  if (invertLeftMotor) leftMotorSpeed = -leftMotorSpeed;
  if (invertRightMotor) rightMotorSpeed = -rightMotorSpeed;

  // Обрізаємо значення, щоб не вийшли за рамки ШІМ
  leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
  rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);

  // Застосовуємо ліміт швидкості від енкодера
  leftMotorSpeed = (leftMotorSpeed * maxSpeedLimit) / 255;
  rightMotorSpeed = (rightMotorSpeed * maxSpeedLimit) / 255;

  // Подача на драйвери
  if (leftMotorSpeed > 0) {
    ledcWrite(L_LPWM_PIN, 0); ledcWrite(L_RPWM_PIN, leftMotorSpeed); 
  } else if (leftMotorSpeed < 0) {
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, abs(leftMotorSpeed)); 
  } else {
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0); 
  }

  if (rightMotorSpeed > 0) {
    ledcWrite(R_LPWM_PIN, 0); ledcWrite(R_RPWM_PIN, rightMotorSpeed); 
  } else if (rightMotorSpeed < 0) {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, abs(rightMotorSpeed)); 
  } else {
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0); 
  }

  // === 7. КЕРУВАННЯ ФЛІПЕРАМИ ===
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
    Serial.print("Drive: "); Serial.print(isAdaptiveMode ? "RACING" : "TANK");
    Serial.print(" | Limit: "); Serial.print(maxSpeedLimit);
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
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
const int CH_THROTTLE = 4; // Канал руху Вперед/Назад 
const int CH_STEER = 2;    // Канал поворотів Вліво/Вправо 
const int CH_ARM = 5;      // Канал тумблера для фліпера

// 2. ІНВЕРСІЯ МОТОРІВ
bool invertLeftMotor = false;  
bool invertRightMotor = true;  
// =======================================================

void setup() {
  
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); 
  // --- 2. НАЛАШТУВАННЯ МОТОРІВ (ЖОРСТКЕ БЛОКУВАННЯ ПРИ СТАРТІ) ---
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  // Драйвери фізично знеструмлені!
  digitalWrite(R_EN_PIN, LOW); 
  digitalWrite(L_EN_PIN, LOW); 

  delay(1000);

  Serial.begin(115200);

  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);

  // --- 1. НАЛАШТУВАННЯ СЕРВО (ОБОВ'ЯЗКОВО ПЕРШИМИ!) ---
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  rightFlipperServo.setPeriodHertz(50);
  leftFlipperServo.setPeriodHertz(50);
  rightFlipperServo.attach(SERVO_R_PIN, 500, 2400);
  leftFlipperServo.attach(SERVO_L_PIN, 500, 2400);

  

  ledcAttach(R_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(R_LPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_LPWM_PIN, pwmFreq, pwmRes);

  // Глушимо ШІМ про всяк випадок
  ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
  ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);

  tone(BUZZER_PIN, 2000, 200);
}

void loop() {
  // Зчитуємо дані з буфера (але поки не віримо їм)
  crsf.update();

  // Зчитуємо канали
  int throttleVal = crsf.getChannel(CH_THROTTLE); 
  int steerVal = crsf.getChannel(CH_STEER);    
  int switchVal = crsf.getChannel(CH_ARM);   

  // --- ВАЛІДАЦІЯ ДАНИХ (Перевірка на сміття) ---
  // Якщо пульт віддає адекватні цифри (більше 800), значить зв'язок 100% є
  if (throttleVal > 800 && steerVal > 800) {
    lastPacketTime = millis();
  }

  // --- БРОНЬОВАНИЙ FAILSAFE ---
  // Якщо нормальних даних не було понад 500 мс (або взагалі ще не було)
  if (millis() - lastPacketTime > 500) {
    // 1. Програмно зупиняємо ШІМ
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
    ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
    
    // 2. ФІЗИЧНО БЛОКУЄМО ДРАЙВЕРИ
    digitalWrite(R_EN_PIN, LOW); 
    digitalWrite(L_EN_PIN, LOW); 
    
    digitalWrite(LED_PIN, LOW); // Гасимо діод
    return; // ЗАВЕРШУЄМО LOOP ТУТ. Мотори ніколи не запустяться.
  } 

  // --- ЯКЩО ЗВ'ЯЗОК Є І ДАНІ ЧИСТІ ---
  digitalWrite(LED_PIN, HIGH);  // Світимо діодом
  digitalWrite(R_EN_PIN, HIGH); // Фізично вмикаємо правий драйвер
  digitalWrite(L_EN_PIN, HIGH); // Фізично вмикаємо лівий драйвер

  // Переводимо у ШІМ
  int throttle = map(throttleVal, 1000, 2000, -255, 255);
  int steer = map(steerVal, 1000, 2000, -255, 255);

  // Мертва зона стіків
  if (abs(throttle) < 30) throttle = 0;
  if (abs(steer) < 30) steer = 0;

  // Мікшуємо газ і поворот
  int leftMotorSpeed = throttle + steer;
  int rightMotorSpeed = throttle - steer;

  // --- ПРОГРАМНА ІНВЕРСІЯ МОТОРІВ ---
  if (invertLeftMotor) leftMotorSpeed = -leftMotorSpeed;
  if (invertRightMotor) rightMotorSpeed = -rightMotorSpeed;

  leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
  rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);

  // --- Керування Лівим ---
  if (leftMotorSpeed > 0) {
    ledcWrite(L_LPWM_PIN, 0);
    ledcWrite(L_RPWM_PIN, leftMotorSpeed); 
  } else if (leftMotorSpeed < 0) {
    ledcWrite(L_RPWM_PIN, 0);
    ledcWrite(L_LPWM_PIN, abs(leftMotorSpeed)); 
  } else {
    ledcWrite(L_RPWM_PIN, 0);
    ledcWrite(L_LPWM_PIN, 0); 
  }

  // --- Керування Правим ---
  if (rightMotorSpeed > 0) {
    ledcWrite(R_LPWM_PIN, 0);
    ledcWrite(R_RPWM_PIN, rightMotorSpeed); 
  } else if (rightMotorSpeed < 0) {
    ledcWrite(R_RPWM_PIN, 0);
    ledcWrite(R_LPWM_PIN, abs(rightMotorSpeed)); 
  } else {
    ledcWrite(R_RPWM_PIN, 0);
    ledcWrite(R_LPWM_PIN, 0); 
  }

  // --- Керування Серво ---
  int targetAngle = (switchVal > 1500) ? 90 : 0;
  rightFlipperServo.write(targetAngle);
  leftFlipperServo.write(targetAngle); 

  // Телеметрія
  if (millis() - lastPrint > 200) {
    Serial.print("T(CH"); Serial.print(CH_THROTTLE); Serial.print("): "); Serial.print(throttleVal);
    Serial.print(" | S(CH"); Serial.print(CH_STEER); Serial.print("): "); Serial.print(steerVal);
    Serial.print(" => L_M: "); Serial.print(leftMotorSpeed);
    Serial.print(" R_M: "); Serial.println(rightMotorSpeed);
    lastPrint = millis();
  }
}
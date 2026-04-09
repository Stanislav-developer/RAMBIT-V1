// --- Налаштування пінів ---

// Потенціометр
const int potPin = 34;

// Драйвер 1 (Лівий мотор)
const int en1Pin = 27;     // Дозвіл роботи (об'єднані R_EN та L_EN)
const int rPwm1Pin = 25;   // Рух вперед
const int lPwm1Pin = 26;   // Рух назад

// Драйвер 2 (Правий мотор)
const int en2Pin = 14;     // Дозвіл роботи (об'єднані R_EN та L_EN)
const int rPwm2Pin = 32;   // Рух вперед
const int lPwm2Pin = 33;   // Рух назад

// Параметри ШІМ
const int pwmFreq = 20000; // Частота 20 кГц (для уникнення писку двигунів)
const int pwmRes = 8;      // 8 біт (значення швидкості від 0 до 255)

void setup() {
  Serial.begin(115200);

  // Налаштовуємо піни активації як виходи
  pinMode(en1Pin, OUTPUT);
  pinMode(en2Pin, OUTPUT);

  // Ініціалізуємо ШІМ для всіх керуючих пінів (Синтаксис ESP32 Core 3.x.x)
  ledcAttach(rPwm1Pin, pwmFreq, pwmRes);
  ledcAttach(lPwm1Pin, pwmFreq, pwmRes);
  ledcAttach(rPwm2Pin, pwmFreq, pwmRes);
  ledcAttach(lPwm2Pin, pwmFreq, pwmRes);

  // Вмикаємо обидва драйвери
  digitalWrite(en1Pin, HIGH);
  digitalWrite(en2Pin, HIGH);
}

void loop() {
  // 1. Зчитуємо аналогове значення з потенціометра (діапазон ESP32: 0 - 4095)
  int potValue = analogRead(potPin);

  // 2. Конвертуємо значення АЦП у формат ШІМ (діапазон: 0 - 255)
  int speed = map(potValue, 0, 4095, 0, 255);

  // 3. Відправляємо швидкість на обидва мотори (рух вперед)
  
  // Керування Драйвером 1
  ledcWrite(lPwm1Pin, 0);       // Глушимо реверс
  ledcWrite(rPwm1Pin, speed);   // Задаємо швидкість вперед

  // Керування Драйвером 2
  ledcWrite(lPwm2Pin, 0);       // Глушимо реверс
  ledcWrite(rPwm2Pin, speed);   // Задаємо швидкість вперед

  // 4. Виведення даних для діагностики
  Serial.print("ADC: ");
  Serial.print(potValue);
  Serial.print(" | Speed PWM: ");
  Serial.println(speed);

  // Затримка для стабільності роботи АЦП
  delay(15);
}
// --- Налаштування пінів ---
const int btnPin = 4; // Пін кнопки

// --- Піни Драйвера 1 (Правий) ---
#define R_EN_PIN 27
#define R_RPWM_PIN 26
#define R_LPWM_PIN 25

// --- Піни Драйвера 2 (Лівий) ---
#define L_EN_PIN 14
#define L_RPWM_PIN 32
#define L_LPWM_PIN 33

// Параметри ШІМ
const int pwmFreq = 20000; 
const int pwmRes = 8;      

// --- Змінні для логіки ---
bool lastBtnState = HIGH;      // Попередній стан кнопки
bool isForward = false;        // Напрямок руху (false - назад, true - вперед)
int currentSpeed = 0;          // Поточна швидкість (від 0 до 255)

unsigned long lastRampTime = 0; // Таймер для плавного розгону
const int rampDelay = 15;       // Швидкість розгону: +1 до швидкості кожні 15 мілісекунд
                               // (255 * 15 = ~3.8 секунди до максимальної швидкості)

void setup() {
  pinMode(btnPin, INPUT_PULLUP); // Кнопка підтягнута до плюса
  
  // Налаштовуємо піни активації та вмикаємо драйвери
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH); 
  digitalWrite(L_EN_PIN, HIGH); 

  // Ініціалізуємо ШІМ для моторів (ESP32 Core 3.x.x)
  ledcAttach(R_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(R_LPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_LPWM_PIN, pwmFreq, pwmRes);
}

void loop() {
  // Зчитуємо поточний стан кнопки (LOW = натиснута, HIGH = відпущена)
  bool btnState = digitalRead(btnPin);

  // --- 1. Відстежуємо момент "ЩОЙНО НАТИСНУЛИ" (Зміна напрямку) ---
  if (btnState == LOW && lastBtnState == HIGH) {
    isForward = !isForward;  // Змінюємо напрямок на протилежний
    currentSpeed = 0;        // Скидаємо швидкість до нуля
    delay(50);               // Антибрязкіт контактів (щоб одне натискання не зарахувалось як два)
  }

  // --- 2. Дія під час "УТРИМАННЯ" (Плавний розгін) ---
  if (btnState == LOW) {
    
    // Перевіряємо, чи пройшло достатньо часу для збільшення швидкості
    if (millis() - lastRampTime > rampDelay) {
      if (currentSpeed < 255) {
        currentSpeed++; // Збільшуємо швидкість на 1
      }
      lastRampTime = millis(); // Оновлюємо таймер
    }

    // Подаємо ШІМ на мотори залежно від обраного напрямку
    if (isForward) {
      // Їдемо вперед
      ledcWrite(R_LPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0); // Глушимо реверс
      ledcWrite(R_RPWM_PIN, currentSpeed); ledcWrite(L_RPWM_PIN, currentSpeed);
    } else {
      // Їдемо назад
      ledcWrite(R_RPWM_PIN, 0); ledcWrite(L_RPWM_PIN, 0); // Глушимо хід вперед
      ledcWrite(R_LPWM_PIN, currentSpeed); ledcWrite(L_LPWM_PIN, currentSpeed);
    }
  } 
  
  // --- 3. Дія під час "ВІДПУСКАННЯ" (Миттєва зупинка) ---
  else {
    currentSpeed = 0; // Скидаємо швидкість
    
    // Вимикаємо всі канали ШІМ
    ledcWrite(R_RPWM_PIN, 0); ledcWrite(L_RPWM_PIN, 0);
    ledcWrite(R_LPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);
  }

  // Запам'ятовуємо поточний стан кнопки для наступного циклу
  lastBtnState = btnState;
  
  delay(2); // Маленька затримка для стабільності роботи мікроконтролера
}
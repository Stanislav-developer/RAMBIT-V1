// --- Піни Драйвера 1 (Правий) ---
#define R_EN_PIN 27
#define R_RPWM_PIN 26
#define R_LPWM_PIN 25

// --- Піни Драйвера 2 (Лівий) ---
#define L_EN_PIN 14
#define L_RPWM_PIN 32
#define L_LPWM_PIN 33

// Параметри ШІМ (щоб мотори не пищали)
const int pwmFreq = 20000; 
const int pwmRes = 8;      

void setup() {
  // 1. Налаштовуємо піни активації (EN) як виходи
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);

  // 2. Вмикаємо обидва драйвери (даємо дозвіл на роботу)
  digitalWrite(R_EN_PIN, HIGH); 
  digitalWrite(L_EN_PIN, HIGH); 

  // 3. Налаштовуємо ШІМ для керуючих пінів (ESP32 Core 3.x.x)
  ledcAttach(R_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(R_LPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_LPWM_PIN, pwmFreq, pwmRes);

  // Оскільки ми тестуємо тільки рух ВПЕРЕД, 
  // одразу жорстко глушимо піни руху НАЗАД, щоб не було замикання.
  ledcWrite(R_LPWM_PIN, 0);
  ledcWrite(L_LPWM_PIN, 0);
}

void loop() {
  // --- КРОК 1: Їдемо вперед ---
  // Задаємо швидкість 150 (діапазон від 0 до 255)
  ledcWrite(R_RPWM_PIN, 150);
  ledcWrite(L_RPWM_PIN, 150);
  
  delay(1000); // Чекаємо 1 секунду

  // --- КРОК 2: Зупиняємось ---
  // Скидаємо швидкість до 0
  ledcWrite(R_RPWM_PIN, 0);
  ledcWrite(L_RPWM_PIN, 0);
  
  delay(1000); // Чекаємо 1 секунду
}
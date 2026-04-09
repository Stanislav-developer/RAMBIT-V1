#include <ESP32Servo.h>

Servo myServo;

// Визначаємо піни
const int potPin = 34;   // Аналоговий пін для потенціометра
const int servoPin = 18; // Пін для сервоприводу

// Змінні для зберігання значень
int potValue = 0;
int servoAngle = 0;

void setup() {
  // Ініціалізація Serial монітора для перевірки значень
  Serial.begin(115200);

  // Налаштування сервоприводу (стандартні таймінги 500-2400 мкс)
  myServo.attach(servoPin, 500, 2400);
}

void loop() {
  // 1. Читаємо значення з потенціометра (діапазон 0 - 4095)
  potValue = analogRead(potPin);

  // 2. Масштабуємо значення АЦП (0-4095) у кут для серво (0-180 градусів)
  servoAngle = map(potValue, 0, 4095, 0, 180);

  // 3. Відправляємо команду на сервопривід
  myServo.write(servoAngle);

  // 4. Виводимо дані в консоль для діагностики
  Serial.print("Potentiometer: ");
  Serial.print(potValue);
  Serial.print(" => Servo Angle: ");
  Serial.println(servoAngle);

  // Невелика затримка для стабільності роботи (щоб не перевантажувати шину)
  delay(15);
}
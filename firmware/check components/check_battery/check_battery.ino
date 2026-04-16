// --- Налаштування піна ---
const int voltagePin = 34;

// --- Калібрувальні змінні ---
// Точка 1 (Розряджений акумулятор)
const float calibPinV1 = 1.65;  // Напруга на піні
const float calibBatV1 = 7.40;  // Реальна напруга акумулятора

// Точка 2 (Повністю заряджений акумулятор)
const float calibPinV2 = 2.90;  // Напруга на піні
const float calibBatV2 = 12.60; // Реальна напруга акумулятора

void setup() {
  Serial.begin(115200);
  
  // Для 34 піна (ADC1) це не обов'язково, але є хорошим тоном
  pinMode(voltagePin, INPUT); 
  
  Serial.println("Battery Voltage Monitor Started");
  Serial.println("=================================");
}

void loop() {
  // 1. Зчитуємо сире значення АЦП (від 0 до 4095)
  // Робимо кілька замірів для усереднення і зменшення шуму
  long sumADC = 0;
  for (int i = 0; i < 10; i++) {
    sumADC += analogRead(voltagePin);
    delay(2);
  }
  float avgADC = sumADC / 10.0;

  // 2. Переводимо значення АЦП у напругу на піні (ESP32 ADC reference ~3.3V)
  float pinVoltage = avgADC * (3.3 / 4095.0);

  // 3. Обчислюємо реальну напругу акумулятора (лінійна інтерполяція)
  // Формула: Y = Y1 + (X - X1) * ((Y2 - Y1) / (X2 - X1))
  float batteryVoltage = calibBatV1 + (pinVoltage - calibPinV1) * ((calibBatV2 - calibBatV1) / (calibPinV2 - calibPinV1));

  // Якщо напруга менша за нуль (коли нічого не підключено), показуємо 0
  if (batteryVoltage < 0) {
    batteryVoltage = 0.0;
  }

  // 4. Вивід даних у Serial Monitor
  Serial.print("Raw ADC: ");
  Serial.print(avgADC, 0);
  Serial.print(" \t| Pin V: ");
  Serial.print(pinVoltage, 2);
  Serial.print(" V \t| Battery: ");
  Serial.print(batteryVoltage, 2);
  Serial.println(" V");

  delay(500); // Оновлення двічі на секунду
}
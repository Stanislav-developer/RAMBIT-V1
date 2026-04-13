#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

// Створюємо об'єкти для роботи з приймачем
HardwareSerial crsfSerial(2); // Використовуємо UART2
AlfredoCRSF crsf;

// --- Налаштування пінів ---
#define RX_PIN 16
#define TX_PIN 17

// --- Змінні для таймерів ---
unsigned long lastPrintTime = 0;
unsigned long lastPacketTime = 0; 

void setup() {
  // Запускаємо Serial Monitor для виводу на екран
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Starting CRSF Receiver Test...");

  // Запускаємо зв'язок з приймачем (CRSF працює на швидкості 420000 бод)
  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);
}

void loop() {
  // --- Перевірка наявності зв'язку ---
  if (crsfSerial.available()) {
    lastPacketTime = millis(); // Оновлюємо час останнього пакета
  }
  
  // Оновлюємо дані з приймача
  crsf.update();

  // --- Вивід даних раз на 100 мілісекунд (щоб не зависав Serial) ---
  if (millis() - lastPrintTime > 100) {
    
    // Якщо нових даних не було більше півсекунди - пульт вимкнено
    if (millis() - lastPacketTime > 500) {
      Serial.println("--- NO LINK (Пульт вимкнено або втрачено зв'язок) ---");
    } 
    else {
      // Якщо зв'язок є, зчитуємо перші 6 каналів
      int ch1 = crsf.getChannel(1); // Зазвичай Aileron (Roll / Поворот)
      int ch2 = crsf.getChannel(2); // Зазвичай Elevator (Pitch / Газ або Вперед/Назад)
      int ch3 = crsf.getChannel(3); // Зазвичай Throttle
      int ch4 = crsf.getChannel(4); // Зазвичай Rudder (Yaw)
      int ch5 = crsf.getChannel(5); // Зазвичай ARM (Тумблер безпеки)
      int ch6 = crsf.getChannel(6); // Твій тумблер для фліпера

      // Виводимо все в один рядок
      Serial.print("CH1: "); Serial.print(ch1);
      Serial.print(" | CH2: "); Serial.print(ch2);
      Serial.print(" | CH3: "); Serial.print(ch3);
      Serial.print(" | CH4: "); Serial.print(ch4);
      Serial.print(" | CH5(Arm): "); Serial.print(ch5);
      Serial.print(" | CH6(Flip): "); Serial.println(ch6);
    }
    
    lastPrintTime = millis();
  }
}
#include <AlfredoCRSF.h>
#include <HardwareSerial.h>

// --- Налаштування UART для ELRS ---
#define RX_PIN 16
#define TX_PIN 17

HardwareSerial crsfSerial(2);
AlfredoCRSF crsf;

// --- Розкладка каналів (Radiomaster Pocket) ---
const int CH_AIL    = 1; // Стік: Крен (Вліво/Вправо)
const int CH_ELE    = 2; // Стік: Тангаж (Вперед/Назад)
const int CH_THR    = 3; // Стік: Газ (Вгору/Вниз)
const int CH_RUD    = 4; // Стік: Рискання (Поворот навколо осі)
const int CH_ARM    = 5; // SE: Запобіжник (ARM)
const int CH_L_FLIP = 6; // SB: Лівий фліпер
const int CH_R_FLIP = 7; // SC: Правий фліпер
const int CH_MODE   = 8; // SD: Зміна режимів
const int CH_SPEED  = 9; // SF: Енкодер швидкості

unsigned long lastPrintTime = 0;

void setup() {
  // Запускаємо Serial Monitor для виводу на екран
  Serial.begin(115200);
  
  // Запускаємо зв'язок з приймачем ELRS
  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);

  Serial.println("=========================================");
  Serial.println("  Діагностика ELRS Приймача запущена!");
  Serial.println("  Очікування пакетів даних...");
  Serial.println("=========================================");
  delay(1000);
}

void loop() {
  // Оновлюємо дані з приймача (обов'язково викликати в кожному циклі)
  crsf.update();

  // Виводимо дані кожні 200 мілісекунд (щоб не засмічувати екран)
  if (millis() - lastPrintTime > 200) {
    
    // Зчитуємо всі 9 каналів
    int ail   = crsf.getChannel(CH_AIL);
    int ele   = crsf.getChannel(CH_ELE);
    int thr   = crsf.getChannel(CH_THR);
    int rud   = crsf.getChannel(CH_RUD);
    int arm   = crsf.getChannel(CH_ARM);
    int lFlip = crsf.getChannel(CH_L_FLIP);
    int rFlip = crsf.getChannel(CH_R_FLIP);
    int mode  = crsf.getChannel(CH_MODE);
    int speed = crsf.getChannel(CH_SPEED);

    // Проста перевірка наявності зв'язку 
    // (якщо пульт вимкнений, AlfredoCRSF зазвичай повертає 0 або старі значення, 
    // але якщо зв'язок є, канали стіків завжди > 800)
    if (ail > 800) {
      Serial.println("\n--- СТІКИ (AETR) ---");
      Serial.printf("CH1 (AIL) : %4d  |  CH2 (ELE) : %4d\n", ail, ele);
      Serial.printf("CH3 (THR) : %4d  |  CH4 (RUD) : %4d\n", thr, rud);
      
      Serial.println("--- ТУМБЛЕРИ ТА ЕНКОДЕР ---");
      Serial.printf("CH5 (ARM) : %4d  |  CH8 (MOD) : %4d\n", arm, mode);
      Serial.printf("CH6 (L_FL): %4d  |  CH7 (R_FL): %4d\n", lFlip, rFlip);
      Serial.printf("CH9 (ENC) : %4d\n", speed);
      Serial.println("-----------------------------------------");
    } else {
      Serial.println("Зв'язок відсутній. Увімкніть пульт...");
    }

    lastPrintTime = millis();
  }
}
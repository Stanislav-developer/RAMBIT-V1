#include <AlfredoCRSF.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <DNSServer.h> 

HardwareSerial crsfSerial(2);
AlfredoCRSF crsf;
WebServer server(80);
Preferences preferences;
DNSServer dnsServer; 

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
#define CONFIG_BUTTON_PIN 4 

// --- Налаштування батареї ---
const int VOLTAGE_PIN = 34;
const float calibPinV1 = 1.65;  
const float calibBatV1 = 7.40;  
const float calibPinV2 = 2.90;  
const float calibBatV2 = 12.60; 

unsigned long lastBatteryCheck = 0;
float currentBatteryVoltage = 0.0;

// Параметри ШІМ для моторів
const int pwmFreq = 20000;
const int pwmRes = 8;      

unsigned long lastPrint = 0; 

// =======================================================
// ⚙️ ГОЛОВНІ НАЛАШТУВАННЯ КЕРУВАННЯ
// =======================================================
float lowBatteryThreshold = 11.0; 

int CH_AIL = 1;      
int CH_ELE = 2;      
int CH_THR = 3;      
int CH_RUD = 4;      
int CH_ARM = 5;      
int CH_L_FLIP = 6;   
int CH_R_FLIP = 7;   
int CH_DRIVE_MODE = 8; 
int CH_SPEED = 9;    
int CH_SWAP = 10;    

bool invertLeftMotor = true;  
bool invertRightMotor = false;  
bool invertSteering = true; 

bool swapFlipperSides = true; 
bool invertServoR = false; 
bool invertServoL = true;  

bool isElrsConnected = false;
bool isRobotLocked = true;    
int lastEncoderVal = 1000;    

bool isSwappedMode = false;   
bool lastSwapButtonState = false; 
bool isAdaptiveMode = false;      

// =======================================================
// HTML ВЕБ-СЕРВЕРА
// =======================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RAMBIT-V1 Config</title>
  <style>
    body { background-color: #121212; color: #E0E0E0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; }
    h1 { color: #FFD700; text-align: center; border-bottom: 2px solid #8A2BE2; padding-bottom: 10px; }
    h3 { color: #8A2BE2; }
    .card { background-color: #1E1E1E; padding: 15px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); border-left: 4px solid #8A2BE2; }
    .telemetry { display: flex; justify-content: space-around; font-size: 1.2em; font-weight: bold; }
    .tel-val { color: #FFD700; }
    .ok { color: #00FF00; } .err { color: #FF0000; }
    .form-group { margin-bottom: 15px; display: flex; justify-content: space-between; align-items: center; }
    input[type="number"], input[type="text"] { background-color: #2D2D2D; color: #FFF; border: 1px solid #8A2BE2; padding: 8px; border-radius: 4px; width: 80px; }
    input[type="checkbox"] { transform: scale(1.5); accent-color: #8A2BE2; }
    button { background-color: #8A2BE2; color: white; border: none; padding: 12px 20px; font-size: 16px; border-radius: 5px; cursor: pointer; width: 48%; font-weight: bold; }
    button:hover { background-color: #6a0dad; }
    .btn-danger { background-color: #555; }
    .btn-danger:hover { background-color: #333; }
    .btn-container { display: flex; justify-content: space-between; margin-top: 20px; }
    .ota-section { border-left: 4px solid #FFD700; }
    input[type="file"] { color: #FFF; }
    footer { text-align: center; margin-top: 30px; color: #777; }
    a { color: #FFD700; text-decoration: none; }
  </style>
</head>
<body>
  <h1>RAMBIT-V1<br><span style="font-size: 0.5em; color: #8A2BE2;">by Cyber Constructing Club</span></h1>

  <div class="card">
    <h3>📡 Телеметрія (Оновлюється автоматично)</h3>
    <div class="telemetry">
      <div>Батарея: <span id="batVal" class="tel-val">-- V</span></div>
      <div>ELRS Сигнал: <span id="elrsVal" class="tel-val">--</span></div>
    </div>
  </div>

  <form id="configForm" action="/save" method="POST">
    <div class="card">
      <h3>⚙️ Головні налаштування</h3>
      <div class="form-group">
        <label>Поріг розряду АКБ (Вольти):</label>
        <input type="number" step="0.1" min="5.0" max="15.0" name="thr" id="thr" required>
      </div>
    </div>

    <div class="card">
      <h3>🎮 Налаштування каналів (1-14)</h3>
      <div class="form-group"><label>CH_AIL (Крен):</label><input type="number" min="1" max="14" name="c_ail" id="c_ail" required></div>
      <div class="form-group"><label>CH_ELE (Тангаж):</label><input type="number" min="1" max="14" name="c_ele" id="c_ele" required></div>
      <div class="form-group"><label>CH_THR (Газ):</label><input type="number" min="1" max="14" name="c_thr" id="c_thr" required></div>
      <div class="form-group"><label>CH_RUD (Рискання):</label><input type="number" min="1" max="14" name="c_rud" id="c_rud" required></div>
      <div class="form-group"><label>CH_ARM (Запобіжник):</label><input type="number" min="1" max="14" name="c_arm" id="c_arm" required></div>
      <div class="form-group"><label>CH_L_FLIP (Лівий фліпер):</label><input type="number" min="1" max="14" name="c_lfl" id="c_lfl" required></div>
      <div class="form-group"><label>CH_R_FLIP (Правий фліпер):</label><input type="number" min="1" max="14" name="c_rfl" id="c_rfl" required></div>
      <div class="form-group"><label>CH_DRIVE_MODE (Танк/Адаптив):</label><input type="number" min="1" max="14" name="c_drv" id="c_drv" required></div>
      <div class="form-group"><label>CH_SPEED (Енкодер):</label><input type="number" min="1" max="14" name="c_spd" id="c_spd" required></div>
      <div class="form-group"><label>CH_SWAP (Зміна стіків):</label><input type="number" min="1" max="14" name="c_swp" id="c_swp" required></div>
    </div>

    <div class="card">
      <h3>🔄 Інверсія</h3>
      <div class="form-group"><label>Інверсія Лівого Мотора:</label><input type="checkbox" name="i_lm" id="i_lm" value="1"></div>
      <div class="form-group"><label>Інверсія Правого Мотора:</label><input type="checkbox" name="i_rm" id="i_rm" value="1"></div>
      <div class="form-group"><label>Інверсія Керма (Steering):</label><input type="checkbox" name="i_st" id="i_st" value="1"></div>
    </div>

    <div class="btn-container">
      <button type="button" class="btn-danger" onclick="window.location.href='/exit'">🚪 Вийти (Без збереження)</button>
      <button type="submit">💾 Зберегти та Запустити</button>
    </div>
  </form>

  <div class="card ota-section" style="margin-top: 30px;">
    <h3>🚀 Оновлення прошивки (OTA)</h3>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required>
      <button type="submit" style="width: 100%; margin-top: 15px;">⬆️ Завантажити прошивку</button>
    </form>
  </div>

  <footer>
    <a href="https://github.com/" target="_blank">💻 GitHub Project Repository</a>
  </footer>

  <script>
    function fetchTelemetry() {
      fetch('/data').then(r => r.json()).then(data => {
        document.getElementById('batVal').innerText = data.bat + " V";
        document.getElementById('batVal').className = data.bat < data.cfg.thr ? "tel-val err" : "tel-val ok";
        
        let elrsElem = document.getElementById('elrsVal');
        elrsElem.innerText = data.elrs ? "ПІДКЛЮЧЕНО" : "НЕМАЄ СИГНАЛУ";
        elrsElem.className = data.elrs ? "tel-val ok" : "tel-val err";
        
        if(!window.loadedData) {
          document.getElementById('thr').value = data.cfg.thr;
          document.getElementById('c_ail').value = data.cfg.c_ail;
          document.getElementById('c_ele').value = data.cfg.c_ele;
          document.getElementById('c_thr').value = data.cfg.c_thr;
          document.getElementById('c_rud').value = data.cfg.c_rud;
          document.getElementById('c_arm').value = data.cfg.c_arm;
          document.getElementById('c_lfl').value = data.cfg.c_lfl;
          document.getElementById('c_rfl').value = data.cfg.c_rfl;
          document.getElementById('c_drv').value = data.cfg.c_drv;
          document.getElementById('c_spd').value = data.cfg.c_spd;
          document.getElementById('c_swp').value = data.cfg.c_swp;
          document.getElementById('i_lm').checked = data.cfg.i_lm;
          document.getElementById('i_rm').checked = data.cfg.i_rm;
          document.getElementById('i_st').checked = data.cfg.i_st;
          window.loadedData = true;
        }
      });
    }
    setInterval(fetchTelemetry, 3000);
    fetchTelemetry();
  </script>
</body>
</html>
)rawliteral";

// =======================================================
// ДОПОМІЖНІ ФУНКЦІЇ ДЛЯ КОНФІГУРАЦІЇ
// =======================================================
float getBatteryVoltage() {
  long sumADC = 0;
  for (int i = 0; i < 5; i++) sumADC += analogRead(VOLTAGE_PIN);
  float avgADC = sumADC / 5.0;
  float pinVoltage = avgADC * (3.3 / 4095.0);
  float bat = calibBatV1 + (pinVoltage - calibPinV1) * ((calibBatV2 - calibBatV1) / (calibPinV2 - calibPinV1));
  return (bat < 0) ? 0.0 : bat;
}

void loadPreferences() {
  preferences.begin("rambit", true); 
  lowBatteryThreshold = preferences.getFloat("thr", 8.0);
  CH_AIL = preferences.getInt("c_ail", 1);
  CH_ELE = preferences.getInt("c_ele", 2);
  CH_THR = preferences.getInt("c_thr", 3);
  CH_RUD = preferences.getInt("c_rud", 4);
  CH_ARM = preferences.getInt("c_arm", 5);
  CH_L_FLIP = preferences.getInt("c_lfl", 6);
  CH_R_FLIP = preferences.getInt("c_rfl", 7);
  CH_DRIVE_MODE = preferences.getInt("c_drv", 8);
  CH_SPEED = preferences.getInt("c_spd", 9);
  CH_SWAP = preferences.getInt("c_swp", 10);
  
  invertLeftMotor = preferences.getBool("i_lm", true);
  invertRightMotor = preferences.getBool("i_rm", false);
  invertSteering = preferences.getBool("i_st", true);
  preferences.end();
}

void savePreferences() {
  preferences.begin("rambit", false); 
  preferences.putFloat("thr", server.arg("thr").toFloat());
  preferences.putInt("c_ail", server.arg("c_ail").toInt());
  preferences.putInt("c_ele", server.arg("c_ele").toInt());
  preferences.putInt("c_thr", server.arg("c_thr").toInt());
  preferences.putInt("c_rud", server.arg("c_rud").toInt());
  preferences.putInt("c_arm", server.arg("c_arm").toInt());
  preferences.putInt("c_lfl", server.arg("c_lfl").toInt());
  preferences.putInt("c_rfl", server.arg("c_rfl").toInt());
  preferences.putInt("c_drv", server.arg("c_drv").toInt());
  preferences.putInt("c_spd", server.arg("c_spd").toInt());
  preferences.putInt("c_swp", server.arg("c_swp").toInt());
  
  preferences.putBool("i_lm", server.hasArg("i_lm"));
  preferences.putBool("i_rm", server.hasArg("i_rm"));
  preferences.putBool("i_st", server.hasArg("i_st"));
  preferences.end();
}

void playWebEnterSound() {
  tone(BUZZER_PIN, 1000, 150); delay(150);
  tone(BUZZER_PIN, 2000, 150); delay(150);
  tone(BUZZER_PIN, 3000, 300);
}

void playWebExitSound() {
  tone(BUZZER_PIN, 3000, 150); delay(150);
  tone(BUZZER_PIN, 2000, 150); delay(150);
  tone(BUZZER_PIN, 1000, 300);
}

// --- РЕЖИМ ПІТ-СТОПУ (WEB SERVER + CAPTIVE PORTAL) ---
void runConfigMode() {
  Serial.println(">>> УВІМКНЕНО РЕЖИМ ПІТ-СТОПУ <<<");
  playWebEnterSound();
  
  WiFi.softAP("RAMBIT_CONFIG", "12345678");
  Serial.print("IP Адреса: "); Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP());

  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);

  server.on("/", HTTP_GET, []() { 
    server.send(200, "text/html; charset=utf-8", INDEX_HTML); 
  });

  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"bat\":" + String(getBatteryVoltage(), 2) + ",";
    json += "\"elrs\":" + String(crsf.isLinkUp() ? "true" : "false") + ",";
    json += "\"cfg\":{";
    json += "\"thr\":" + String(lowBatteryThreshold) + ",";
    json += "\"c_ail\":" + String(CH_AIL) + ",\"c_ele\":" + String(CH_ELE) + ",";
    json += "\"c_thr\":" + String(CH_THR) + ",\"c_rud\":" + String(CH_RUD) + ",";
    json += "\"c_arm\":" + String(CH_ARM) + ",\"c_lfl\":" + String(CH_L_FLIP) + ",";
    json += "\"c_rfl\":" + String(CH_R_FLIP) + ",\"c_drv\":" + String(CH_DRIVE_MODE) + ",";
    json += "\"c_spd\":" + String(CH_SPEED) + ",\"c_swp\":" + String(CH_SWAP) + ",";
    json += "\"i_lm\":" + String(invertLeftMotor ? "true" : "false") + ",";
    json += "\"i_rm\":" + String(invertRightMotor ? "true" : "false") + ",";
    json += "\"i_st\":" + String(invertSteering ? "true" : "false");
    json += "}}";
    server.send(200, "application/json; charset=utf-8", json);
  });

  server.on("/save", HTTP_POST, []() {
    savePreferences();
    server.send(200, "text/html; charset=utf-8", "<h2 style='color:white; font-family:sans-serif;'>Збережено! Перезавантаження...</h2>");
    playWebExitSound();
    delay(1000);
    ESP.restart();
  });

  server.on("/exit", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", "<h2 style='color:white; font-family:sans-serif;'>Вихід... Перезавантаження...</h2>");
    playWebExitSound();
    delay(1000);
    ESP.restart();
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain; charset=utf-8", (Update.hasError()) ? "ПОМИЛКА ОНОВЛЕННЯ" : "ПРОШИВКУ ЗАВАНТАЖЕНО! ПЕРЕЗАВАНТАЖЕННЯ...");
    playWebExitSound();
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA Start: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
      else Update.printError(Serial);
    }
  });

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  while (true) {
    dnsServer.processNextRequest(); 
    server.handleClient();
    crsf.update();
    delay(2);
  }
}

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

// =======================================================
// SETUP (СИСТЕМНИЙ СТАРТ)
// =======================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP); 
  pinMode(VOLTAGE_PIN, INPUT);
  
  digitalWrite(LED_PIN, LOW); 

  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, LOW); 
  digitalWrite(L_EN_PIN, LOW); 

  Serial.begin(115200);
  delay(500);

  loadPreferences();

  if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    runConfigMode(); 
  }

  crsfSerial.begin(420000, SERIAL_8N1, RX_PIN, TX_PIN);
  crsf.begin(crsfSerial);

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

  ledcAttach(R_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(R_LPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_RPWM_PIN, pwmFreq, pwmRes);
  ledcAttach(L_LPWM_PIN, pwmFreq, pwmRes);

  ledcWrite(R_RPWM_PIN, 0); ledcWrite(R_LPWM_PIN, 0);
  ledcWrite(L_RPWM_PIN, 0); ledcWrite(L_LPWM_PIN, 0);

  tone(BUZZER_PIN, 800, 200); delay(250);
  tone(BUZZER_PIN, 1200, 200); delay(250);
  tone(BUZZER_PIN, 600, 400);  delay(400);
}

// =======================================================
// LOOP (БОЙОВИЙ ЦИКЛ)
// =======================================================
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

  // ⚠️ НОВЕ: ПЕРЕВІРКА БЕЗПЕКИ ПЕРЕД АРМОМ (PRE-ARM CHECK)
  static bool lastRequestArm = false;
  bool requestArm = (armVal > 1500);

  if (requestArm && !lastRequestArm) { 
    // Спроба заармити (перехід тумблера вгору)
    if (isRobotLocked) {
      // Умови безпеки (все в мінімумі / вимкнено)
      bool isThrottleSafe  = (rawThr < 1050); 
      bool isEncoderSafe   = (speedEncoderVal < 1050);
      bool areSwitchesSafe = (leftFlipVal < 1300 && rightFlipVal < 1300 && driveModeVal < 1500 && swapModeVal < 1500);

      if (isThrottleSafe && isEncoderSafe && areSwitchesSafe) {
        isRobotLocked = false;
        // Звук успішного розблокування
        tone(BUZZER_PIN, 1500, 100); delay(150); 
        tone(BUZZER_PIN, 2000, 100); delay(150); 
        tone(BUZZER_PIN, 2500, 250);
      } else {
        // Звук ПОМИЛКИ (Pre-arm failed)
        tone(BUZZER_PIN, 300, 400); 
        Serial.println("PRE-ARM ПОМИЛКА: Опустіть всі тумблери та стік газу в нуль!");
      }
    }
  } 
  else if (!requestArm && lastRequestArm) { 
    // Дизарм (перехід тумблера вниз)
    if (!isRobotLocked) {
      isRobotLocked = true;
      tone(BUZZER_PIN, 800, 150); delay(200); 
      tone(BUZZER_PIN, 400, 250);
    }
  }
  
  // Якщо зв'язок зник, треба скинути тригер арму, щоб після відновлення зв'язку 
  // робот вимагав "пересмикнути" тумблер ARM
  if (!isCurrentlyConnected) {
    lastRequestArm = false; 
  } else {
    lastRequestArm = requestArm;
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

  if (abs(speedEncoderVal - lastEncoderVal) > 15) {
    lastEncoderVal = speedEncoderVal;
    tone(BUZZER_PIN, 4000, 15); 
  }
  
  int maxSpeedLimit = map(speedEncoderVal, 1000, 2000, 0, 255);
  maxSpeedLimit = constrain(maxSpeedLimit, 0, 255);

  bool currentSwapButtonState = (swapModeVal > 1500);
  if (currentSwapButtonState && !lastSwapButtonState) {
    isSwappedMode = !isSwappedMode; 
    if (isSwappedMode) tone(BUZZER_PIN, 3000, 100); 
    else tone(BUZZER_PIN, 2000, 100); 
  }
  lastSwapButtonState = currentSwapButtonState; 

  bool currentDriveMode = (driveModeVal > 1500);
  if (currentDriveMode != isAdaptiveMode) {
    isAdaptiveMode = currentDriveMode;
    if (isAdaptiveMode) {
      tone(BUZZER_PIN, 3500, 150); 
    } else {
      tone(BUZZER_PIN, 2500, 150); 
    }
  }

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
    if (throttle == 0) {
      leftMotorSpeed = steer;
      rightMotorSpeed = -steer;
    } else {
      float turnFactor = abs(steer) / 255.0; 
      int innerSpeed = throttle * (1.0 - (turnFactor * 0.75)); 

      if (steer > 0) { 
        leftMotorSpeed = throttle;       
        rightMotorSpeed = innerSpeed;    
      } else {         
        rightMotorSpeed = throttle;      
        leftMotorSpeed = innerSpeed;     
      }
    }
  } else {
    leftMotorSpeed = throttle + steer;
    rightMotorSpeed = throttle - steer;
  }

  if (invertLeftMotor) leftMotorSpeed = -leftMotorSpeed;
  if (invertRightMotor) rightMotorSpeed = -rightMotorSpeed;

  leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
  rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);

  leftMotorSpeed = (leftMotorSpeed * maxSpeedLimit) / 255;
  rightMotorSpeed = (rightMotorSpeed * maxSpeedLimit) / 255;

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

  if (millis() - lastBatteryCheck >= 5000) {
    lastBatteryCheck = millis();
    currentBatteryVoltage = getBatteryVoltage();

    if (currentBatteryVoltage > 5.0 && currentBatteryVoltage < lowBatteryThreshold) {
      tone(BUZZER_PIN, 4000, 300); 
    }
  }

  if (millis() - lastPrint > 200) {
    Serial.print("Bat: "); Serial.print(currentBatteryVoltage, 1); Serial.print("V | ");
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


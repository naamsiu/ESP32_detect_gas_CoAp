#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// --- CẤU HÌNH MẠNG ---
const char* ssid = "VAN TRIEU";
const char* password = "conkhongbiet";
// Thiết bị tự nhận IP qua DHCP

// --- ĐỊNH NGHĨA CHÂN PHẦN CỨNG ---
#define MQ2_PIN 34
#define MOTOR_PWM_PIN 26
#define MOTOR_DIR_PIN 27
#define BUZZER_PIN 25

// --- FSM (Finite State Machine) ---
enum SystemState { SAFE, GAS_DETECTED, ALARM, FAN_RUNNING };
SystemState currentState = SAFE;

// --- BIẾN TOÀN CỤC ---
int gasValue = 0;
const int GAS_THRESHOLD = 600;
const int HYSTERESIS = 200;

bool isAutoMode = true; // AUTO hoặc MANUAL

// Biến quản lý Quạt cho chế độ MANUAL
int targetFanSpeed = 150; // Nhớ tốc độ cài đặt (mặc định 150)
bool isFanOn = false;     // Nhớ trạng thái Công tắc quạt

// Biến quản lý thời gian chống nhiễu vật lý (AUTO)
unsigned long fanStartTime = 0; 
const unsigned long MIN_RUN_TIME = 10000; // Quạt phải chạy tối thiểu 10 giây (10000ms)

// --- LỌC NHIỄU SENSOR (Moving Average) ---
const int numReadings = 50; // Tăng lên 50 để lấy mẫu trong 0.5s giúp đồ thị mượt hơn
int readings[numReadings];
int readIndex = 0;
long total = 0;

// --- GIAO THỨC CoAP ---
WiFiUDP udp;
Coap coap(udp);

void callback_state(CoapPacket &packet, IPAddress ip, int port);
void callback_command(CoapPacket &packet, IPAddress ip, int port);

void setupWiFi() {
  WiFi.begin(ssid, password); 
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Động (DHCP): " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(MOTOR_DIR_PIN, OUTPUT);
  digitalWrite(MOTOR_DIR_PIN, LOW);
  
  ledcAttach(MOTOR_PWM_PIN, 5000, 8);
  ledcWrite(MOTOR_PWM_PIN, 0);        

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < numReadings; i++) readings[i] = 0;

  setupWiFi();

  coap.server(callback_state, "state");      
  coap.server(callback_command, "command");  
  coap.start();
}

int getFilteredGas() {
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(MQ2_PIN);
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  return total / numReadings;
}

void handleFSM() {
  if (!isAutoMode) return;

  switch (currentState) {
    case SAFE:
      digitalWrite(BUZZER_PIN, LOW);
      ledcWrite(MOTOR_PWM_PIN, 0);
      if (gasValue > GAS_THRESHOLD) {
        currentState = GAS_DETECTED;
      }
      break;

    case GAS_DETECTED:
      currentState = ALARM;
      break;

    case ALARM:
      digitalWrite(BUZZER_PIN, HIGH);
      currentState = FAN_RUNNING;
      
      // Bắt đầu bấm giờ và Kick-start quạt (Mồi dòng để chống kẹt motor)
      fanStartTime = millis(); 
      ledcWrite(MOTOR_PWM_PIN, 255); // Chạy 100% công suất trong 1 giây đầu
      break;

    case FAN_RUNNING:
      // Sau 1 giây tạo đà, giảm tốc độ về mức ổn định để đỡ ồn/nóng
      if (millis() - fanStartTime > 1000) {
        ledcWrite(MOTOR_PWM_PIN, 150); 
      }

      // Điều kiện thoát: Khí gas an toàn VÀ quạt đã chạy đủ thời gian tối thiểu 10s
      if ((gasValue < (GAS_THRESHOLD - HYSTERESIS)) && (millis() - fanStartTime > MIN_RUN_TIME)) {
        currentState = SAFE;
      }
      break;
  }
}

void loop() {
  gasValue = getFilteredGas();
  handleFSM();                
  coap.loop();                
  delay(10);                  
}

// --- XỬ LÝ REQUEST TỪ NODE.JS ---
void callback_state(CoapPacket &packet, IPAddress ip, int port) {
  char payload[150];
  sprintf(payload, "{\"gas\":%d, \"state\":%d, \"mode\":\"%s\"}",
          gasValue, currentState, isAutoMode ? "AUTO" : "MANUAL");
  coap.sendResponse(ip, port, packet.messageid, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
}

void callback_command(CoapPacket &packet, IPAddress ip, int port) {
  String msg = "";
  for(int i = 0; i < packet.payloadlen; i++) {
    msg += (char)packet.payload[i];
  }
  
  if (msg == "AUTO") {
    isAutoMode = true;
  }
  else if (msg == "MANUAL") {
    isAutoMode = false;
    digitalWrite(BUZZER_PIN, LOW);
    isFanOn = false;
    ledcWrite(MOTOR_PWM_PIN, 0);  
  }
  else if (msg == "FAN_ON" && !isAutoMode) {
    isFanOn = true;
    ledcWrite(MOTOR_PWM_PIN, targetFanSpeed);
  }
  else if (msg == "FAN_OFF" && !isAutoMode) {
    isFanOn = false;
    ledcWrite(MOTOR_PWM_PIN, 0);
  }
  else if (msg.startsWith("FAN_SPD:") && !isAutoMode) {
    targetFanSpeed = msg.substring(8).toInt();
    if (isFanOn) {
      ledcWrite(MOTOR_PWM_PIN, targetFanSpeed);
    }
  }
  else if (msg.startsWith("BUZ:") && !isAutoMode) {
    int state = msg.substring(4).toInt();
    digitalWrite(BUZZER_PIN, state > 0 ? HIGH : LOW);
  }
  
  coap.sendResponse(ip, port, packet.messageid, "OK");
}
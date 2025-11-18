#include <WiFi.h>
#include <PubSubClient.h>

// ==================== CONFIGURAÇÕES UBIDOTS ====================
const char* WIFI_SSID = "iPhone";
const char* WIFI_PASSWORD = "87654321";
const char* UBIDOTS_TOKEN = "BBUS-RGM8NEtn2FOwN3p4QCfJg4UqSF3ace";
const char* DEVICE_LABEL = "esp32_t17_anny";
const char* MQTT_BROKER = "industrial.api.ubidots.com";
const int MQTT_PORT = 1883;

// Tópicos MQTT
char PUBLISH_TOPIC[150];
char SUBSCRIBE_TOPIC[150];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ==================== PINOS E ESTADOS ====================
enum TrafficLightState { TL_RED, TL_YELLOW, TL_GREEN };

// Semáforo 1
const int LED_RED_1    = 4;
const int LED_YELLOW_1 = 16;
const int LED_GREEN_1  = 17;

// Semáforo 2
const int LED_RED_2    = 33;
const int LED_YELLOW_2 = 25;
const int LED_GREEN_2  = 26;

// Sensores
const int LDR_PIN      = 34;
const int TRIG_PIN     = 5;
const int ECHO_PIN     = 18;

// Funções auxiliares
void ledOn(int pin)  { digitalWrite(pin, LOW); }
void ledOff(int pin) { digitalWrite(pin, HIGH); }

// Estados
TrafficLightState state1 = TL_RED;
TrafficLightState state2 = TL_GREEN;
bool nightMode = false;

// Variáveis LDR
int ldrValue = 0;
int LDR_THRESHOLD = 100;

// Tempos
long timeRed = 4000;
long timeYellow = 3000;
long timeGreen = 6000;
long timeNightMode = 500;
unsigned long lastStateChange = 0;
unsigned long lastPublish = 0;
const long PUBLISH_INTERVAL = 2000; // Publica a cada 2 segundos

// ==================== FUNÇÕES AUXILIARES ====================

int filteredLdr(int samples = 20) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(LDR_PIN);
    delay(2);
  }
  return sum / samples;
}

long readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  return distance;
}

void setTrafficLight(int redPin, int yellowPin, int greenPin, TrafficLightState state) {
  ledOff(redPin);
  ledOff(yellowPin);
  ledOff(greenPin);
  
  switch (state) {
    case TL_RED:    ledOn(redPin);    break;
    case TL_YELLOW: ledOn(yellowPin); break;
    case TL_GREEN:  ledOn(greenPin);  break;
  }
}

// ==================== WIFI E MQTT ====================

void connectWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);
  
  // Converte payload para string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Payload: ");
  Serial.println(message);
  
  // Parse JSON simples (formato: {"variable":value})
  // Exemplo de controle via Ubidots
  if (message.indexOf("\"night_mode\"") > 0) {
    if (message.indexOf("1.0") > 0 || message.indexOf("1}") > 0) {
      nightMode = true;
      Serial.println("Modo noturno ATIVADO via Ubidots");
    } else {
      nightMode = false;
      Serial.println("Modo noturno DESATIVADO via Ubidots");
    }
  }
  
  if (message.indexOf("\"time_red\"") > 0) {
    int value = extractValue(message);
    if (value > 0) {
      timeRed = value * 1000;
      Serial.print("Tempo vermelho atualizado: ");
      Serial.println(timeRed);
    }
  }
  
  if (message.indexOf("\"time_yellow\"") > 0) {
    int value = extractValue(message);
    if (value > 0) {
      timeYellow = value * 1000;
      Serial.print("Tempo amarelo atualizado: ");
      Serial.println(timeYellow);
    }
  }
  
  if (message.indexOf("\"time_green\"") > 0) {
    int value = extractValue(message);
    if (value > 0) {
      timeGreen = value * 1000;
      Serial.print("Tempo verde atualizado: ");
      Serial.println(timeGreen);
    }
  }
  
  if (message.indexOf("\"threshold\"") > 0) {
    int value = extractValue(message);
    if (value > 0) {
      LDR_THRESHOLD = value;
      Serial.print("Threshold atualizado: ");
      Serial.println(LDR_THRESHOLD);
    }
  }
  
  if (message.indexOf("\"calibrate\"") > 0) {
    calibrateLdr();
  }
}

int extractValue(String message) {
  int colonPos = message.indexOf(":");
  if (colonPos < 0) return -1;
  
  int endPos = message.indexOf("}", colonPos);
  if (endPos < 0) endPos = message.length();
  
  String valueStr = message.substring(colonPos + 1, endPos);
  valueStr.trim();
  valueStr.replace("\"", "");
  
  return valueStr.toInt();
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando ao MQTT...");
    
    if (mqttClient.connect(DEVICE_LABEL, UBIDOTS_TOKEN, "")) {
      Serial.println(" conectado!");
      mqttClient.subscribe(SUBSCRIBE_TOPIC);
      Serial.print("Inscrito no tópico: ");
      Serial.println(SUBSCRIBE_TOPIC);
    } else {
      Serial.print(" falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

void publishToUbidots() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  
  // Lê sensores
  ldrValue = filteredLdr(20);
  long distance = readUltrasonic();
  
  // Cria payload JSON
  char payload[512];
  sprintf(payload, 
    "{\"ldr\":%d,\"distance\":%ld,\"night_mode\":%d,\"state1\":%d,\"state2\":%d,\"time_red\":%ld,\"time_yellow\":%ld,\"time_green\":%ld,\"threshold\":%d}",
    ldrValue,
    distance,
    nightMode ? 1 : 0,
    state1,
    state2,
    timeRed / 1000,
    timeYellow / 1000,
    timeGreen / 1000,
    LDR_THRESHOLD
  );
  
  // Publica
  if (mqttClient.publish(PUBLISH_TOPIC, payload)) {
    Serial.println("Dados publicados no Ubidots:");
    Serial.println(payload);
  } else {
    Serial.println("Falha ao publicar");
  }
}

void calibrateLdr() {
  Serial.println("Calibrando LDR...");
  
  long sum = 0;
  const int samples = 100;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(LDR_PIN);
    delay(5);
  }
  
  int base = sum / samples;
  int margin = base * 0.35;
  LDR_THRESHOLD = base - margin;
  
  if (LDR_THRESHOLD < 20) LDR_THRESHOLD = 20;
  
  Serial.print("Base: ");
  Serial.print(base);
  Serial.print(" | Threshold: ");
  Serial.println(LDR_THRESHOLD);
}

// ==================== LÓGICA DOS SEMÁFOROS ====================

void updateNightMode() {
  ldrValue = filteredLdr(30);
  bool isDark = (ldrValue < LDR_THRESHOLD);
  
  if (isDark && !nightMode) {
    nightMode = true;
    lastStateChange = millis();
    Serial.println(">>> MODO NOTURNO ATIVADO");
  }
  
  if (!isDark && nightMode) {
    nightMode = false;
    lastStateChange = millis();
    Serial.println(">>> MODO NOTURNO DESATIVADO");
    
    state1 = TL_RED;
    state2 = TL_GREEN;
    setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, state1);
    setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, state2);
  }
}

void forceCarPriority() {
  long distance = readUltrasonic();
  
  if (distance > 0 && distance < 7) {
    Serial.print("Carro detectado a ");
    Serial.print(distance);
    Serial.println(" cm — PRIORIDADE!");
    
    state1 = TL_GREEN;
    state2 = TL_RED;
    
    setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, TL_GREEN);
    setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, TL_RED);
    
    lastStateChange = millis();
  }
}

void updateTrafficLights() {
  unsigned long currentTime = millis();
  
  if (nightMode) {
    static bool yellowBlink = false;
    
    if (currentTime - lastStateChange >= timeNightMode) {
      lastStateChange = currentTime;
      yellowBlink = !yellowBlink;
      
      if (yellowBlink) {
        ledOn(LED_YELLOW_1);
        ledOn(LED_YELLOW_2);
        ledOff(LED_RED_1); ledOff(LED_GREEN_1);
        ledOff(LED_RED_2); ledOff(LED_GREEN_2);
      } else {
        ledOff(LED_YELLOW_1);
        ledOff(LED_YELLOW_2);
      }
    }
    return;
  }
  
  forceCarPriority();
  
  long stateDuration = 0;
  switch (state1) {
    case TL_RED:    stateDuration = timeRed;    break;
    case TL_YELLOW: stateDuration = timeYellow; break;
    case TL_GREEN:  stateDuration = timeGreen;  break;
  }
  
  if (currentTime - lastStateChange >= stateDuration) {
    lastStateChange = currentTime;
    
    if (state1 == TL_GREEN) {
      state1 = TL_YELLOW;
      state2 = TL_RED;
    } else if (state1 == TL_YELLOW) {
      state1 = TL_RED;
      state2 = TL_GREEN;
    } else if (state1 == TL_RED && state2 == TL_GREEN) {
      state2 = TL_YELLOW;
    } else if (state1 == TL_RED && state2 == TL_YELLOW) {
      state1 = TL_GREEN;
      state2 = TL_RED;
    }
    
    setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, state1);
    setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, state2);
  }
}

// ==================== SETUP E LOOP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Sistema de Semáforos com Ubidots ===");
  
  // Configura pinos
  pinMode(LED_RED_1, OUTPUT);
  pinMode(LED_YELLOW_1, OUTPUT);
  pinMode(LED_GREEN_1, OUTPUT);
  pinMode(LED_RED_2, OUTPUT);
  pinMode(LED_YELLOW_2, OUTPUT);
  pinMode(LED_GREEN_2, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Desliga todos os LEDs
  ledOff(LED_RED_1); ledOff(LED_YELLOW_1); ledOff(LED_GREEN_1);
  ledOff(LED_RED_2); ledOff(LED_YELLOW_2); ledOff(LED_GREEN_2);
  
  // Conecta WiFi
  connectWiFi();
  
  // Configura MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  // Configura tópicos
  sprintf(PUBLISH_TOPIC, "/v1.6/devices/%s", DEVICE_LABEL);
  sprintf(SUBSCRIBE_TOPIC, "/v1.6/devices/%s/+/lv", DEVICE_LABEL);
  
  // Conecta MQTT
  connectMQTT();
  
  // Calibra LDR
  calibrateLdr();
  
  // Estados iniciais
  setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, state1);
  setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, state2);
  
  Serial.println("Sistema iniciado!");
}

void loop() {
  // Mantém conexão MQTT
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // Atualiza modo noturno
  updateNightMode();
  
  // Atualiza semáforos
  updateTrafficLights();
  
  // Publica dados no Ubidots
  if (millis() - lastPublish > PUBLISH_INTERVAL) {
    lastPublish = millis();
    publishToUbidots();
  }
  
  delay(10);
}
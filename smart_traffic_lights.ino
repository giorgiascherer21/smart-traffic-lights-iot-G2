enum TrafficLightState { TL_RED, TL_YELLOW, TL_GREEN };
// Estados possíveis do semáforo

void setTrafficLight(int redPin, int yellowPin, int greenPin, TrafficLightState state);
void updateNightMode();
void updateTrafficLights();
void calibrateLdr();
long readUltrasonic();

// Semáforo 1
const int LED_RED_1    = 4;
const int LED_YELLOW_1 = 16;
const int LED_GREEN_1  = 17;

// Semáforo 2
const int LED_RED_2    = 33;
const int LED_YELLOW_2 = 25;
const int LED_GREEN_2  = 26;

// LDR
const int LDR_PIN      = 34;

// Ultrassônico
const int TRIG_PIN     = 5;
const int ECHO_PIN     = 18;

void ledOn(int pin)  { digitalWrite(pin, LOW); }  // LEDs ativos em LOW
void ledOff(int pin) { digitalWrite(pin, HIGH); }

TrafficLightState state1 = TL_RED;   // Estado inicial semáforo 1
TrafficLightState state2 = TL_GREEN; // Estado inicial semáforo 2

bool nightMode = false; // Controle do modo noturno

// LDR
int  ldrValue = 0;
int  LDR_THRESHOLD = 0;  // Limite para definir escuridão
unsigned long lastLdrPrint = 0;

unsigned long lastStateChange = 0;  // Controle de tempo entre estados

long timeRed       = 4000; // Duração do vermelho
long timeYellow    = 3000; // Duração do amarelo
long timeGreen     = 6000; // Duração do verde
long timeNightMode = 500;  // Piscar do modo noturno

// Filtro para suavizar leitura do LDR
int filteredLdr(int samples = 20) {
  long sum = 0;
  for (int i=0; i<samples; i++) {
    sum += analogRead(LDR_PIN);
    delay(2);
  }
  return sum / samples; // Média filtrada
}

void calibrateLdr() {
  // Define threshold com base na luz ambiente no início
  Serial.println("Calibrando LDR (luz ambiente atual = DIA)...");

  long sum = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(LDR_PIN);
    delay(5);
  }

  int base = sum / samples;
  int margin = base * 0.35;
  LDR_THRESHOLD = base - margin;  // Ajuste adaptativo

  if (LDR_THRESHOLD < 20) LDR_THRESHOLD = 20; // Limite mínimo

  Serial.print("Base: ");
  Serial.print(base);
  Serial.print("  Threshold: ");
  Serial.println(LDR_THRESHOLD);
}

long readUltrasonic() {
  // Mede distância pelo tempo do pulso de eco
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long distance = duration * 0.034 / 2;

  return distance;  // Distância em cm
}

void setTrafficLight(int redPin, int yellowPin, int greenPin, TrafficLightState state) {
  // Garante que apenas uma cor fica ativa
  ledOff(redPin);
  ledOff(yellowPin);
  ledOff(greenPin);

  switch (state) {
    case TL_RED:
      ledOn(redPin);
      break;

    case TL_YELLOW:
      ledOn(yellowPin);
      break;

    case TL_GREEN:
      ledOn(greenPin);
      break;
  }
}

void updateNightMode() {
  ldrValue = filteredLdr(30); // Leitura estabilizada

  if (millis() - lastLdrPrint > 1000) {
    lastLdrPrint = millis();
    Serial.print("LDR = ");
    Serial.print(ldrValue);
    Serial.print(" | TH = ");
    Serial.println(LDR_THRESHOLD);
  }

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

    // Restabelece estados normais
    state1 = TL_RED;
    state2 = TL_GREEN;

    setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, state1);
    setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, state2);
  }
}

void forceCarPriority() {
  long distance = readUltrasonic();

  if (distance > 0 && distance < 7) { // Carro muito próximo
    Serial.print("Carro detectado a ");
    Serial.print(distance);
    Serial.println(" cm — PRIORIDADE!");

    // Força abertura do semáforo 1
    state1 = TL_GREEN;
    state2 = TL_RED;

    setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, TL_GREEN);
    setTrafficLight(LED_RED_2, LED_YELLOW_2, LED_GREEN_2, TL_RED);

    lastStateChange = millis(); // Reinicia ciclo após prioridade
  }
}

void updateTrafficLights() {
  unsigned long currentTime = millis();

  if (nightMode) {
    // Modo noturno: apenas piscar amarelo
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

  // Verifica prioridade para veículos
  forceCarPriority();

  long stateDuration = 0;

  switch (state1) {
    case TL_RED:    stateDuration = timeRed;    break;
    case TL_YELLOW: stateDuration = timeYellow; break;
    case TL_GREEN:  stateDuration = timeGreen;  break;
  }

  if (currentTime - lastStateChange >= stateDuration) {
    lastStateChange = currentTime;

    // Transições coordenadas dos dois semáforos
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

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // Configuração dos LEDs dos dois semáforos
  pinMode(LED_RED_1, OUTPUT);
  pinMode(LED_YELLOW_1, OUTPUT);
  pinMode(LED_GREEN_1, OUTPUT);

  pinMode(LED_RED_2, OUTPUT);
  pinMode(LED_YELLOW_2, OUTPUT);
  pinMode(LED_GREEN_2, OUTPUT);

  pinMode(LDR_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Garante LEDs apagados no início
  ledOff(LED_RED_1); ledOff(LED_YELLOW_1); ledOff(LED_GREEN_1);
  ledOff(LED_RED_2); ledOff(LED_YELLOW_2); ledOff(LED_GREEN_2);

  calibrateLdr(); // Ajuste inicial de luminosidade

  // Estados iniciais
  setTrafficLight(LED_RED_1, LED_YELLOW_1, LED_GREEN_1, state1);
  setTrafficLight(LED_RED_

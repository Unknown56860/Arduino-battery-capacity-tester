
#define VOLTAGE_PIN A7 // Must be an analog pin
#define CURRENT_PIN A6 // Must be an analog pin
#define RELAY_PIN 4    // Any digital/analog pin

#define MAX_BATTERY_VOLT 4.15  // Maximum voltage before cutoff
#define MIN_BATTERY_VOLT 2.95  // Minimum voltage before cutoff

#define R1 4700  // Resistor between battery +ve and arduino analog's pin
#define R2 980   // Resistor between arduino's analog pin and ground

#define ACS_STEP_VOLT 0.185 // Voltage increase on arduino's pin for every amp
// I've used ACS7127 with max rating of 5A.

#define AREF_VOLT 3.285  // Voltage at the Aref/ref pin of the arduino (never exceed 5 volts)
#define ADC_MAX 1023     // Max value of adc

#define POLL_INTERVAL 1000 // Update data every 1000 ms
#define SAMPLE_INTERVAL 50 // Take a voltage and currrent sample every 50 ms

unsigned long poll_interval, sample_interval;

float ACS_IDLE_VAL;
unsigned long curMillis;
unsigned long voltage, current, sampleCount;
float volts, amps, watts, watthour;

void setup() {
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  analogReference(EXTERNAL);

  ACS_IDLE_VAL = 0;
  for(int i=0; i<200; i++) {
    ACS_IDLE_VAL += analogRead(CURRENT_PIN);
    delay(2);
  }
  ACS_IDLE_VAL = ACS_IDLE_VAL / 200;

  voltage = 0; current = 0; sampleCount = 0;
  volts = 0.0; amps = 0.0; watts = 0.0; watthour = 0.0;

  blinkLed(2, 500);
  Serial.begin(115200);
  Serial.println("START");

  curMillis = millis();
  poll_interval = curMillis;
  sample_interval = curMillis;
}

void loop() {
  curMillis = millis();
  if((int32_t)(curMillis - poll_interval) >= 0) {
    poll_interval += POLL_INTERVAL;
    if(sampleCount==0) { sampleCount = 1; }
    volts = voltage * AREF_VOLT * (R1 + R2) / R2 / ADC_MAX / sampleCount;
    amps = ((current / sampleCount) - ACS_IDLE_VAL) * AREF_VOLT / ADC_MAX / ACS_STEP_VOLT;
    watts = volts * amps;
    watthour = watthour + (watts * POLL_INTERVAL / 1000 / 3600);
    // Log serial data here...
    Serial.print(volts); Serial.print(", ");
    Serial.print(amps); Serial.print(", ");
    Serial.print(watts); Serial.print(", ");
    Serial.println(watthour);
    voltage = 0; current = 0; sampleCount = 0;
  }
  if((int32_t)(curMillis - sample_interval) >= 0) {
    sample_interval += SAMPLE_INTERVAL;
    voltage += analogRead(VOLTAGE_PIN);
    current += analogRead(CURRENT_PIN);
    sampleCount++;
  }
  if(volts>MAX_BATTERY_VOLT || volts<MIN_BATTERY_VOLT) {
    digitalWrite(LED_BUILTIN, HIGH); digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW); digitalWrite(RELAY_PIN, LOW);
  }
}

void blinkLed(int blinks, int duration) {
  int gap = duration/2;
  for(int i=0; i<blinks; i++) {
    digitalWrite(LED_BUILTIN, HIGH); delay(gap);
    digitalWrite(LED_BUILTIN, LOW); delay(gap);
  }
}

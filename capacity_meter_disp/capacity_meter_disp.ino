#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define VOLTAGE_PIN A7 // Must be an analog pin
#define CURRENT_PIN A6 // Must be an analog pin
#define RELAY_PIN 4    // Any digital/analog pin
#define DISPLAY_ADDRESS 0x3C  // oled address: 0X3C (default) or 0x3D

#define MAX_BATTERY_VOLT 4.15  // Maximum voltage before charging cutoff
#define MIN_BATTERY_VOLT 2.95  // Minimum voltage before discharging cutoff

#define R1 4700  // Resistor between battery +ve and arduino analog's pin
#define R2 980   // Resistor between arduino's analog pin and ground

#define ACS_STEP_VOLT 0.185 // Voltage increase on arduino's pin for every amp
// I've used ACS7127 with max rating of 5A.

#define POLL_INTERVAL 1000 // Update data every 1000 ms
#define SAMPLE_INTERVAL 50 // Take a voltage and currrent sample every 50 ms

#define AREF_VOLT 3.285  // Voltage at the Aref/ref pin of the arduino (never exceed 5 volts)
#define ADC_MAX 1023     // Max value of adc

unsigned long poll_interval, sample_interval;

Adafruit_SSD1306 disp(128, 64, &Wire, -1);

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

  Wire.begin();
  while(!disp.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    blinkLed(1, 1000);
  }

  blinkLed(2, 500);

  ACS_IDLE_VAL = 0;
  for(int i=0; i<200; i++) {
    ACS_IDLE_VAL += analogRead(CURRENT_PIN);
    delay(2);
  }
  ACS_IDLE_VAL = ACS_IDLE_VAL / 200;

  voltage = 0; current = 0; sampleCount = 0;
  volts = 0.0; amps = 0.0; watts = 0.0; watthour = 0.0;

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
    amps = (((float)current / sampleCount) - ACS_IDLE_VAL) * AREF_VOLT / ADC_MAX / ACS_STEP_VOLT;
    watts = volts * amps;
    watthour = watthour + (watts * POLL_INTERVAL / 1000 / 3600);
    displayValue();
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

void displayValue() {
  disp.clearDisplay(); disp.setTextColor(WHITE);
  disp.setCursor(0, 0);  disp.print("Voltage: ");  disp.print(volts);
  disp.setCursor(0, 12); disp.print("Current: ");  disp.print(amps);
  disp.setCursor(0, 24); disp.print("Power: ");    disp.print(watts);
  disp.setCursor(0, 36); disp.print("Watthour: "); disp.print(watthour);
  disp.display();
}


void blinkLed(int blinks, int duration) {
  int gap = duration/2;
  for(int i=0; i<blinks; i++) {
    digitalWrite(LED_BUILTIN, HIGH); delay(gap);
    digitalWrite(LED_BUILTIN, LOW); delay(gap);
  }
}

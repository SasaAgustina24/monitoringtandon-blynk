#define BLYNK_PRINT Serial
#define BLYNK_NO_BANNER  // Menonaktifkan banner Blynk

#define BLYNK_TEMPLATE_ID "TMPL6cApJWWEv"
#define BLYNK_TEMPLATE_NAME "Monitoring Tandon"
#define BLYNK_AUTH_TOKEN "r_0Lgn1wQGhSbIR-RQuS9BFLxtwRjo24"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "OPPO Reno10 5G";
char pass[] = "sasa2405";

// Pin Definitions
#define TURBIDITY_SENSOR 35   // GPIO35 (Analog Input)
#define TURBIDITY_DIGITAL 32  // GPIO32 (Digital Output dari Sensor)
#define ULTRASONIC_TRIG 19    // GPIO19
#define ULTRASONIC_ECHO 18    // GPIO18
#define BUZZER 25             // GPIO25
#define RELAY 5               // GPIO5

// Variabel untuk filter moving average
#define NUM_READINGS 5  
float turbidityReadings[NUM_READINGS];
int readIndex = 0;
float totalTurbidity = 0;
String previousWaterStatus = "";

// Variabel Timer Buzzer
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;
bool buzzerCooldown = false;
unsigned long cooldownStartTime = 0;
const unsigned long buzzerDuration = 5 * 60 * 1000; // 5 menit dalam milidetik
const unsigned long cooldownDuration = 20 * 60 * 1000; // 20 menit dalam milidetik

// Callback function ketika tombol di Blynk diklik
BLYNK_WRITE(V0) {
    int buttonBlynk = param.asInt();
    Serial.print("Data dari Blynk: ");
    Serial.println(buttonBlynk);
    if (buttonBlynk == 1) {
        digitalWrite(RELAY, HIGH);
    } else {
        digitalWrite(RELAY, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nSystem starting...");

    analogReadResolution(12);
    pinMode(TURBIDITY_SENSOR, INPUT);
    pinMode(TURBIDITY_DIGITAL, INPUT_PULLUP);
    pinMode(ULTRASONIC_TRIG, OUTPUT);
    pinMode(ULTRASONIC_ECHO, INPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(RELAY, OUTPUT);

    digitalWrite(BUZZER, LOW);

    for (int i = 0; i < NUM_READINGS; i++) {
        turbidityReadings[i] = 0;
    }

    Serial.println("Connecting to WiFi...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to WiFi: ");
        Serial.println(ssid);
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect to WiFi. Check credentials.");
    }
}

float readTurbidity() {
    totalTurbidity -= turbidityReadings[readIndex];
    int rawValue = analogRead(TURBIDITY_SENSOR);
    float voltage = rawValue * (3.3 / 4095.0);
    float ntu;
    if (voltage >= 2.8) {
        ntu = 0;
    } else if (voltage >= 1.8) {
        ntu = map(voltage * 100, 280, 180, 10, 25);
    } else {
        ntu = map(voltage * 100, 180, 100, 25, 50);
    }
    ntu = constrain(ntu, 0, 50);

    turbidityReadings[readIndex] = ntu;
    totalTurbidity += ntu;
    readIndex = (readIndex + 1) % NUM_READINGS;

    return totalTurbidity / NUM_READINGS;
}

float readUltrasonic() {
    digitalWrite(ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG, LOW);

    long duration = pulseIn(ULTRASONIC_ECHO, HIGH);
    float distance = duration * 0.034 / 2;
    return (distance > 400 || distance < 2) ? -1 : distance;
}

void loop() {
    float turbidity = readTurbidity();
    float distance = readUltrasonic();
    unsigned long currentMillis = millis();

    Serial.print("Turbidity: "); Serial.print(turbidity, 1); Serial.println(" NTU");
    Blynk.virtualWrite(V1, turbidity);

    if (distance >= 0) {
        Serial.print("Water Level: "); Serial.print(distance, 1); Serial.println(" cm");
        Blynk.virtualWrite(V2, distance);
    } else {
        Serial.println("Ultrasonic reading error");
    }

    String currentWaterStatus = "";
    if (turbidity < 10) {
        currentWaterStatus = "Air Jernih";
        buzzerActive = false;
        digitalWrite(BUZZER, LOW);
    } else if (turbidity >= 10 && turbidity <= 25) {
        currentWaterStatus = "Air Cukup Keruh";
        buzzerActive = false;
        digitalWrite(BUZZER, LOW);
    } else {
        currentWaterStatus = "Air Sangat Keruh";
        if (!buzzerCooldown) {
            if (!buzzerActive) {
                buzzerStartTime = currentMillis;
                buzzerActive = true;
                digitalWrite(BUZZER, HIGH);
                Blynk.logEvent("air_keruh", "");
                Serial.println("Buzzer AKTIF! Air sangat keruh!");
            }
            if (buzzerActive && currentMillis - buzzerStartTime >= buzzerDuration) {
                digitalWrite(BUZZER, LOW);
                buzzerActive = false;
                buzzerCooldown = true;
                cooldownStartTime = currentMillis;
                Serial.println("Buzzer dimatikan, masuk ke cooldown.");
            }
        }
        if (buzzerCooldown && currentMillis - cooldownStartTime >= cooldownDuration) {
            buzzerCooldown = false;
            Serial.println("Cooldown selesai, buzzer siap digunakan lagi.");
        }
    }
    previousWaterStatus = currentWaterStatus;

    Blynk.run();
    Serial.println("------------------------");
    delay(1000);
}

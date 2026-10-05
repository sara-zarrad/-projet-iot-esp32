#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ==========================================
// 1. Configuration des Broches & Matériel
// ==========================================
#define DHTPIN 15         // Capteur DHT22 connecté au GPIO 15
#define DHTTYPE DHT22     // Type de capteur : DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LED_1_PIN 22      // LED 1 (Verte) connectée au GPIO 22
#define LED_2_PIN 23      // LED 2 (Bleue) connectée au GPIO 23

// ==========================================
// 2. Configuration Wi-Fi
// ==========================================
// Sur simulateur Wokwi : "Wokwi-GUEST" (sans mot de passe)
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// ==========================================
// 3. Configuration Firebase Realtime Database
// ==========================================
#define FIREBASE_HOST "projetiot-e1bdb-default-rtdb.europe-west1.firebasedatabase.app"
// Variables locales
bool led1State = false;
bool led2State = false;

// ==========================================
// 4. Fonction : Récupérer l'état des LEDs (GET)
// ==========================================
void getLedStatesFromFirebase() {
String url = String("https://") + FIREBASE_HOST + "/.json";
  HTTPClient http;

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("[Firebase GET] Réponse : " + payload);

    // Extraction LED 1 (prise en charge de "Led_1" et "led_1")
    if (payload.indexOf("\"Led_1\":true") != -1 || payload.indexOf("\"led_1\":true") != -1) {
      led1State = true;
    } else if (payload.indexOf("\"Led_1\":false") != -1 || payload.indexOf("\"led_1\":false") != -1) {
      led1State = false;
    }

    // Extraction LED 2 (prise en charge de "Led_2" et "led_2")
    if (payload.indexOf("\"Led_2\":true") != -1 || payload.indexOf("\"led_2\":true") != -1) {
      led2State = true;
    } else if (payload.indexOf("\"Led_2\":false") != -1 || payload.indexOf("\"led_2\":false") != -1) {
      led2State = false;
    }

    // Application immédiate sur les broches GPIO
    digitalWrite(LED_1_PIN, led1State ? HIGH : LOW);
    digitalWrite(LED_2_PIN, led2State ? HIGH : LOW);

    Serial.print("   -> LED 1 (GPIO 22): "); Serial.print(led1State ? "ON" : "OFF");
    Serial.print(" | LED 2 (GPIO 23): "); Serial.println(led2State ? "ON" : "OFF");
  } else {
    Serial.print("[Firebase GET Erreur] Code HTTP : ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ==========================================
// 5. Fonction : Envoyer Température & Humidité (PATCH)
// ==========================================
void sendSensorDataToFirebase() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("[DHT22] Erreur de lecture du capteur !");
    return;
  }

  Serial.println("----------------------------------------");
  Serial.print("[DHT22] Température : "); Serial.print(temperature, 1); Serial.print(" °C | ");
  Serial.print("Humidité : "); Serial.print(humidity, 1); Serial.println(" %");

  // Format JSON envoyé à Firebase
  String jsonPayload = "{\"temperature\":" + String(temperature, 1) + ",\"humidity\":" + String(humidity, 1) + "}";
String url = String("https://") + FIREBASE_HOST + "/sensors.json";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // PATCH met à jour sans écraser les clés Led_1 et Led_2
  int httpResponseCode = http.PATCH(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.println("[Firebase PATCH] Capteurs synchronisés (Code " + String(httpResponseCode) + ")");
  } else {
    Serial.print("[Firebase PATCH Erreur] Code : ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ==========================================
// 6. Initialisation (Setup)
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Démarrage ESP32 IoT - Firebase & DHT22 ===");

  // Configuration des broches LEDs
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);

  // Initialisation du capteur DHT22
  dht.begin();

  // Connexion Wi-Fi
  Serial.print("Connexion au Wi-Fi ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Connecté avec succès !");
  Serial.print("[Wi-Fi] Adresse IP : ");
  Serial.println(WiFi.localIP());
}

// ==========================================
// 7. Boucle Principale (Loop)
// ==========================================
void loop() {
  // 1. Envoyer les données du capteur DHT22 vers Firebase
  sendSensorDataToFirebase();

  // 2. Récupérer l'état des LEDs depuis Firebase et les commuter
  getLedStatesFromFirebase();

  // 3. Pause de 2 secondes avant la prochaine itération
  delay(2000);
}

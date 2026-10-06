#include <Arduino.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ---- Matériel ----
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define LED_1_PIN 22
#define LED_2_PIN 23

// ---- Wi-Fi / Firebase ----
#define WIFI_SSID "Sousou iphone"
#define WIFI_PASSWORD "sarasir080102"
#define FIREBASE_HOST                                                          \
  "projetiot-e1bdb-default-rtdb.europe-west1.firebasedatabase.app"

WiFiClientSecure client;
bool led1State = false, led2State = false;

// Reconnecte le Wi-Fi si besoin et force un DNS public (corrige "DNS Failed")
bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  Serial.println("[Wi-Fi] Reconnexion...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
    delay(500);
  if (WiFi.status() != WL_CONNECTED)
    return false;
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  return true;
}

// Résout le nom de domaine avec 3 essais avant d'abandonner
bool dnsOk() {
  IPAddress ip;
  for (int i = 0; i < 3; i++) {
    if (WiFi.hostByName(FIREBASE_HOST, ip))
      return true;
    delay(500);
  }
  Serial.println("[DNS] Résolution impossible : " + String(FIREBASE_HOST));
  return false;
}

void getLedStatesFromFirebase() {
  String url = String("https://") + FIREBASE_HOST + "/.json";
  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();

  if (code > 0) {
    String payload = http.getString();
    Serial.println("[Firebase GET] " + payload);
    if (payload.indexOf("\"led_1\":true") != -1)
      led1State = true;
    else if (payload.indexOf("\"led_1\":false") != -1)
      led1State = false;
    if (payload.indexOf("\"led_2\":true") != -1)
      led2State = true;
    else if (payload.indexOf("\"led_2\":false") != -1)
      led2State = false;

    digitalWrite(LED_1_PIN, led1State ? HIGH : LOW);
    digitalWrite(LED_2_PIN, led2State ? HIGH : LOW);
    Serial.println("   -> LED 1 : " + String(led1State ? "ON" : "OFF") +
                   " | LED 2 : " + String(led2State ? "ON" : "OFF"));
  } else {
    Serial.println("[Firebase GET Erreur] Code : " + String(code));
  }
  http.end();
}

// ---- Journal (historique) ----
#define LOG_INTERVAL 30000 // une entrée toutes les 30 s
unsigned long lastLog = 0;

// POST = Firebase crée une clé unique à chaque entrée dans /history
void logHistoryToFirebase(float t, float h) {
  String json =
      "{\"temperature\":" + String(t, 1) + ",\"humidity\":" + String(h, 1) +
      ",\"ts\":{\".sv\":\"timestamp\"}}"; // horodatage fourni par le serveur
  String url = String("https://") + FIREBASE_HOST + "/history.json";
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  Serial.println(code > 0
                     ? "[Historique] entrée ajoutée (Code " + String(code) + ")"
                     : "[Historique Erreur] Code : " + String(code));
  http.end();
}

void sendSensorDataToFirebase() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT22] Erreur de lecture du capteur !");
    return;
  }
  Serial.println("[DHT22] Température : " + String(t, 1) +
                 " °C | Humidité : " + String(h, 1) + " %");

  String json = "{\"temperature\":" + String(t, 1) +
                ",\"humidity\":" + String(h, 1) + "}";
  String url = String("https://") + FIREBASE_HOST + "/sensors.json";
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(json);
  Serial.println(code > 0 ? "[Firebase PATCH] OK (Code " + String(code) + ")"
                          : "[Firebase PATCH Erreur] Code : " + String(code));
  http.end();

  if (code > 0 && (lastLog == 0 || millis() - lastLog >= LOG_INTERVAL)) {
    logHistoryToFirebase(t, h);
    lastLog = millis();
  }
}
void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  dht.begin();
  client.setInsecure(); // HTTPS sans vérification de certificat (TP)

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au Wi-Fi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  Serial.println("\n[Wi-Fi] Connecté, IP : " + WiFi.localIP().toString());
}

void loop() {
  if (ensureWifi() && dnsOk()) {
    sendSensorDataToFirebase();
    getLedStatesFromFirebase();
  }
  delay(2000);
}

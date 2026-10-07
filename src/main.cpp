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
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define FIREBASE_HOST                                                          \
  "projetiot-e1bdb-default-rtdb.europe-west1.firebasedatabase.app"

// ---- Réglages ----
#define LOOP_DELAY 2000    // pause entre deux cycles (ms)
#define LOG_INTERVAL 20000 // une entrée dans /log toutes les 20 s
#define LOG_SIZE 20        // nombre d'entrées gardées dans /log
#define HTTP_TIMEOUT 8000  // délai max par requête (ms)

WiFiClientSecure client;
bool led1State = false, led2State = false;
unsigned long lastLog = 0;
bool firstLogDone = false;
int logNext = 0;             // prochaine case à écrire (0 à LOG_SIZE-1)
bool logIndexLoaded = false; // l'index a-t-il été relu depuis Firebase ?

// ---------- Réseau ----------
void setDns() {
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
              IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
}

const char *wifiStatusToString(wl_status_t status) {
  switch (status) {
  case WL_NO_SHIELD:
    return "NO_SHIELD";
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "SSID_INTROUVABLE (Vérifier 2.4GHz / Nom SSID)";
  case WL_SCAN_COMPLETED:
    return "SCAN_COMPLETED";
  case WL_CONNECTED:
    return "CONNECTE";
  case WL_CONNECT_FAILED:
    return "ECHEC_CONNEXION (Mauvais mot de passe ?)";
  case WL_CONNECTION_LOST:
    return "CONNEXION_PERDUE";
  case WL_DISCONNECTED:
    return "DECONNECTE";
  default:
    return "INCONNU";
  }
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  Serial.println("\n[Wi-Fi] Reconnexion...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[Wi-Fi Erreur] Statut : " +
                   String(wifiStatusToString(WiFi.status())));
    return false;
  }
  setDns();
  Serial.println("\n[Wi-Fi] Reconnecté !");
  return true;
}

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

// Prépare une requête HTTPS propre (connexion neuve à chaque fois)
void beginRequest(HTTPClient &http, const String &path) {
  client.stop(); // évite une connexion TLS périmée
  http.setReuse(false);
  http.setTimeout(HTTP_TIMEOUT);
  http.begin(client, String("https://") + FIREBASE_HOST + path);
}

// ---------- LEDs : lecture de /led_1 et /led_2 (petites réponses) ----------
bool readLed(const char *path, bool &state) {
  HTTPClient http;
  beginRequest(http, String("/") + path + ".json");
  int code = http.GET();
  if (code != 200) {
    Serial.println("[GET " + String(path) + " Erreur] " +
                   http.errorToString(code) + " (" + String(code) + ")");
    http.end();
    return false;
  }
  String payload = http.getString(); // "true", "false" ou "null"
  http.end();
  if (payload.indexOf("true") != -1)
    state = true;
  else if (payload.indexOf("false") != -1)
    state = false; // "null" : on garde l'état
  return true;
}

void getLedStatesFromFirebase() {
  readLed("led_1", led1State);
  readLed("led_2", led2State);
  digitalWrite(LED_1_PIN, led1State ? HIGH : LOW);
  digitalWrite(LED_2_PIN, led2State ? HIGH : LOW);
  Serial.println("   -> LED 1 : " + String(led1State ? "ON" : "OFF") +
                 " | LED 2 : " + String(led2State ? "ON" : "OFF"));
}

// ---------- Journal circulaire : /log/0 ... /log/19 ----------
// Relit /log_next (une seule fois après un redémarrage) pour reprendre
// à la bonne case et ne pas écraser des mesures récentes.
void loadLogIndex() {
  HTTPClient http;
  beginRequest(http, "/log_next.json");
  int code = http.GET();
  if (code == 200) {
    String payload =
        http.getString(); // un nombre, ou "null" si premier démarrage
    http.end();
    logNext = payload.toInt() % LOG_SIZE;
    logIndexLoaded = true;
    Serial.println("[Journal] prochaine case : " + String(logNext));
  } else {
    Serial.println("[Journal] lecture de l'index impossible (" + String(code) +
                   ")");
    http.end();
  }
}

// Une seule requête PATCH écrit la mesure ET met à jour l'index (mise à jour
// multi-chemins de Firebase). Résultat : /log ne contient jamais plus de 20
// entrées.
bool logToFirebase(float t, float h) {
  int next = (logNext + 1) % LOG_SIZE;
  String entry =
      "{\"temperature\":" + String(t, 1) + ",\"humidity\":" + String(h, 1) +
      ",\"ts\":{\".sv\":\"timestamp\"}}"; // heure fournie par Firebase
  String body = "{\"log/" + String(logNext) + "\":" + entry +
                ",\"log_next\":" + String(next) + "}";

  HTTPClient http;
  beginRequest(http, "/.json");
  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(body);
  bool ok = (code == 200);
  Serial.println(ok ? "[Journal] mesure enregistrée dans /log/" +
                          String(logNext)
                    : "[Journal Erreur] " + http.errorToString(code) + " (" +
                          String(code) + ")");
  http.end();
  if (ok)
    logNext = next;
  return ok;
}

// ---------- Capteur : PATCH dans /sensors ----------
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
  HTTPClient http;
  beginRequest(http, "/sensors.json");
  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(json);
  bool ok = (code == 200);
  Serial.println(ok ? "[Firebase PATCH] OK (Code 200)"
                    : "[Firebase PATCH Erreur] " + http.errorToString(code) +
                          " (" + String(code) + ")");
  http.end();

  // Journal : au premier cycle, puis toutes les LOG_INTERVAL ms.
  // lastLog n'est mis à jour que si l'écriture a réussi, sinon on réessaie.
  if (ok && (!firstLogDone || millis() - lastLog >= LOG_INTERVAL)) {
    if (!logIndexLoaded)
      loadLogIndex();
    if (logToFirebase(t, h)) {
      firstLogDone = true;
      lastLog = millis();
    }
  }
}

// ---------- Programme ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("===      ESP32 Serre connectée       ===");
  Serial.println("========================================");

  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  dht.begin();
  client.setInsecure(); // HTTPS sans vérification de certificat (TP)

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  // --- Scan des réseaux Wi-Fi disponibles ---
  Serial.println("\n[Scan Wi-Fi] Recherche des réseaux 2.4 GHz disponibles...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[Scan Wi-Fi] Aucun réseau trouvé !");
  } else {
    Serial.printf("[Scan Wi-Fi] %d réseaux détectés :\n", n);
    bool foundTarget = false;
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      Serial.printf("  %d: \"%s\" (%d dBm)\n", i + 1, ssid.c_str(), rssi);
      if (ssid == WIFI_SSID)
        foundTarget = true;
    }
    if (foundTarget) {
      Serial.println("  ==> Réseau \"" + String(WIFI_SSID) +
                     "\" BIEN DÉTECTÉ !");
    } else {
      Serial.println("  ==> ATTENTION : Réseau \"" + String(WIFI_SSID) +
                     "\" NON DÉTECTÉ dans la liste ci-dessus !");
    }
  }

  // --- Tentative de connexion ---
  Serial.println("\n[Wi-Fi] Connexion à : \"" + String(WIFI_SSID) + "\" ...");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setDns();
    Serial.println("\n[Wi-Fi] Connecté avec succès !");
    Serial.println("[Wi-Fi] Adresse IP : " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[Wi-Fi] Échec ! Statut : " +
                   String(wifiStatusToString(WiFi.status())));
  }
}

void loop() {
  if (ensureWifi() && dnsOk()) {
    sendSensorDataToFirebase();
    getLedStatesFromFirebase();
  }
  delay(LOOP_DELAY);
}
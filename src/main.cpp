#include <Arduino.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <lwip/dns.h>

// ==========================================
// 1. Configuration des Broches & Matériel
// ==========================================
#define DHTPIN 15     // Capteur DHT22 connecté au GPIO 15
#define DHTTYPE DHT22 // Type de capteur : DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LED_1_PIN 22 // LED 1 (Verte) connectée au GPIO 22
#define LED_2_PIN 23 // LED 2 (Bleue) connectée au GPIO 23

// ==========================================
// 2. Configuration Wi-Fi
// ==========================================
// Sur simulateur Wokwi : "Wokwi-GUEST" (sans mot de passe)
#define WIFI_SSID "Sousou iphone"
#define WIFI_PASSWORD "sarasir080102"

// ==========================================
// 3. Configuration Firebase Realtime Database
// ==========================================
#define FIREBASE_HOST                                                          \
  "projetiot-e1bdb-default-rtdb.europe-west1.firebasedatabase.app"
// Variables locales
bool led1State = false;
bool led2State = false;

// ==========================================
// 4. Fonction : Récupérer l'état des LEDs (GET)
// ==========================================
void getLedStatesFromFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Wi-Fi] Non connecté, abandon du GET.");
    return;
  }

  String url = String("https://") + FIREBASE_HOST + "/.json";

  WiFiClientSecure client;
  client
      .setInsecure(); // Permet la connexion HTTPS sans certificat racine lourd
  client.setTimeout(5); // 5s timeout

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(5000);

  if (http.begin(client, url)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("[Firebase GET] Réponse : " + payload);

      // Extraction LED 1 (prise en charge de "Led_1" et "led_1")
      if (payload.indexOf("\"Led_1\":true") != -1 ||
          payload.indexOf("\"led_1\":true") != -1) {
        led1State = true;
      } else if (payload.indexOf("\"Led_1\":false") != -1 ||
                 payload.indexOf("\"led_1\":false") != -1) {
        led1State = false;
      }

      // Extraction LED 2 (prise en charge de "Led_2" et "led_2")
      if (payload.indexOf("\"Led_2\":true") != -1 ||
          payload.indexOf("\"led_2\":true") != -1) {
        led2State = true;
      } else if (payload.indexOf("\"Led_2\":false") != -1 ||
                 payload.indexOf("\"led_2\":false") != -1) {
        led2State = false;
      }

      // Application immédiate sur les broches GPIO
      digitalWrite(LED_1_PIN, led1State ? HIGH : LOW);
      digitalWrite(LED_2_PIN, led2State ? HIGH : LOW);

      Serial.print("   -> LED 1 (GPIO 22): ");
      Serial.print(led1State ? "ON" : "OFF");
      Serial.print(" | LED 2 (GPIO 23): ");
      Serial.println(led2State ? "ON" : "OFF");
    } else {
      Serial.print("[Firebase GET Erreur] Code HTTP : ");
      Serial.print(httpResponseCode);
      Serial.print(" - ");
      Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("[Firebase GET Erreur] Impossible d'initialiser HTTPClient");
  }
}

// ==========================================
// 5. Fonction : Envoyer Température & Humidité (PATCH)
// ==========================================
void sendSensorDataToFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Wi-Fi] Non connecté, abandon du PATCH.");
    return;
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("[DHT22] Erreur de lecture du capteur !");
    return;
  }

  Serial.println("----------------------------------------");
  Serial.print("[DHT22] Température : ");
  Serial.print(temperature, 1);
  Serial.print(" °C | ");
  Serial.print("Humidité : ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  // Format JSON envoyé à Firebase
  String jsonPayload = "{\"temperature\":" + String(temperature, 1) +
                       ",\"humidity\":" + String(humidity, 1) + "}";
  String url = String("https://") + FIREBASE_HOST + "/sensors.json";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5);

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(5000);

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");

    // PATCH met à jour sans écraser les clés Led_1 et Led_2
    int httpResponseCode = http.PATCH(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.println("[Firebase PATCH] Capteurs synchronisés (Code " +
                     String(httpResponseCode) + ")");
    } else {
      Serial.print("[Firebase PATCH Erreur] Code : ");
      Serial.print(httpResponseCode);
      Serial.print(" - ");
      Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println(
        "[Firebase PATCH Erreur] Impossible d'initialiser HTTPClient");
  }
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

  // Connexion Wi-Fi (DHCP automatique)
  Serial.print("Connexion au Wi-Fi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Connecté avec succès !");
  Serial.print("   -> IP ESP32 : ");
  Serial.println(WiFi.localIP());
  Serial.print("   -> Passerelle : ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("   -> DNS DHCP  : ");
  Serial.println(WiFi.dnsIP());

  // Définir un serveur DNS public (Google 8.8.8.8) au niveau lwIP pour
  // fiabiliser la résolution
  ip_addr_t dns_google;
  dns_google.type = IPADDR_TYPE_V4;
  dns_google.u_addr.ip4.addr = ipaddr_addr("8.8.8.8");
  dns_setserver(0, &dns_google);

  // Test de résolution DNS direct
  IPAddress resolvedIP;
  if (WiFi.hostByName(FIREBASE_HOST, resolvedIP)) {
    Serial.print("   -> DNS Résolu avec succès : ");
    Serial.print(FIREBASE_HOST);
    Serial.print(" = ");
    Serial.println(resolvedIP);
  } else {
    Serial.print("   -> [AVERTISSEMENT] Résolution DNS échouée pour : ");
    Serial.println(FIREBASE_HOST);
  }
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

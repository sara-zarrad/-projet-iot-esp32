# 🌐 Système IoT ESP32 avec Firebase & Dashboard Web

Projet complet d'Internet des Objets (IoT) combinant un microcontrôleur **ESP32**, un capteur environnemental **DHT22**, deux **LEDs de contrôle**, une base de données temps réel **Firebase Realtime Database**, et un **Dashboard Web moderne**.

---

## 📋 Fonctionnalités

- **Capteur DHT22 (GPIO 15)** : Mesure continue de la température et de l'humidité relative.
- **Synchronisation Firebase** :
  - Envoi périodique des mesures des capteurs vers Firebase (`/sensors.json`) via des requêtes HTTP `PATCH`.
  - Récupération en temps réel de l'état des actionneurs (`/Led_1` et `/Led_2`) via requêtes HTTP `GET`.
- **Contrôle d'Actionneurs** :
  - **LED 1 Verte** connectée sur le GPIO 22.
  - **LED 2 Bleue** connectée sur le GPIO 23.
- **Simulation Wokwi** : Schéma complet prêt à être simulé directement avec [diagram.json](diagram.json) et [wokwi.toml](wokwi.toml).
- **Interface Web (Dashboard)** : Tableau de bord interactif dans [public/index.html](public/index.html) pour visualiser les données et télécommander les LEDs.

---

## 🛠️ Architecture du Projet

```text
├── src/
│   └── main.cpp          # Code source C++ Arduino pour ESP32
├── public/
│   └── index.html        # Dashboard Web interactif (HTML5 / CSS / JS)
├── diagram.json          # Schéma de câblage Wokwi
├── wokwi.toml            # Configuration de la simulation Wokwi
├── platformio.ini        # Configuration PlatformIO et dépendances
├── firebase.json         # Configuration du déploiement Firebase Hosting
└── .gitignore
```

---

## 🚀 Démarrage Rapide

### 1. Prérequis
- [PlatformIO](https://platformio.org/) (extension VS Code ou CLI)
- [Wokwi Simulator](https://wokwi.com/) (pour tester en simulation sans matériel physique)

### 2. Compilation du Firmware ESP32
```bash
pio run
```

### 3. Simulation Wokwi
Ouvrez le projet avec l'extension Wokwi dans VS Code et démarrez la simulation via le fichier `diagram.json`.

---

## 🔒 Configuration Firebase
Pour connecter votre propre instance Firebase :
1. Créez un projet sur la console [Firebase](https://console.firebase.google.com/).
2. Activez **Realtime Database** et définissez les règles de lecture/écriture selon vos besoins.
3. Mettez à jour la constante `FIREBASE_HOST` dans [src/main.cpp](src/main.cpp) et l'URL dans [public/index.html](public/index.html).

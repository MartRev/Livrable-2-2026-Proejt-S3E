# 🌦️ Station Météo – Projet Arduino / Grove

## 📌 Présentation du projet
Ce projet consiste à développer une station météorologique autonome intégrant :
- Capteurs environnementaux (température, humidité, pression…)
- Module GPS v1.2
- Horloge RTC v1.2
- Stockage sur carte SD
- Modes utilisateur (standard, configuration, maintenance, économie)
- Gestion d’erreurs et boutons poussoirs

---

## 🎯 Objectifs
- Acquérir et horodater des données environnementales
- Enregistrer les données sur carte SD
- Permettre la configuration via interface série
- Assurer un fonctionnement robuste et autonome

---

## 🧩 Architecture générale du système

```mermaid
flowchart LR
    %% Acteurs
    Utilisateur((Utilisateur))
    Technicien((Technicien))
    Capteurs((Capteurs))
    SD((Carte SD))
    RTC((RTC))
    GPS((GPS))

    %% Système
    subgraph Systeme["Systeme de la station météorologique"]
        UC1[Demarrer en mode standard]
        UC2[Acquerir les donnees des capteurs]
        UC3[Horodater les donnees]
        UC4[Enregistrer les donnees sur carte SD]

        UC5[Demarrer en mode configuration]
        UC6[Configurer les parametres du systeme]
        UC7[Configurer date et heure RTC]

        UC8[Acceder au mode maintenance]
        UC9[Consulter les donnees via interface serie]
        UC10[Remplacer la carte SD en securite]

        UC11[Activer le mode economique]
        UC12[Reduire la frequence d acquisition et désactiver certain capteurs]

        UC13[Detecter et signaler les erreurs]
        UC14[Action sur les boutons poussoirs]
    end

    %% Liens Utilisateur
    Utilisateur --> UC14

    %% Liens Technicien
    Technicien --> UC6
    Technicien --> UC7
    Technicien --> UC9
    Technicien --> UC10
    Technicien --> UC14

    %% Liens Capteurs et modules
    Capteurs --> UC2
    RTC --> UC3
    GPS --> UC2
    SD --> UC4
    SD --> UC10

    %% Relations entre cas d'utilisation
    UC1 --> UC2
    UC2 --> UC3
    UC3 --> UC4

    UC5 --> UC6
    UC5 --> UC7

    UC8 --> UC9
    UC8 --> UC10

    UC11 --> UC12

    %% Erreurs (cas transversal)
    UC2 --> UC13
    UC3 --> UC13
    UC4 --> UC13

    %% Lien entre les modes et les boutons poussoirs
    UC14 --> UC1
    UC14 --> UC5
    UC14 --> UC8
    UC14 --> UC11
```

```mermaid
flowchart TD

    Start([Démarrage du système])

    Start --> D0{Bouton rouge pressé au démarrage ?}
    D0 -- Oui --> Configuration
    D0 -- Non --> Standard

    Standard["Mode STANDARD activé"]

    Standard --> D1{Bouton vert 5s ?}
    D1 -- Oui --> Economique
    D1 -- Non --> D2{Bouton rouge 5s ?}
    D2 -- Oui --> Maintenance_S
    D2 -- Non --> Standard

    Configuration["Mode CONFIGURATION activé (Acquisition désactivée)"]

    Configuration --> D3{30 min sans activité ?}
    D3 -- Oui --> Standard
    D3 -- Non --> Configuration

    Economique["Mode ECONOMIQUE activé (Capteurs partiellement désactivés)"]

    Economique --> D4{Bouton rouge 5s ?}
    D4 -- Oui --> Standard
    D4 -- Non --> D5{Bouton rouge pressé ?}
    D5 -- Oui --> Maintenance_E
    D5 -- Non --> Economique

    Maintenance_S["Mode MAINTENANCE activé (depuis STANDARD)"]
    Maintenance_E["Mode MAINTENANCE activé (depuis ECONOMIQUE)"]

    Maintenance_S --> D6{Bouton rouge 5s ?}
    D6 -- Oui --> Standard
    D6 -- Non --> Maintenance_S

    Maintenance_E --> D7{Bouton rouge 5s ?}
    D7 -- Oui --> Economique
    D7 -- Non --> Maintenance_E
```
// -------------------------------------------------------------
// 📚 Bibliothèques nécessaires pour le projet
// -------------------------------------------------------------
#include "EEPROM.h"              // Stocke et récupère les paramètres de configuration
#include "RTClib.h"              // Manipule l'horloge temps réel (RTC) DS1307
#include "Wire.h"                // Communication I2C
#include "SoftwareSerial.h"      // Communication série avec un module GPS
#include "SD.h"                  // Lire et écrire les données sur une carte SD
#include "DHT.h"                 // Capteur DHT11 (température / humidité)
#include "Rgb_lcd.h"             // Écran LCD RGB
#include "ChainableLED.h"        // LED RGB
#include "BH1750.h"              // Capteur de luminosité


// -------------------------------------------------------------
// ⚙️ Constantes EEPROM
// -------------------------------------------------------------
ADDR_LOG_INTERVAL, ADDR_TEMP_LIMIT, ADDR_HUMIDITY_LIMIT, ADDR_LUMINOSITY_LIMIT

// Paramètres par défaut
DEFAULT_LOG_INTERVAL, DEFAULT_TEMP_LIMIT, DEFAULT_HUMIDITY_LIMIT, DEFAULT_LUMINOSITY_LIMIT


// -------------------------------------------------------------
// 🧩 Objets capteurs et modules
// -------------------------------------------------------------
rtc : RTC_DS1307
dht : DHT
SoftSerial : SoftwareSerial
lcd : Rgb_lcd
leds : ChainableLED
bh1750 : BH1750


// -------------------------------------------------------------
// 🔘 Boutons poussoirs
// -------------------------------------------------------------
volatile bool RBPushed, GBPushed
volatile unsigned long lastPushRB, lastPushGB


// -------------------------------------------------------------
// 🔄 Modes de fonctionnement
// -------------------------------------------------------------
enum Enum_Mod {STANDARD, CONFIG, MAINTENANCE, ECO}
Enum_Mod actualMod, lastMod


// -------------------------------------------------------------
// 🛠️ Paramètres de configuration
// -------------------------------------------------------------
ConfigParameters configParams


// -------------------------------------------------------------
// ⏱️ Variables liées au temps
// -------------------------------------------------------------
unsigned long lastActivity, lastMeasure
const unsigned long inactivityDuration = 30000 // 30 secondes


// -------------------------------------------------------------
// 💾 Variables liées au fichier
// -------------------------------------------------------------
File myFile
char nomDufichier[20]


// -------------------------------------------------------------
// 📺 Variables liées à l'écran LCD
// -------------------------------------------------------------
long lUpdate
int displayState


// -------------------------------------------------------------
// 📄 Création d’un nom de fichier unique
// -------------------------------------------------------------
fonction createNameFile(DateTime now, int revision)
    fichier = "data_" + now.getYear() + "_" + now.getMonth() + "_" +
              now.getDay() + "_" + now.getHour() + "_" +
              now.getMinute() + "_" + revision + ".txt"
    retourner fichier
FIN FONCTION


// -------------------------------------------------------------
// 📏 Vérifier la taille du fichier SD
// -------------------------------------------------------------
fonction verifySizeFile(DateTime now, int revision)
    si (tailleFichier > limite)
        nouveauFichier = createNameFile(now, revision + 1)
        retourner nouveauFichier
    sinon
        retourner fichierActuel
    fin si
FIN FONCTION


// -------------------------------------------------------------
// 💾 Écriture des données sur SD
// -------------------------------------------------------------
fonction writeSD(float temperature, float humidity, String lumens, String gpsData)
    écrire("Température: " + temperature +
           ", Humidité: " + humidity +
           ", Luminosité: " + lumens +
           ", GPS: " + gpsData) dans myFile
FIN FONCTION


// -------------------------------------------------------------
// ✔️ Vérifications capteurs
// -------------------------------------------------------------
fonction veriftemp(int i)
    si (lectureTemperature valide)
        retourner vrai
    sinon
        retourner faux
FIN FONCTION

fonction verifhum(int i)
    si (lectureHumidité valide)
        retourner vrai
    sinon
        retourner faux
FIN FONCTION

fonction veriflum(int i)
    si (lectureLuminosité valide)
        retourner vrai
    sinon
        retourner faux
FIN FONCTION


// -------------------------------------------------------------
// 📡 Collecte des données capteurs
// -------------------------------------------------------------
fonction collectLuminosity()
    sensorValue = lireValeurLuminosité()
    retourner sensorValue
FIN FONCTION

fonction collectTemperature()
    temperature = lireTemperature()
    si (temperature >= limiteMin && temperature <= limiteMax)
        retourner temperature
    sinon
        retourner erreur
FIN FONCTION

fonction collectHumidity()
    humidity = lireHumidité()
    si (humidity >= limiteMin && humidity <= limiteMax)
        retourner humidity
    sinon
        retourner erreur
FIN FONCTION

fonction readGPS()
    gpsData = lireGPS()
    retourner gpsData
FIN FONCTION


// -------------------------------------------------------------
// 🧪 Initialisation des capteurs
// -------------------------------------------------------------
fonction initSensors()
    initialiserCapteurDHT()
    initialiserModuleGPS()
FIN FONCTION


// -------------------------------------------------------------
// 🔄 Changement de mode
// -------------------------------------------------------------
fonction changeMode(Mod newMod)
    actualMod = newMod
FIN FONCTION


// -------------------------------------------------------------
// 📥 Collecte + écriture SD
// -------------------------------------------------------------
fonction collectData(long timeIntervalle)
    si (currentTime - lastMeasure >= timeIntervalle)
        lum = collectLuminosity()
        gpsData = readGPS()
        temperature = collectTemperature()
        humidity = collectHumidity()
        writeSD(temperature, humidity, lum, gpsData)
        lastMeasure = currentTime
    fin si
FIN FONCTION


// -------------------------------------------------------------
// 🔘 Interruptions boutons
// -------------------------------------------------------------
fonction interruptRB()
    // Action bouton RB
FIN FONCTION

fonction interruptGB()
    // Action bouton GB
FIN FONCTION


// -------------------------------------------------------------
// ⏳ Détection appui long
// -------------------------------------------------------------
fonction longPushButton(volatile unsigned long& lastPush, volatile bool& pushButton)
    si (currentTime - lastPush >= 5000)
        pushButton = vrai
    sinon
        pushButton = faux
FIN FONCTION


// -------------------------------------------------------------
// 🛠️ Affichage maintenance
// -------------------------------------------------------------
fonction displayMaintenance()
    // Affichage LCD
FIN FONCTION


// -------------------------------------------------------------
// ⚙️ Configuration système
// -------------------------------------------------------------
fonction configParam(String command, int hour, int minute, int second,
                     int month, int day, int year, int dayOfWeek)
    // Mise à jour EEPROM
FIN FONCTION

fonction resetDefaults()
    // Réinitialisation EEPROM
FIN FONCTION


// -------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------
fonction setup()
    initial

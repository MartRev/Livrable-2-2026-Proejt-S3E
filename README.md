# 🌦️ Station Météo – Projet Arduino / Grove

## Présentation du projet
L'Agence Internationale pour la Vigilance Météorologique (AIVM) se lance dans un projet ambitieux : déployer dans les océans des navires de surveillance équipés de stations météo embarquées chargées de mesurer les paramètres influant sur la formation de cyclones ou autres catastrophes naturelles.

Un grand nombre de sociétés utilisant des transports navals ont accepté d'équiper leurs bateaux avec ces stations embarquées. En revanche, ces dernières devront être simples et efficaces et pilotables par un des membres de l'équipage (une documentation technique utilisateur sera mise à disposition).



---

## Objectifs
- Acquérir et horodater des données environnementales
- Enregistrer les données sur carte SD
- Permettre la configuration via interface série
- Assurer un fonctionnement robuste et autonome

---

## Structure générale du programme : 
```mermaid
flowchart TD

    subgraph System["Architecture Fonctionnelle"]
        A[Lecture capteurs] --> B[Horodatage RTC]
        B --> C[Enregistrement SD]
        A --> D[GPS]
        D --> C

        E[Interface Série] --> F[Mode Configuration]
        G[Boutons poussoirs] --> H[Machine à états]
        H --> A
        H --> F
        H --> I[Mode Maintenance]
        H --> J[Mode Économique]
    end
```
<1> Déclarations globales
```ccp
// -------------------------------------------------------------
// Bibliothèques nécessaires
// -------------------------------------------------------------
#include <EEPROM.h>          // Gestion des paramètres sauvegardés
#include <RTClib.h>          // Horloge temps réel (RTC)
#include <Wire.h>            // Bus I2C
#include <SoftwareSerial.h>  // GPS via port série logiciel
#include <SD.h>              // Carte SD
#include <DHT.h>             // Capteur température / humidité
#include <Rgb_lcd.h>         // Écran LCD RGB
#include <ChainableLED.h>    // LED RGB
#include <BH1750.h>          // Capteur de luminosité

// -------------------------------------------------------------
// Modes de fonctionnement
// -------------------------------------------------------------
enum Mode { STANDARD, CONFIG, MAINTENANCE, ECO };
Mode actualMod, lastMod;

// -------------------------------------------------------------
// Gestion des capteurs
// -------------------------------------------------------------
const int NB_CAPTEURS = 3;   // Nombre total de capteurs: température, humidité, luminosité 
const int NB_VAL = 10;       // Taille de la moyenne glissante

struct Capteur {
    float moy_gliss[NB_VAL]; // Tableau pour moyenne glissante
    int nb_erreur;           // Compteur d’erreurs successives
};

Capteur capteurs[NB_CAPTEURS];
int ind_moy = 0;             // Index circulaire pour la moyenne glissante

// -------------------------------------------------------------
// Paramètres système (EEPROM)
// -------------------------------------------------------------
struct ConfigParams {
    int LOG_INTERVAL;        // Intervalle entre mesures
    int FILE_MAX_SIZE;       // Taille max fichier SD
    int TIMEOUT;             // Timeout capteurs
    int LUMIN_LOW;           // Seuil luminosité faible
    int LUMIN_HIGH;          // Seuil luminosité forte
    int MIN_TEMP_AIR;        // Température min valide
    int MAX_TEMP_AIR;        // Température max valide
};
ConfigParams config;

// -------------------------------------------------------------
// Gestion des fichiers SD
// -------------------------------------------------------------
File myFile;
char nomFichier[20];

// -------------------------------------------------------------
// Prototype du sous-programme de vérification des limites
// -------------------------------------------------------------
String checkLimits(float value, float minVal, float maxVal);

```
<2> Initialisation (setup)
```ccp
// -------------------------------------------------------------
// Initialisation du système
// -------------------------------------------------------------
void setup() {

    initLED();              // LED RGB (état du système)
    initButtons();          // Boutons poussoirs
    initSensors();          // DHT, BH1750, etc.
    initGPS();              // GPS via SoftwareSerial
    initRTC();              // Horloge RTC
    initSD();               // Carte SD
    loadConfigEEPROM();     // Chargement des paramètres utilisateur

    actualMod = STANDARD;   // Mode par défaut
    updateLED(actualMod);   // Mise à jour de la LED selon le mode
}
```
<3> Changement de modes (loop)
```ccp
// -------------------------------------------------------------
// Boucle principale : machine à états
// -------------------------------------------------------------
void loop() {

    handleButtons();  // Détection des appuis courts / longs

    switch(actualMod) {

        case STANDARD:
            collectData(config.LOG_INTERVAL);
            break;

        case CONFIG:
            processSerialCommands();
            break;

        case MAINTENANCE:
            displayMaintenance();
            break;

        case ECO:
            collectData(config.LOG_INTERVAL * 2);
            break;
    }
}
```
<4> Lecture des capteurs (avec pointeurs)
```ccp
// -------------------------------------------------------------
// Lecture générique des capteurs via pointeurs
// -------------------------------------------------------------
```ccp
void Lecture(float* tab_val, int* erreurs) {

    float mesure;
    bool erreur;

    for (int i = 0; i < NB_CAPTEURS; i++) {

        mesure = 0;

        erreur = Lecture_capteur(&mesure, i);

        if (erreur) {
            erreurs[i]++;
        } else {
            Add_Val(tab_val, mesure);
        }
    }
}
```
<5> Moyenne glissante
```ccp
// -------------------------------------------------------------
// Ajout d’une valeur dans la moyenne glissante
// -------------------------------------------------------------
void Add_Val(float* tab_moy, float val) {

    tab_moy[ind_moy] = val;

    if (ind_moy >= NB_VAL - 1)
        ind_moy = 0;
    else
        ind_moy++;
}
```
<6> Ajout du sous‑programme des limites des capteurs : checkLimits()
```ccp
// -------------------------------------------------------------
// Vérification des limites d’un capteur
// -------------------------------------------------------------
String checkLimits(float value, float minVal, float maxVal) {

    if (isnan(value)) {
        return "NA";   // Capteur non répondant
    }

    if (value < minVal || value > maxVal) {
        return "Valeur Hors-Limite";
    }

    return String(value);
}
```
<7> Collecte + enregistrement SD
```ccp
// -------------------------------------------------------------
// Collecte des données + écriture sur SD
// -------------------------------------------------------------
void collectData(int interval) {

    if (millis() - lastMeasure >= interval) {

        float temp = collectTemperature(); // Température
        float hum  = collectHumidity();    // Humidité
        int lum    = collectLuminosity();  // Luminosité
        String gps = getGPSData();         // Données GPS

        // Vérification des limites
        String tempStr = checkLimits(temp, config.MIN_TEMP_AIR, config.MAX_TEMP_AIR);
        String humStr  = checkLimits(hum, 0, 100);
        String lumStr  = checkLimits(lum, config.LUMIN_LOW, config.LUMIN_HIGH);

        // Écriture sur SD
        writeSD(tempStr, humStr, lumStr, gps);

        lastMeasure = millis();
    }
}
```

```mermaid
stateDiagram-v2

    [*] --> CONFIG : BTN_BLUE == PRESSED_AT_BOOT
    %% Si le bouton bleu est pressé démarrage du mode configuration

    state CONFIG {

        [*] --> INIT_CONFIG

        %% ============================
        %% INITIALISATION DU MODE
        %% ============================

        INIT_CONFIG : MODE = CONFIG
        INIT_CONFIG : DisableSensors()
        INIT_CONFIG : INACTIVITY_TIMER = 0
        %% On désactive l'acquisition
        %% On initialise le timer d'inactivité

        INIT_CONFIG --> WAIT_CMD

        %% ============================
        %% ATTENTE DE COMMANDE UART
        %% ============================

        WAIT_CMD : UART_CMD = ReadUART()
        %% Lecture d'une commande depuis la console série

        WAIT_CMD --> PROCESS_CMD : UART_CMD != 0
        %% Si une commande est reçue

        WAIT_CMD --> CHECK_TIMEOUT : UART_CMD == 0
        %% Sinon, on vérifie le timer d'inactivité

        %% ============================
        %% TRAITEMENT DES COMMANDES
        %% ============================

        PROCESS_CMD : Update(EEPROM_PARAM)
        PROCESS_CMD : Reset(INACTIVITY_TIMER)
        %% Mise à jour des paramètres système
        %% Remise à zéro du timer

        PROCESS_CMD --> WAIT_CMD
        %% Retour en attente d'une nouvelle commande

        %% ============================
        %% GESTION DU TIMEOUT
        %% ============================

        CHECK_TIMEOUT --> STANDARD : INACTIVITY_TIMER >= 30min
        %% Si aucune activité pendant 30 minutes,
        %% retour automatique au mode standard

        CHECK_TIMEOUT --> WAIT_CMD : ELSE
        %% Sinon, on continue à attendre une commande
    }
```
```mermaid
stateDiagram-v2

    [*] --> MAINTENANCE

    state MAINTENANCE {

        [*] --> INIT_MAINT

        INIT_MAINT : MODE = MAINTENANCE
        INIT_MAINT : SD_WRITE_ENABLE = FALSE
        INIT_MAINT : EnableUART()

        INIT_MAINT --> RUN_MAINT

        RUN_MAINT : SENSOR_DATA = ReadSensors()
        RUN_MAINT : UART_CMD = ReadUART()

        RUN_MAINT --> SEND_DATA : UART_CMD == "READ"
        RUN_MAINT --> CHECK_EXIT : BTN_RED == LONG_PRESS
        RUN_MAINT --> RUN_MAINT : ELSE

        SEND_DATA : SendToUART(SENSOR_DATA)
        SEND_DATA --> RUN_MAINT

        CHECK_EXIT --> RETURN_MODE
    }

    RETURN_MODE --> STANDARD : PREVIOUS_MODE == STANDARD
    RETURN_MODE --> ECONOMIE : PREVIOUS_MODE == ECONOMIE
```
```mermaid
flowchart TD
    %% on initialise 
    START([START])

    %% on rentre en mode standard
    START --> MODE[MODE = STANDARD]

    %% Initialisation des capteurs
    MODE --> INIT[INIT_SENSORS]

    %% Indication avec la led du fonctionnement normal
    INIT --> LED[LED = GREEN]

    %% Pour la BOUCLE PRINCIPALE
    %% Déclenchement périodique selon LOG_INTERVAL de 10min
    LED --> LOOP{{t >= LOG_INTERVAL}}

    %% Lecture du capteur i
    LOOP --> READ[READ_SENSOR_i]

    %% on vérifie le temps de réponse
    READ --> RESP{RESP_TIME < TIMEOUT}

    %% Si le capteur ne répond pas
    RESP -- no --> SET_NA[VALUE_i = NA]

    %% Si le capteur répond bien
    RESP -- yes --> SET_VAL[VALUE_i = DATA]

    %% on passe au capteur suivant
    SET_NA --> NEXT[I = I + 1]
    SET_VAL --> NEXT

    %% Vérification s’il reste des capteurs à lire
    NEXT --> CHECK_I{I < NB_SENSORS}

    %% on continue la lecture des capteurs
    CHECK_I -- yes --> READ

    %% tous les capteurs ont été lus
    CHECK_I -- no --> LINE[BUILD_LINE TIME VALUE_ARRAY]

    %% ÉCRITURE SUR CARTE SD
    %% Écriture dans le fichier de révision 0
    LINE --> WRITE[WRITE_SD FILE_0]

    %% Vérification si il y a des erreur d’écriture
    WRITE --> ERR_SD{SD_ERROR}

    %% En cas d’erreur SD
    ERR_SD -- yes --> STOP[SD_WRITE = FALSE]
    STOP --> ERR_MSG[UI = SD_ERROR]

    %% Si pas d’erreur SD
    ERR_SD -- no --> SIZE{FILE_SIZE > FILE_MAX_SIZE}

    %% Si le fichier dépasse la taille max
    SIZE -- yes --> ROTATE[ROTATE_FILE]
    ROTATE --> RESET[CLEAR_FILE_0]

    %% Sinon on continue l’écriture normale
    SIZE -- no --> CONTINUE

    RESET --> LOOP
    CONTINUE --> LOOP

    %%  CHANGEMENT DE MODE
    %% on regarde le bouton de mode
    LOOP --> BTN{MODE_BTN}

    %% Passage en mode économique
    BTN -- ECO --> ECO[MODE = ECO]

    %% Passage en mode maintenance
    BTN -- MAINT --> MAINT[MODE = MAINT]

    %% Pas de changement de mode
    BTN -- NONE --> LOOP
```
```mermaid
stateDiagram-v2

    [*] --> ECONOMIE

    state ECONOMIE {

        [*] --> INIT_ECO

        INIT_ECO : MODE = ECONOMIE
        INIT_ECO : LOG_INTERVAL = BASE_LOG_INTERVAL * 2
        INIT_ECO : GPS_SAMPLE_COUNT = 0

        INIT_ECO --> RUN_ECO

        RUN_ECO : SENSOR_DATA = ReadSensors()
        RUN_ECO : GPS_SAMPLE_COUNT++

        RUN_ECO --> READ_GPS : GPS_SAMPLE_COUNT % 2 == 0
        RUN_ECO --> SKIP_GPS : ELSE

        READ_GPS : ReadGPS()
        READ_GPS --> STORE_DATA

        SKIP_GPS --> STORE_DATA

        STORE_DATA : StoreData(LOG_INTERVAL)

        STORE_DATA --> CHECK_EXIT

        CHECK_EXIT --> STANDARD : BTN_RED == LONG_PRESS
        CHECK_EXIT --> RUN_ECO : ELSE
    }
```

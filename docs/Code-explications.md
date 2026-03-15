```md
# Station Météo Arduino – Documentation Technique Résumée

Ce document présente le fonctionnement du programme Arduino de la station météo.  
Le code est découpé en sections, chacune suivie d’une explication technique concise.

---

# Table des matières

- [1. Bibliothèques](#1-bibliothèques)
- [2. Constantes et broches](#2-constantes-et-broches)
- [3. Objets matériels](#3-objets-matériels)
- [4. Paramètres et seuils](#4-paramètres-et-seuils)
- [5. États et modes](#5-états-et-modes)
- [6. Flags et boutons](#6-flags-et-boutons)
- [7. Textes PROGMEM](#7-textes-progmem)
- [8. Version et fichiers SD](#8-version-et-fichiers-sd)
- [9. Prototypes](#9-prototypes)
- [10. Interruption Timer1](#10-interruption-timer1)
- [11. Configuration Timer1](#11-configuration-timer1)
- [12. Setup](#12-setup)
- [13. Loop](#13-loop)
- [14. Configuration série](#14-configuration-série)
- [15. LCD](#15-lcd)
- [16. Modes](#16-modes)
- [17. Heure & GPS](#17-heure--gps)
- [18. Capteurs](#18-capteurs)
- [19. Carte SD](#19-carte-sd)
- [20. LED & erreurs](#20-led--erreurs)
- [21. Test RTC](#21-test-rtc)
- [22. Boutons](#22-boutons)
- [23. Résumé EEPROM / Interruptions / PROGMEM](#23-résumé-eeprom--interruptions--progmem)

---

# 1. Bibliothèques

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <ChainableLED.h>
#include "rgb_lcd.h"
#include <SoftwareSerial.h>
#include <DHT.h>
#include <TinyGPS.h>
#include <SD.h>
#include <avr/interrupt.h>
#include <EEPROM.h>
```

### Explication  
- `Wire` : bus I2C (RTC + LCD)  
- `RTClib` : gestion de l’horloge DS1307  
- `ChainableLED` : LED RGB  
- `SoftwareSerial` : GPS sur port série logiciel  
- `DHT` : capteur température/humidité  
- `TinyGPS` : décodage trames GPS  
- `SD` : gestion carte SD  
- `interrupt.h` : interruptions matérielles  
- `EEPROM` : stockage persistant

---

# 2. Constantes et broches

```cpp
#define BUTTON_RED 7
#define BUTTON_GREEN 6
#define LED_DATA_PIN 4
#define LED_CLOCK_PIN 5
#define GPS_RX 8
#define GPS_TX 3
#define DHTPIN 2
#define LIGHT_SENSOR A0
#define SD_CS 10
#define DEBOUNCE_MS 200
#define DEFAULT_LOG_INTERVAL 5
#define EEPROM_ADDR_LOG_INTERVAL 0
```

### Explication  
Définit le câblage matériel et les paramètres globaux (anti‑rebond, intervalle par défaut, adresse EEPROM).

---

# 3. Objets matériels

```cpp
ChainableLED leds(LED_DATA_PIN, LED_CLOCK_PIN, 1);
rgb_lcd lcd;
RTC_DS1307 rtc;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
DHT dht(DHTPIN, DHT11);
TinyGPS gps;
File dataFile;
```

### Explication  
Chaque objet représente un module physique (LED, LCD, GPS, DHT, SD).

---

# 4. Paramètres et seuils

```cpp
unsigned int LOG_INTERVAL = DEFAULT_LOG_INTERVAL;

int MIN_TEMP_AIR = -5, MAX_TEMP_AIR = 30;
int HYGR_MIN = 0, HYGR_MAX = 100;
int MIN_LUMIN = 0, MAX_LUMIN = 1023;
```

### Explication  
Seuils de validité des capteurs.  
`LOG_INTERVAL` peut être modifié via le port série et sauvegardé en EEPROM.

---

# 5. États et modes

```cpp
bool sdOK = true, rtcOK = true, rtcMessageShown = false, sdFull = false;

enum Mode { DEMARRAGE, CONFIGURATION, STANDARD, ECONOMIQUE, MAINTENANCE };
volatile Mode modeActuel = DEMARRAGE;
```

### Explication  
Machine à états gérant le comportement global du système.

---

# 6. Flags et boutons

```cpp
volatile bool flagHorloge = false;
volatile bool flagCapteurs = false;

unsigned long lastRedPress = 0, lastGreenPress = 0;
bool redPressed = false, greenPressed = false;
```

### Explication  
Les flags sont mis à jour par une interruption.  
Les timestamps servent à l’anti‑rebond logiciel.

---

# 7. Textes PROGMEM

```cpp
const char STR_DEMARRAGE[] PROGMEM = "Mode Demarrage";
const char STR_CONFIG[] PROGMEM = "Mode Config";
const char STR_STANDARD[] PROGMEM = "Mode Standard";
...
const char STR_SD_PLEINE[] PROGMEM = "SD pleine !";
```

### Explication  
Stockés en **mémoire flash** pour économiser la RAM.  
Utilise des **pointeurs PROGMEM**.

---

# 8. Version et fichiers SD

```cpp
const char PROGRAM_VERSION[] PROGMEM = "1.0.0";
const char PROGRAM_LOT[] PROGMEM = "26030101";
const char FICHIER_CSV[] PROGMEM = "donnees.csv";
const uint32_t SD_MAX_SIZE = 2684354560UL;
```

### Explication  
Informations version + limite de taille du fichier CSV.

---

# 9. Prototypes

```cpp
void configTimer1();
void changerMode(Mode m,uint8_t r,uint8_t g,uint8_t b,const char* texte);
void lireCapteurs();
void afficherHeure();
void gererBoutons();
void afficherTexteLCD(const char* textePROGMEM);
void enregistrerDonnees(...);
bool ouvrirSD();
bool sdPleine();
void clignoterErreur(...);
void clignoterErreurIncoherence();
bool testerRTC();
void reafficherMode();
void clignoterGPS();
void gererConfigurationSerie();
```

### Explication  
Déclare les fonctions avant leur utilisation.

---

# 10. Interruption Timer1

```cpp
ISR(TIMER1_COMPA_vect)
{
    flagHorloge = true;
    flagCapteurs = true;
}
```

### Explication  
Interruption déclenchée toutes les secondes.  
Met à jour les flags utilisés dans `loop()`.

**Zone clé : interruptions matérielles**

---

# 11. Configuration Timer1

```cpp
void configTimer1()
{
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = 15624;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS12) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
    interrupts();
}
```

### Explication  
- Mode CTC (Clear Timer on Compare Match)  
- Prescaler 1024  
- Interruption toutes les 1 s  
- Active l’ISR TIMER1_COMPA_vect

---

# 12. Setup

```cpp
void setup()
{
    Serial.begin(9600);
    pinMode(BUTTON_RED, INPUT_PULLUP);
    pinMode(BUTTON_GREEN, INPUT_PULLUP);

    gpsSerial.begin(9600);
    dht.begin();
    Wire.begin();

    lcd.begin(16, 2);
    lcd.setRGB(255, 255, 255);
    afficherTexteLCD(STR_INIT);
    delay(2000);

    sdOK = SD.begin(SD_CS);

    if (!rtc.begin())
    {
        rtcOK = false;
        afficherTexteLCD(STR_RTC_ERROR);
        rtcMessageShown = true;
        delay(2000);
    }

    EEPROM.get(EEPROM_ADDR_LOG_INTERVAL, LOG_INTERVAL);
    if (LOG_INTERVAL == 0 || LOG_INTERVAL > 1440)
    {
        LOG_INTERVAL = DEFAULT_LOG_INTERVAL;
        EEPROM.put(EEPROM_ADDR_LOG_INTERVAL, LOG_INTERVAL);
    }

    configTimer1();
    changerMode(DEMARRAGE, 255, 255, 255, STR_DEMARRAGE);
}
```

### Explication  
Initialise tous les modules.  
Lit `LOG_INTERVAL` depuis l’EEPROM.  
Configure Timer1.  
Affiche le mode démarrage.

**Zone clé : EEPROM (lecture/écriture)**

---

# 13. Loop

```cpp
void loop()
{
    gererBoutons();
    gererConfigurationSerie();

    while (gpsSerial.available())
        gps.encode(gpsSerial.read());

    if (!sdOK) { clignoterErreur(...); return; }
    if (!rtcOK) { ... }
    if (sdFull) { ... }

    if (flagHorloge) { flagHorloge = false; afficherHeure(); }
    if (flagCapteurs) { flagCapteurs = false; lireCapteurs(); }
}
```

### Explication  
Boucle principale orchestrant :

- boutons  
- configuration série  
- décodage GPS  
- gestion erreurs  
- mise à jour heure  
- lecture capteurs  

---

# 14. Configuration série

```cpp
void gererConfigurationSerie()
{
    if (modeActuel != CONFIGURATION) return;
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    int pos = cmd.indexOf('=');
    int value = (pos != -1) ? cmd.substring(pos + 1).toInt() : 0;

    if (cmd.startsWith("LOG_INTERVAL="))
    {
        if (value > 0 && value <= 1440)
        {
            LOG_INTERVAL = value;
            EEPROM.put(EEPROM_ADDR_LOG_INTERVAL, LOG_INTERVAL);
            Serial.print(F("LOG_INTERVAL="));
            Serial.println(LOG_INTERVAL);
        }
    }
    ...
}
```

### Explication  
Permet de modifier les paramètres via le port série.  
Sauvegarde `LOG_INTERVAL` dans l’EEPROM.

**Zone clé : EEPROM (écriture)**

---

# 15. LCD

```cpp
void afficherTexteLCD(const char* textePROGMEM)
{
    lcd.clear();
    lcd.print((__FlashStringHelper*)textePROGMEM);
}
```

### Explication  
Affiche un texte stocké en PROGMEM.

**Zone clé : pointeurs PROGMEM**

---

# 16. Modes

```cpp
void changerMode(Mode m, uint8_t r, uint8_t g, uint8_t b, const char* texte)
{
    if (m == STANDARD || m == ECONOMIQUE)
    {
        if (!ouvrirSD()) return;
    }
    else fermerSD();

    modeActuel = m;
    leds.setColorRGB(0, r, g, b);
    afficherTexteLCD(texte);
}
```

### Explication  
Change le mode, la LED et le texte LCD.  
Ouvre/ferme la SD selon le mode.

---

# 17. Heure & GPS

```cpp
void afficherHeure()
{
    DateTime now = rtc.now();
    if (now.year() < 2020) { rtcOK = false; return; }

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());

    lcd.setCursor(0, 1);
    lcd.print(buffer);

    float lat, lon;
    gps.f_get_position(&lat, &lon);

    lcd.setCursor(10, 1);
    if (lat != TinyGPS::GPS_INVALID_F_ANGLE) lcd.print(F("GPS"));
    else { lcd.print(F("...")); clignoterGPS(); }
}
```

### Explication  
Affiche l’heure + état GPS.  
Détecte RTC invalide.

---

# 18. Capteurs

```cpp
void lireCapteurs()
{
    if (modeActuel != STANDARD && modeActuel != ECONOMIQUE) return;

    static unsigned long lastSDWrite = 0;
    unsigned long nowMs = millis();
    unsigned long interval = (modeActuel == STANDARD)
        ? LOG_INTERVAL * 1000UL
        : LOG_INTERVAL * 2000UL;

    if (nowMs - lastSDWrite < interval) return;
    lastSDWrite = nowMs;

    float temperature = dht.readTemperature();
    float humidite = dht.readHumidity();
    int luminosite = analogRead(LIGHT_SENSOR);

    bool erreurDHT = isnan(temperature) || isnan(humidite);
    bool erreurTemp = (temperature < MIN_TEMP_AIR || temperature > MAX_TEMP_AIR);
    bool erreurHum  = (humidite < HYGR_MIN || humidite > HYGR_MAX);
    bool erreurLum1 = (luminosite == 0 || luminosite == 1023);
    bool erreurLum2 = (luminosite < MIN_LUMIN || luminosite > MAX_LUMIN);

    if (erreurDHT || erreurTemp || erreurHum || erreurLum1 || erreurLum2)
    {
        afficherTexteLCD(STR_ERR_CAPTEUR);
        clignoterErreurIncoherence();
        return;
    }

    float lat, lon;
    gps.f_get_position(&lat, &lon);
    bool gpsOK = !(lat == TinyGPS::GPS_INVALID_F_ANGLE);

    DateTime now = rtc.now();

    if (sdPleine()) { sdFull = true; return; }

    enregistrerDonnees(temperature, humidite, luminosite,
                       gpsOK, lat, lon,
                       now.year(), now.month(), now.day(),
                       now.hour(), now.minute(), now.second());
}
```

### Explication  
Lit les capteurs, valide les valeurs, lit GPS + RTC, enregistre sur SD.

---

# 19. Carte SD

```cpp
bool sdPleine()
{
    File f = SD.open((__FlashStringHelper*)FICHIER_CSV, FILE_READ);
    if (!f) return false;
    uint32_t taille = f.size();
    f.close();
    return (taille >= SD_MAX_SIZE);
}
```

```cpp
void enregistrerDonnees(...)
{
    dataFile = SD.open((__FlashStringHelper*)FICHIER_CSV, FILE_WRITE);
    if (!dataFile) return;

    dataFile.print(annee); dataFile.print("-");
    ...
    dataFile.println();
    dataFile.close();
}
```

### Explication  
- Vérifie si le fichier dépasse la limite  
- Écrit une ligne CSV complète

---

# 20. LED & erreurs

```cpp
void clignoterErreur(...)
void clignoterErreurIncoherence()
void clignoterGPS()
```

### Explication  
Chaque erreur a un pattern de clignotement spécifique.

---

# 21. Test RTC

```cpp
bool testerRTC()
{
    Wire.beginTransmission(0x68);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;

    Wire.requestFrom(0x68, 1);
    if (!Wire.available()) return false;

    uint8_t sec = Wire.read();
    return !(sec == 0xFF || (sec & 0x7F) > 59);
}
```

### Explication  
Vérifie que la RTC renvoie une valeur cohérente.

---

# 22. Boutons

```cpp
void gererBoutons()
{
    if (sdFull) return;

    bool redState = !digitalRead(BUTTON_RED);
    bool greenState = !digitalRead(BUTTON_GREEN);

    unsigned long now = millis();

    if (redState && !redPressed && now - lastRedPress > DEBOUNCE_MS)
    {
        redPressed = true;
        lastRedPress = now;

        if (modeActuel == DEMARRAGE) changerMode(CONFIGURATION, 255, 255, 0, STR_CONFIG);
        else if (modeActuel == CONFIGURATION) changerMode(STANDARD, 0, 255, 0, STR_STANDARD);
        else if (modeActuel == STANDARD) changerMode(MAINTENANCE, 255, 165, 0, STR_MAINT);
        else if (modeActuel == MAINTENANCE) changerMode(STANDARD, 0, 255, 0, STR_STANDARD);
        else if (modeActuel == ECONOMIQUE) changerMode(MAINTENANCE, 255, 165, 0, STR_MAINT);
    }

    if (!redState) redPressed = false;

    if (greenState && !greenPressed && now - lastGreenPress > DEBOUNCE_MS)
    {
        greenPressed = true;
        lastGreenPress = now;

        if (modeActuel == DEMARRAGE) changerMode(CONFIGURATION, 255, 255, 0, STR_CONFIG);
        else if (modeActuel == CONFIGURATION) changerMode(STANDARD, 0, 255, 0, STR_STANDARD);
        else if (modeActuel == STANDARD) changerMode(ECONOMIQUE, 0, 0, 255, STR_ECO);
        else if (modeActuel == ECONOMIQUE) changerMode(STANDARD, 0, 255, 0, STR_STANDARD);
        else if (modeActuel == MAINTENANCE) changerMode(ECONOMIQUE, 0, 0, 255, STR_ECO);
    }

    if (!greenState) greenPressed = false;
}
```

### Explication  
Gère les transitions entre modes via les boutons.

---

# 23. Résumé EEPROM / Interruptions / PROGMEM

```md
EEPROM utilisée dans :
- setup() → lecture LOG_INTERVAL
- gererConfigurationSerie() → écriture LOG_INTERVAL

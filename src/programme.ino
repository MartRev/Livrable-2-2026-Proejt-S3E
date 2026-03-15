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

// === CONSTANTES ===
#define BUTTON_RED 7
#define BUTTON_GREEN 6
#define LED_DATA_PIN 4
#define LED_CLOCK_PIN 5
#define GPS_RX 8
#define GPS_TX 3
#define DHTPIN 2
#define DHTTYPE DHT11
#define LIGHT_SENSOR A0
#define SD_CS 10
#define DEBOUNCE_MS 200
#define DEFAULT_LOG_INTERVAL 5

// EEPROM
#define EEPROM_ADDR_LOG_INTERVAL 0

// === OBJETS ===
ChainableLED leds(LED_DATA_PIN, LED_CLOCK_PIN, 1);
rgb_lcd lcd;
RTC_DS1307 rtc;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
DHT dht(DHTPIN, DHTTYPE);
TinyGPS gps;

File dataFile;

// === PARAMETRES ===
unsigned int LOG_INTERVAL = DEFAULT_LOG_INTERVAL;

// === SEUILS CAPTEURS ===
int MIN_TEMP_AIR = -5;
int MAX_TEMP_AIR = 30;

int HYGR_MIN = 0;
int HYGR_MAX = 100;

int MIN_LUMIN = 0;
int MAX_LUMIN = 1023;

// === ETATS ===
bool sdOK = true;
bool rtcOK = true;
bool rtcMessageShown = false;
bool sdFull = false;

// === MODES ===
enum Mode { DEMARRAGE, CONFIGURATION, STANDARD, ECONOMIQUE, MAINTENANCE };
volatile Mode modeActuel = DEMARRAGE;

// === FLAGS ===
volatile bool flagHorloge = false;
volatile bool flagCapteurs = false;

// === BOUTONS ===
unsigned long lastRedPress = 0;
unsigned long lastGreenPress = 0;
bool redPressed = false;
bool greenPressed = false;

// === TEXTES PROGMEM ===
const char STR_DEMARRAGE[]     PROGMEM = "Mode Demarrage";
const char STR_CONFIG[]        PROGMEM = "Mode Config";
const char STR_STANDARD[]      PROGMEM = "Mode Standard";
const char STR_ECO[]           PROGMEM = "Mode Eco";
const char STR_MAINT[]         PROGMEM = "Maintenance";
const char STR_INIT[]          PROGMEM = "Initialisation";
const char STR_RTC_ERROR[]     PROGMEM = "RTC erreur";
const char STR_ERR_CAPTEUR[]   PROGMEM = "erreur capteur";
const char STR_SD_ABSENTE[]    PROGMEM = "SD absente !";
const char STR_SD_PLEINE[]     PROGMEM = "SD pleine !";

// === VERSION ===
const char PROGRAM_VERSION[] PROGMEM = "1.0.0";
const char PROGRAM_LOT[]     PROGMEM = "26030101";

// === FICHIERS SD ===
const char FICHIER_CSV[] PROGMEM = "donnees.csv";

// === LIMITE SD ===
const uint32_t SD_MAX_SIZE = 2684354560UL;

// === PROTOTYPES ===
void configTimer1();
void changerMode(Mode m,uint8_t r,uint8_t g,uint8_t b,const char* texte);
void lireCapteurs();
void afficherHeure();
void gererBoutons();
void afficherTexteLCD(const char* textePROGMEM);
void enregistrerDonnees(float t,float h,int lum,bool gpsOK,float lat,float lon,
                        int annee,int mois,int jour,int heure,int minute,int seconde);
void fermerSD();
bool ouvrirSD();
bool sdPleine();
void clignoterErreur(uint8_t r1,uint8_t g1,uint8_t b1,
                     uint8_t r2,uint8_t g2,uint8_t b2,
                     unsigned long interval);
void clignoterErreurIncoherence();
bool testerRTC();
void reafficherMode();
void clignoterGPS();
void gererConfigurationSerie();

// === INTERRUPTIONS ===
ISR(TIMER1_COMPA_vect)
{
    flagHorloge = true;
    flagCapteurs = true;
}

// === TIMER ===
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

// === SETUP ===
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

// === LOOP ===
void loop()
{
    gererBoutons();
    gererConfigurationSerie();

    // === DECODAGE GPS ===
    while (gpsSerial.available())
        gps.encode(gpsSerial.read());

    if (!sdOK)
    {
        clignoterErreur(255, 255, 255, 255, 0, 0, 500);
        return;
    }

    if (!rtcOK)
    {
        if (!rtcMessageShown)
        {
            afficherTexteLCD(STR_RTC_ERROR);
            rtcMessageShown = true;
        }

        static unsigned long lastCheck = 0;

        if (millis() - lastCheck > 1000)
        {
            lastCheck = millis();
            rtcOK = testerRTC();

            if (rtcOK)
            {
                rtcMessageShown = false;
                reafficherMode();
            }
        }

        if (!rtcOK)
        {
            clignoterErreur(0, 0, 255, 255, 0, 0, 500);
            return;
        }
    }

    if (sdFull)
    {
        afficherTexteLCD(STR_SD_PLEINE);
        clignoterErreur(255, 0, 0, 255, 255, 255, 500);
        return;
    }

    if (flagHorloge)
    {
        flagHorloge = false;
        afficherHeure();
    }

    if (flagCapteurs)
    {
        flagCapteurs = false;
        lireCapteurs();
    }
}

// === CONFIGURATION SERIE ===
void gererConfigurationSerie()
{
    if (modeActuel != CONFIGURATION) return;
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // Récupération robuste de la valeur après '='
    int pos = cmd.indexOf('=');
    int value = 0;
    if (pos != -1)
        value = cmd.substring(pos + 1).toInt();

    // --- LOG_INTERVAL ---
    if (cmd.startsWith("LOG_INTERVAL="))
    {
        if (value > 0 && value <= 1440)
        {
            LOG_INTERVAL = value;
            EEPROM.put(EEPROM_ADDR_LOG_INTERVAL, LOG_INTERVAL);

            Serial.print(F("LOG_INTERVAL="));
            Serial.print(LOG_INTERVAL);
            Serial.println(F(" secondes"));
        }
        else
            Serial.println(F("Valeur invalide"));
    }

    // --- MIN_TEMP_AIR ---
    else if (cmd.startsWith("MIN_TEMP_AIR="))
    {
        MIN_TEMP_AIR = value;
        Serial.print(F("MIN_TEMP_AIR="));
        Serial.println(MIN_TEMP_AIR);
    }

    // --- MAX_TEMP_AIR ---
    else if (cmd.startsWith("MAX_TEMP_AIR="))
    {
        MAX_TEMP_AIR = value;
        Serial.print(F("MAX_TEMP_AIR="));
        Serial.println(MAX_TEMP_AIR);
    }

    // --- MIN_HUMID_AIR ---
    else if (cmd.startsWith("HYGR_MIN="))
    {
        HYGR_MIN = value;
        Serial.print(F("HYGR_MIN="));
        Serial.println(HYGR_MIN);
    }

    // --- MAX_HUMID_AIR ---
    else if (cmd.startsWith("HYGR_MAX="))
    {
        HYGR_MAX = value;
        Serial.print(F("HYGR_MAX="));
        Serial.println(HYGR_MAX);
    }

    // --- MIN_LUMIN ---
    else if (cmd.startsWith("MIN_LUMIN="))
    {
        MIN_LUMIN = value;
        Serial.print(F("MIN_LUMIN="));
        Serial.println(MIN_LUMIN);
    }

    // --- MAX_LUMIN ---
    else if (cmd.startsWith("MAX_LUMIN="))
    {
        MAX_LUMIN = value;
        Serial.print(F("MAX_LUMIN="));
        Serial.println(MAX_LUMIN);
    }

    // --- RESET ---
    else if (cmd == "RESET")
    {
        LOG_INTERVAL = DEFAULT_LOG_INTERVAL;
        EEPROM.put(EEPROM_ADDR_LOG_INTERVAL, LOG_INTERVAL);

        MIN_TEMP_AIR = -5;
        MAX_TEMP_AIR = 30;

        HYGR_MIN = 0;
        HYGR_MAX = 100;

        MIN_LUMIN = 0;
        MAX_LUMIN = 1023;

        Serial.println(F("Configuration reinitialisee"));

        Serial.print(F("LOG_INTERVAL="));
        Serial.println(LOG_INTERVAL);

        Serial.print(F("MIN_TEMP_AIR="));
        Serial.println(MIN_TEMP_AIR);
        Serial.print(F("MAX_TEMP_AIR="));
        Serial.println(MAX_TEMP_AIR);

        Serial.print(F("HYGR_MIN="));
        Serial.println(HYGR_MIN);
        Serial.print(F("HYGR_MAX="));
        Serial.println(HYGR_MAX);

        Serial.print(F("MIN_LUMIN="));
        Serial.println(MIN_LUMIN);
        Serial.print(F("MAX_LUMIN="));
        Serial.println(MAX_LUMIN);
    }

    // --- VERSION ---
    else if (cmd == "VERSION")
    {
        Serial.print(F("VERSION="));
        Serial.println((__FlashStringHelper*)PROGRAM_VERSION);

        Serial.print(F("LOT="));
        Serial.println((__FlashStringHelper*)PROGRAM_LOT);
    }

    else
        Serial.println(F("Commande inconnue"));
}

// === LCD ===
void afficherTexteLCD(const char* textePROGMEM)
{
    lcd.clear();
    lcd.print((__FlashStringHelper*)textePROGMEM);
}

void reafficherMode()
{
    switch (modeActuel)
    {
        case DEMARRAGE:   afficherTexteLCD(STR_DEMARRAGE); break;
        case CONFIGURATION: afficherTexteLCD(STR_CONFIG); break;
        case STANDARD:    afficherTexteLCD(STR_STANDARD); break;
        case ECONOMIQUE:  afficherTexteLCD(STR_ECO); break;
        case MAINTENANCE: afficherTexteLCD(STR_MAINT); break;
    }
}

// === MODE ===
void changerMode(Mode m, uint8_t r, uint8_t g, uint8_t b, const char* texte)
{
    if (m == STANDARD || m == ECONOMIQUE)
    {
        if (!ouvrirSD()) return;
    }
    else
        fermerSD();

    modeActuel = m;

    leds.setColorRGB(0, r, g, b);
    afficherTexteLCD(texte);
}

// === HEURE ===
void afficherHeure()
{
    DateTime now = rtc.now();

    if (now.year() < 2020)
    {
        rtcOK = false;
        rtcMessageShown = false;
        return;
    }

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());

    lcd.setCursor(0, 1);
    lcd.print(buffer);

    // === GPS ===
    float lat, lon;
    gps.f_get_position(&lat, &lon);

    lcd.setCursor(10, 1);

    if (lat != TinyGPS::GPS_INVALID_F_ANGLE)
        lcd.print(F("GPS"));
    else
    {
        lcd.print(F("..."));
        clignoterGPS();
    }
}

// === CAPTEURS ===
void lireCapteurs()
{
    if (modeActuel != STANDARD && modeActuel != ECONOMIQUE) return;

    static unsigned long lastSDWrite = 0;

    unsigned long nowMs = millis();
    unsigned long interval;

    if (modeActuel == STANDARD)
        interval = (unsigned long)LOG_INTERVAL * 1000;
    else
        interval = (unsigned long)LOG_INTERVAL * 2 * 1000;

    if (nowMs - lastSDWrite < interval) return;

    lastSDWrite = nowMs;

    float temperature = dht.readTemperature();
    float humidite = dht.readHumidity();
    int luminosite = analogRead(LIGHT_SENSOR);

    // === ERREURS CAPTEURS ===
    bool erreurDHT = isnan(temperature) || isnan(humidite);

    bool erreurTemp = (temperature < MIN_TEMP_AIR || temperature > MAX_TEMP_AIR);
    bool erreurHum  = (humidite   < HYGR_MIN || humidite   > HYGR_MAX);
    bool erreurLum1 = (luminosite == 0 || luminosite == 1023); // capteur saturé
    bool erreurLum2 = (luminosite < MIN_LUMIN || luminosite > MAX_LUMIN);

    if (erreurDHT || erreurTemp || erreurHum || erreurLum1 || erreurLum2)
    {
        afficherTexteLCD(STR_ERR_CAPTEUR);
        clignoterErreurIncoherence();
        return;
    }

    // === GPS ===
    float lat, lon;
    gps.f_get_position(&lat, &lon);
    bool gpsOK = !(lat == TinyGPS::GPS_INVALID_F_ANGLE || lon == TinyGPS::GPS_INVALID_F_ANGLE);

    DateTime now = rtc.now();

    if (sdPleine())
    {
        sdFull = true;
        return;
    }

    enregistrerDonnees(
        temperature, humidite, luminosite,
        gpsOK, lat, lon,
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second()
    );
}

// === SD ===
bool sdPleine()
{
    File f = SD.open((__FlashStringHelper*)FICHIER_CSV, FILE_READ);
    if (!f) return false;

    uint32_t taille = f.size();
    f.close();

    return (taille >= SD_MAX_SIZE);
}

void enregistrerDonnees(float t, float h, int lum, bool gpsOK, float lat, float lon,
                        int annee, int mois, int jour, int heure, int minute, int seconde)
{
    if (modeActuel != STANDARD && modeActuel != ECONOMIQUE) return;

    dataFile = SD.open((__FlashStringHelper*)FICHIER_CSV, FILE_WRITE);
    if (!dataFile) return;

    dataFile.print(annee); dataFile.print("-");
    dataFile.print(mois); dataFile.print("-");
    dataFile.print(jour); dataFile.print(" ");
    dataFile.print(heure); dataFile.print(":");
    dataFile.print(minute); dataFile.print(":");
    dataFile.print(seconde); dataFile.print(";");

    dataFile.print(t); dataFile.print(";");
    dataFile.print(h); dataFile.print(";");
    dataFile.print(lum); dataFile.print(";");

    if (gpsOK)
    {
        dataFile.print(lat, 6); dataFile.print(";");
        dataFile.print(lon, 6);
    }
    else
        dataFile.print(F("NA;NA"));

    dataFile.println();
    dataFile.close();
}

void fermerSD()
{
    if (dataFile) dataFile.close();
}

bool ouvrirSD()
{
    if (!SD.begin(SD_CS))
    {
        sdOK = false;
        afficherTexteLCD(STR_SD_ABSENTE);
        return false;
    }

    sdOK = true;
    return true;
}

// === CLIGNOTEMENT ERREUR GENERIQUE ===
void clignoterErreur(uint8_t r1, uint8_t g1, uint8_t b1,
                     uint8_t r2, uint8_t g2, uint8_t b2,
                     unsigned long interval)
{
    static unsigned long lastBlink = 0;
    static bool etat = false;

    if (millis() - lastBlink >= interval)
    {
        lastBlink = millis();
        etat = !etat;

        if (etat)
            leds.setColorRGB(0, r1, g1, b1);
        else
            leds.setColorRGB(0, r2, g2, b2);
    }
}

// === CLIGNOTEMENT ERREUR INCOHERENCE (ROUGE/VERT 1 Hz) ===
void clignoterErreurIncoherence()
{
    static unsigned long lastBlink = 0;
    static bool etat = false;

    unsigned long now = millis();

    // rouge = 333 ms, vert = 666 ms → 1 Hz total
    unsigned long interval = etat ? 666 : 333;

    if (now - lastBlink >= interval)
    {
        lastBlink = now;
        etat = !etat;

        if (etat)
            leds.setColorRGB(0, 0, 255, 0);   // VERT
        else
            leds.setColorRGB(0, 255, 0, 0);   // ROUGE
    }
}

// === CLIGNOTEMENT GPS ===
void clignoterGPS()
{
    static unsigned long lastBlink = 0;
    static bool etat = false;

    if (millis() - lastBlink >= 500)
    {
        lastBlink = millis();
        etat = !etat;

        if (etat)
            leds.setColorRGB(0, 255, 0, 0);
        else
            leds.setColorRGB(0, 255, 255, 0);
    }
}

// === TEST RTC ===
bool testerRTC()
{
    Wire.beginTransmission(0x68);
    Wire.write(0x00);

    if (Wire.endTransmission() != 0) return false;

    Wire.requestFrom(0x68, 1);

    if (Wire.available() < 1) return false;

    uint8_t sec = Wire.read();

    if (sec == 0xFF) return false;
    if ((sec & 0x7F) > 59) return false;

    return true;
}

// === BOUTONS ===
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

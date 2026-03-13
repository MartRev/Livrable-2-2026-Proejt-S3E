#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <ChainableLED.h>
#include "rgb_lcd.h"
#include <SoftwareSerial.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <SD.h>
#include <avr/interrupt.h>

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

// === OBJETS ===
ChainableLED leds(LED_DATA_PIN, LED_CLOCK_PIN, 1);
rgb_lcd lcd;
RTC_DS1307 rtc;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
DHT dht(DHTPIN, DHTTYPE);
TinyGPSPlus gps;

File dataFile;

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
const char STR_DEMARRAGE[] PROGMEM = "Mode Demarrage";
const char STR_CONFIG[] PROGMEM = "Mode Config";
const char STR_STANDARD[] PROGMEM = "Mode Standard";
const char STR_ECO[] PROGMEM = "Mode Eco";
const char STR_MAINT[] PROGMEM = "Maintenance";
const char STR_INIT[] PROGMEM = "Initialisation";
const char STR_RTC_ERROR[] PROGMEM = "RTC erreur";
const char STR_ERR_CAPTEUR[] PROGMEM = "erreur capteur";
const char STR_SD_ABSENTE[] PROGMEM = "SD absente !";
const char STR_SD_PLEINE[] PROGMEM = "SD pleine !";

// === LIMITE SD PLEINE ===
const unsigned long long SD_MAX_SIZE = 26843545600ULL; // 25 Go

// === PROTOTYPES ===
void configTimer1();
void changerMode(Mode m, uint8_t r,uint8_t g,uint8_t b,const char* texte);
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
bool testerRTC();
void reafficherMode();

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
    pinMode(BUTTON_RED,INPUT_PULLUP);
    pinMode(BUTTON_GREEN,INPUT_PULLUP);

    gpsSerial.begin(9600);
    dht.begin();
    Wire.begin();

    lcd.begin(16,2);
    lcd.setRGB(255,255,255);
    afficherTexteLCD(STR_INIT);
    delay(2000);

    sdOK = SD.begin(SD_CS);

    if(!rtc.begin())
    {
        rtcOK = false;
        afficherTexteLCD(STR_RTC_ERROR);
        rtcMessageShown = true;
        delay(2000);
    }

    configTimer1();
    changerMode(DEMARRAGE,255,255,255,STR_DEMARRAGE);
}

// === LOOP ===
void loop()
{
    gererBoutons();

    // === LECTURE GPS EN CONTINU ===
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }

    // --- ERREUR SD ABSENTE ---
    if (!sdOK) {
        clignoterErreur(255,255,255, 255,0,0, 500);
        return;
    }

    // --- ERREUR RTC ---
    if (!rtcOK) {
        if (!rtcMessageShown) {
            afficherTexteLCD(STR_RTC_ERROR);
            rtcMessageShown = true;
        }

        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 1000) {
            lastCheck = millis();
            rtcOK = testerRTC();
            if (rtcOK) {
                rtcMessageShown = false;
                reafficherMode();
            }
        }

        if (!rtcOK) {
            clignoterErreur(0,0,255, 255,0,0, 500);
            return;
        }
    }

    // --- ERREUR SD PLEINE ---
    if (sdFull) {
        afficherTexteLCD(STR_SD_PLEINE);
        clignoterErreur(255,0,0, 255,255,255, 500);
        return;
    }

    if(flagHorloge) {
        flagHorloge = false;
        afficherHeure();
    }

    if(flagCapteurs) {
        flagCapteurs = false;
        lireCapteurs();
    }
}

// === LCD ===
void afficherTexteLCD(const char* textePROGMEM)
{
    char buffer[20];
    strcpy_P(buffer, textePROGMEM);
    lcd.clear();
    lcd.print(buffer);
}

void reafficherMode()
{
    switch(modeActuel) {
        case DEMARRAGE:     afficherTexteLCD(STR_DEMARRAGE); break;
        case CONFIGURATION: afficherTexteLCD(STR_CONFIG); break;
        case STANDARD:      afficherTexteLCD(STR_STANDARD); break;
        case ECONOMIQUE:    afficherTexteLCD(STR_ECO); break;
        case MAINTENANCE:   afficherTexteLCD(STR_MAINT); break;
    }
}

// === MODE GENERIQUE ===
void changerMode(Mode m, uint8_t r,uint8_t g,uint8_t b,const char* texte)
{
    if (m == STANDARD || m == ECONOMIQUE) {
        if (!ouvrirSD()) return;
    } else {
        fermerSD();
    }

    modeActuel = m;
    leds.setColorRGB(0,r,g,b);
    afficherTexteLCD(texte);
}

// === HEURE ===
void afficherHeure()
{
    DateTime now = rtc.now();

    if (now.year() < 2020) {
        rtcOK = false;
        rtcMessageShown = false;
        return;
    }

    char buffer[10];
    snprintf(buffer,sizeof(buffer),"%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    lcd.setCursor(0,1);
    lcd.print(buffer);

    // Indicateur GPS
    lcd.setCursor(10,1);
    if (gps.location.isValid()) lcd.print("GPS");
    else lcd.print("...");
}

// === CAPTEURS ===
void lireCapteurs()
{
    if(modeActuel != STANDARD && modeActuel != ECONOMIQUE) return;

    static unsigned long lastSDWrite = 0;
    unsigned long nowMs = millis();
    unsigned long interval = (modeActuel == STANDARD) ? 15000 : 30000;

    if(nowMs - lastSDWrite < interval) return;
    lastSDWrite = nowMs;

    float temperature = dht.readTemperature();
    float humidite = dht.readHumidity();
    int luminosite = analogRead(LIGHT_SENSOR);

    bool erreurDHT = isnan(temperature) || isnan(humidite);
    bool erreurLum = (luminosite == 0 || luminosite == 1023);

    if (erreurDHT || erreurLum) {
        afficherTexteLCD(STR_ERR_CAPTEUR);
        clignoterErreur(225,0,0, 0,255,0, 500);
        return;
    }

    bool gpsOK = gps.location.isValid();
    float lat = gps.location.lat();
    float lon = gps.location.lng();

    DateTime now = rtc.now();

    if (sdPleine()) {
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
    File f = SD.open("donnees.csv", FILE_READ);
    if (!f) return false;

    unsigned long long taille = f.size();
    f.close();

    return (taille >= SD_MAX_SIZE);
}

void enregistrerDonnees(float t,float h,int lum,bool gpsOK,float lat,float lon,
                        int annee,int mois,int jour,int heure,int minute,int seconde)
{
    if(modeActuel != STANDARD && modeActuel != ECONOMIQUE) return;

    dataFile = SD.open("donnees.csv",FILE_WRITE);
    if(!dataFile) return;

    dataFile.print(annee); dataFile.print("-");
    dataFile.print(mois);  dataFile.print("-");
    dataFile.print(jour);  dataFile.print(" ");
    dataFile.print(heure); dataFile.print(":");
    dataFile.print(minute); dataFile.print(":");
    dataFile.print(seconde); dataFile.print(";");

    dataFile.print(t); dataFile.print(";");
    dataFile.print(h); dataFile.print(";");
    dataFile.print(lum); dataFile.print(";");

    if(gpsOK){
        dataFile.print(lat,6); dataFile.print(";");
        dataFile.print(lon,6);
    } else {
        dataFile.print("NA;NA");
    }

    dataFile.println();
    dataFile.close();
}

void fermerSD(){ if(dataFile) dataFile.close(); }

bool ouvrirSD()
{
    if (!SD.begin(SD_CS)) {
        sdOK = false;
        afficherTexteLCD(STR_SD_ABSENTE);
        return false;
    }
    sdOK = true;
    return true;
}

// === CLIGNOTEMENT GENERIQUE ===
void clignoterErreur(uint8_t r1,uint8_t g1,uint8_t b1,
                     uint8_t r2,uint8_t g2,uint8_t b2,
                     unsigned long interval)
{
    static unsigned long lastBlink = 0;
    static bool etat = false;

    unsigned long now = millis();

    if (now - lastBlink >= interval)
    {
        lastBlink = now;
        etat = !etat;

        if (etat)
            leds.setColorRGB(0, r1,g1,b1);
        else
            leds.setColorRGB(0, r2,g2,b2);
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

    if(redState && !redPressed && now - lastRedPress > DEBOUNCE_MS)
    {
        redPressed = true;
        lastRedPress = now;

        if(modeActuel == DEMARRAGE) changerMode(CONFIGURATION,255,255,0,STR_CONFIG);
        else if(modeActuel == CONFIGURATION) changerMode(STANDARD,0,255,0,STR_STANDARD);
        else if(modeActuel == STANDARD) changerMode(MAINTENANCE,255,165,0,STR_MAINT);
        else if(modeActuel == MAINTENANCE) changerMode(STANDARD,0,255,0,STR_STANDARD);
        else if(modeActuel == ECONOMIQUE) changerMode(MAINTENANCE,255,165,0,STR_MAINT);
    }
    if(!redState) redPressed = false;

    if(greenState && !greenPressed && now - lastGreenPress > DEBOUNCE_MS)
    {
        greenPressed = true;
        lastGreenPress = now;

        if(modeActuel == DEMARRAGE) changerMode(CONFIGURATION,255,255,0,STR_CONFIG);
        else if(modeActuel == CONFIGURATION) changerMode(STANDARD,0,255,0,STR_STANDARD);
        else if(modeActuel == STANDARD) changerMode(ECONOMIQUE,0,0,255,STR_ECO);
        else if(modeActuel == ECONOMIQUE) changerMode(STANDARD,0,255,0,STR_STANDARD);
        else if(modeActuel == MAINTENANCE) changerMode(ECONOMIQUE,0,0,255,STR_ECO);
    }
    if(!greenState) greenPressed = false;
}

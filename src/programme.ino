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
    leds.setColorRGB(0,r,g,b

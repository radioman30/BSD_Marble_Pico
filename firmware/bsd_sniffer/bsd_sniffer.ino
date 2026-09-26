/*
 * bsd_sniffer.ino
 * ---------------
 * ANALIZOR single-wire pentru protocolul BSD / BSS (Bosch Bit Serial Device),
 * pe placa GroundStudio Marble Pico (RP2040). Proiect SEPARAT de cel LIN.
 *
 * BSD e proprietar si NEDOCUMENTAT public. Scopul acestui firmware e sa masori
 * pe viu semnalul ca sa-i decodezi timing-ul si framing-ul:
 *   - captureaza fiecare front de pe linie (timestamp us + nivel),
 *   - tipareste pe USB serial durata fiecarui puls, grupata pe "frame-uri"
 *     (separate de un idle configurabil),
 *   - afiseaza pe OLED un rezumat (nr. frame-uri, ultimul puls, TBIT estimat).
 *
 * De ce IRQ+micros() si nu PIO acum: la vitezele BSD (bit ~tens of us) captura
 * pe intrerupere e fiabila si suficienta pentru reverse-engineering, si COMPILEAZA
 * garantat. Dupa ce cunosti TBIT-ul exact, se poate trece pe PIO pentru precizie
 * (vezi docs/BSD_PROTOCOL.md, sectiunea "Upgrade PIO").
 *
 * HARDWARE (single-wire, NU transceiver LIN/CAN):
 *   Linia BSD e un fir bidirectional cu pull-up, la nivel LOGIC (masoara! tipic ~5V).
 *   Spre RP2040 (3.3V) pune un level shifter bidirectional cu MOSFET (BSS138) +
 *   rezistenta serie + TVS. Pentru DOAR ascultare (sniff) e suficient un divizor
 *   rezistiv / level shifter pe intrare. Masa comuna obligatorie.
 *
 *   ATENTIE: confirma tensiunea liniei cu osciloscopul INAINTE de conectare.
 *
 * Placa / core: rp2040:rp2040:groundstudio_marble_pico (Earle Philhower core).
 * Biblioteci: Adafruit SSD1306 + Adafruit GFX (optional OLED pe conectorul STEMMA QT).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>

// ------------------------------ Config ------------------------------
#define BSD_PIN        15          // GP15 - intrarea single-wire (prin level shifter)
#define USE_OLED       1           // 1 = afiseaza pe OLED SSD1306 I2C
#define OLED_SDA       4           // GP4 (I2C0) - confirma cu conectorul STEMMA QT
#define OLED_SCL       5           // GP5 (I2C0)
#define OLED_ADDR      0x3C

// microSD pe SPI0 (Marble Pico). CONFIRMA pinii din schema placii!
#define USE_SD         1           // 1 = logheaza capturile pe card
#define SD_MISO        16          // GP16 (SPI0 RX)
#define SD_CS          17          // GP17
#define SD_SCK         18          // GP18
#define SD_MOSI        19          // GP19 (SPI0 TX)

#define IDLE_END_US    5000        // pauza care termina un "frame" [us] (calibreaza)
#define GLITCH_MIN_US  3           // ignora pulsuri mai scurte de atat
#define RING_SIZE      2048        // fronturi tamponate

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOk = false;

// microSD
bool  sdOk = false;
File  logFile;
char  logName[16] = "-";

// ------------------------------ Captura fronturi (ISR) ------------------------------
struct Edge { uint32_t t_us; uint8_t level; };
volatile Edge ring[RING_SIZE];
volatile uint32_t rHead = 0, rTail = 0;

void onEdge() {                     // RP2040: ISR ruleaza din flash, fara IRAM_ATTR
  uint32_t now = micros();
  uint8_t lvl = digitalRead(BSD_PIN);
  uint32_t h = rHead;
  uint32_t nxt = (h + 1) % RING_SIZE;
  if (nxt != rTail) {               // nu suprascrie daca bufferul e plin
    ring[h].t_us = now;
    ring[h].level = lvl;
    rHead = nxt;
  }
}

// ------------------------------ Statistici / stare ------------------------------
uint32_t frameCount = 0;
uint32_t lastPulseUs = 0;
uint32_t minPulseUs = 0xFFFFFFFF;   // candidat pentru TBIT
uint32_t framePulses = 0;
uint32_t lastEdgeT = 0;
bool     haveLastEdge = false;

// ------------------------------ Decodare (SCHELET) ------------------------------
// Dupa ce cunosti TBIT si framing-ul din capturi, implementeaza aici reconstructia
// octetilor (esantionare la mijlocul fiecarui bit, start/stop, checksum).
size_t decodeFrame(const uint32_t* durs, const uint8_t* lvls, size_t n,
                   uint8_t* out, size_t outCap) {
  // TODO: reconstruieste nivelul pe axa timpului si esantioneaza la pas TBIT.
  (void)durs; (void)lvls; (void)n; (void)out; (void)outCap;
  return 0;
}

// ------------------------------ microSD logging ------------------------------
// Deschide un fisier nou BSD_NNN.CSV (primul index liber) si scrie antetul.
void openNewLog() {
#if USE_SD
  if (!sdOk) return;
  if (logFile) logFile.close();
  for (int i = 0; i < 1000; i++) {
    snprintf(logName, sizeof(logName), "BSD_%03d.CSV", i);
    if (!SD.exists(logName)) break;
  }
  logFile = SD.open(logName, FILE_WRITE);
  if (logFile) {
    logFile.println("# BSD sniffer capture");
    logFile.println("t_us,frame,level,dur_us");
    logFile.flush();
    Serial.printf("SD: loghez in %s\n", logName);
  } else {
    Serial.println("SD: nu pot deschide fisierul de log");
  }
#endif
}

void logPulse(uint32_t t_us, uint32_t frame, uint8_t level, uint32_t dur) {
#if USE_SD
  if (!sdOk || !logFile) return;
  char line[40];
  snprintf(line, sizeof(line), "%lu,%lu,%u,%lu",
           (unsigned long)t_us, (unsigned long)frame, level, (unsigned long)dur);
  logFile.println(line);
#endif
}

void logFlush() {
#if USE_SD
  if (sdOk && logFile) logFile.flush();
#endif
}

// ------------------------------ OLED ------------------------------
void oledStatus() {
#if USE_OLED
  if (!oledOk) return;
  static uint32_t last = 0;
  if (millis() - last < 250) return;
  last = millis();
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE); oled.setCursor(0, 0);
  oled.println("BSD sniffer (RP2040)");
  oled.printf("frames: %lu\n", (unsigned long)frameCount);
  oled.printf("last pulse: %lu us\n", (unsigned long)lastPulseUs);
  oled.printf("min pulse : %lu us\n",
              minPulseUs == 0xFFFFFFFF ? 0UL : (unsigned long)minPulseUs);
  if (sdOk) oled.printf("SD: %s\n", logName);
  else      oled.printf("SD: - (fara card)\n");
  oled.display();
#endif
}

// ------------------------------ setup / loop ------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(BSD_PIN, INPUT);          // pull-up-ul e extern pe bus
  attachInterrupt(digitalPinToInterrupt(BSD_PIN), onEdge, CHANGE);

#if USE_OLED
  Wire.setSDA(OLED_SDA); Wire.setSCL(OLED_SCL); Wire.begin();
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
#endif

#if USE_SD
  SPI.setRX(SD_MISO); SPI.setTX(SD_MOSI); SPI.setSCK(SD_SCK); SPI.setCS(SD_CS);
  sdOk = SD.begin(SD_CS);
  Serial.println(sdOk ? "SD: card OK" : "SD: card lipsa/eroare (continui fara log)");
  openNewLog();
#endif

  Serial.println("\nBSD/BSS single-wire sniffer - Marble Pico (RP2040)");
  Serial.printf("Ascult GP%d. Idle frame=%d us. Alimenteaza modulul...\n",
                BSD_PIN, IDLE_END_US);
  Serial.println("Comenzi serial: 'n' = fisier de log nou");
}

void loop() {
  // proceseaza fronturile din ring
  while (rTail != rHead) {
    Edge e;
    e.t_us  = ring[rTail].t_us;     // citire camp-cu-camp din bufferul volatile
    e.level = ring[rTail].level;
    rTail = (rTail + 1) % RING_SIZE;

    if (haveLastEdge) {
      uint32_t dur = e.t_us - lastEdgeT;
      uint8_t prevLevel = !e.level;      // nivelul dinaintea acestui front

      if (dur >= GLITCH_MIN_US) {
        // sfarsit de frame? (pauza lunga pe linie idle-high)
        if (dur >= IDLE_END_US && framePulses > 0) {
          Serial.printf("  [idle %lu us] --- frame #%lu: %lu pulsuri ---\n",
                        (unsigned long)dur, (unsigned long)frameCount,
                        (unsigned long)framePulses);
          logFlush();                    // persista frame-ul pe card
          frameCount++;
          framePulses = 0;
        } else {
          // tipareste pulsul: nivel + durata
          Serial.printf("  %s %5lu us\n", prevLevel ? "HIGH" : "LOW ",
                        (unsigned long)dur);
          logPulse(e.t_us, frameCount, prevLevel, dur);   // -> microSD
          lastPulseUs = dur;
          if (dur < minPulseUs) minPulseUs = dur;
          framePulses++;
        }
      }
    }
    lastEdgeT = e.t_us;
    haveLastEdge = true;
  }

  // comenzi serial
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'n' || c == 'N') { openNewLog(); frameCount = 0; }
  }

  oledStatus();
}

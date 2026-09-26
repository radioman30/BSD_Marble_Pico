# Jurnal de discuții și decizii — proiect BSD/BSS Marble Pico

Sesiune: **2026-07-12**. Log cronologic al deciziilor pentru proiectul dedicat
protocolului **BSD/BSS** (Bosch Bit Serial Device) pe **GroundStudio Marble Pico**
(RP2040). Proiectul e SEPARAT de cel LIN (care rămâne pe ESP32, în
`C:\Users\123\ESP32_CAN_LIN_Database\`).

---

## 1. Decizia: proiect separat pe Marble Pico

**Context:** proiectul LIN (tester alternator) merge pe ESP32. Pentru BSD/BSS s-au
discutat mai multe plăci: ESP32-C3, 01Space ESP32-C3-0.42LCD, Cytron Maker Pi Pico
Mini W. Evaluare Pico W vs ESP32: la CAN ESP32 e mai bun (TWAI nativ), dar RP2040
câștigă la **microSD** (logging) și **PIO** (single-wire) — exact ce trebuie la BSD.

**Decizie:** LIN rămâne separat pe ESP32; **BSD = proiect nou pe GroundStudio
Marble Pico** (RP2040). De ce BSD e diferit: e proprietar Bosch, single-wire,
nedocumentat public → necesită reverse-engineering, nu un driver gata făcut.

## 2. Confirmarea plăcii

Marble Pico = **RP2040**, 8 MB flash QSPI, USB-C, **microSD pe SPI0**, conector I2C
**STEMMA QT**, **fără WiFi** (deci fără interfață web — output pe USB serial + OLED +
SD). Pinout compatibil Raspberry Pi Pico. Core arduino-cli
`rp2040:rp2040:groundstudio_marble_pico` (Earle Philhower) — instalat, deci se poate
compila și valida.

## 3. Firmware sniffer

`firmware/bsd_sniffer/bsd_sniffer.ino` — ANALIZOR single-wire:
- captură fronturi pe **întrerupere + micros()** (fiabil la vitezele BSD, compilează
  garantat), într-un ring buffer;
- tipărește pe serial durata fiecărui puls (HIGH/LOW, µs), grupată pe frame-uri
  (separate de un idle configurabil);
- OLED de status (nr. frame-uri, puls minim = candidat TBIT);
- schelet `decodeFrame()` de completat după caracterizarea protocolului.

**Bug-uri reparate la compilare (specific RP2040):**
- copiere dintr-un `struct volatile` → citire câmp-cu-câmp;
- `IRAM_ATTR` e macro ESP32, nu există pe RP2040 → scos (ISR rulează din flash).

Compilat OK: 80 KB (0% flash), 26 KB RAM.

## 4. Logging pe microSD

Adăugat la cerere. La fiecare pornire deschide un fișier nou `BSD_NNN.CSV` (index
liber), loghează fiecare puls (`t_us,frame,level,dur_us`), flush la sfârșit de frame,
comandă serial `n` = fișier nou. Dacă nu e card, continuă doar cu serial+OLED.
Pini SD pe SPI0 (GP16-19, maparea standard) — **de confirmat din schema plăcii**.
Compilat OK: 130 KB (1% flash), 28 KB RAM.

## 5. Script Python de analiză

`analysis/analyze_bsd.py` (pur Python, fără dependințe — pip e stricat pe acest PC):
- histogramă ASCII a duratelor de puls;
- **estimare TBIT** (minimizează reziduul pe multipli k×TBIT);
- distribuția dur/TBIT (grupare pe 1,2,3… = bit corect);
- reconstrucție naivă a biților + grupare în octeți (LSB/MSB-first);
- sumar per frame + top durate.
**Testat** pe o captură sintetică (TBIT=100µs) → estimat corect (reziduu 2.4µs).

## 6. Upload pe placă

Marble Pico detectată pe **COM10**, auto-identificată corect. **Compilat + încărcat**
prin UF2 (reset auto → BOOTSEL RPI-RP2 pe E: → scris 297 KB). Verificat („Wrote
297472 bytes"). Placa rulează sniffer-ul.

## 7. Interfața hardware spre linia BSD (Q&A)

**Întrebare:** ce integrat trebuie sau intrarea merge direct pe pin?

**Răspuns:**
- **RP2040 GPIO = 3.3V, NU e 5V-tolerant.** BSD tipic ~5V → **direct doar dacă linia
  e ≤3.3V** (măsoară cu osciloscopul întâi!).
- **Pentru SNIFF (doar ascultare):** fără integrat — **divizor rezistiv** 5V→3.3V
  (ex. 1k serie + 10k/20k) + opțional TVS/Zener 3.3V. Masă comună obligatorie.
- **Pentru TALK (TX+RX pe același fir, mai târziu):** **level shifter bidirecțional
  open-drain** — **BSS138** (1 MOSFET + 2 pull-up, discret) sau IC **TXS0102**. BSD e
  electric ca I2C (open-drain + pull-up), deci merg „shifter-ele de I2C".
- **Recomandare:** un modul mic „I2C Level Converter BSS138" (4 canale) — merge și
  pentru sniff și pentru talk, folosești un canal. Pasul zero: măsoară tensiunea
  liniei cu osciloscopul.

## 8. Următorii pași

1. Măsoară tensiunea liniei BSD (osciloscop) → confirmă interfața (rezistențe/BSS138).
2. Conectează la GP15 prin level shifter + card microSD (confirmă pinii SPI0).
3. Capturează → `analyze_bsd.py` → deduci TBIT + framing.
4. Completează `decodeFrame()` cu framing-ul găsit.
5. (Opțional) upgrade pe **PIO** pentru captură/emisie de precizie.

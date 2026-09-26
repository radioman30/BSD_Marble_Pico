# BSD / BSS Analyzer — GroundStudio Marble Pico (RP2040)

Proiect **separat** pentru protocolul **BSD / BSS** (Bosch Bit Serial Device) —
interfața single-wire folosită de alternatoare „inteligente", senzori de baterie
(IBS), etc. Proiectul LIN rămâne separat (pe ESP32); acesta e dedicat BSD.

Placă: **GroundStudio Marble Pico** (RP2040, 8 MB flash, USB-C, microSD pe SPI0,
conector I2C STEMMA QT, fără WiFi). Core: `rp2040:rp2040:groundstudio_marble_pico`
(Earle Philhower).

## De ce placa asta

- **PIO** — ideal pentru protocoale single-wire custom (upgrade de precizie).
- **microSD** — logarea capturilor BSD direct pe card, fără PC.
- **STEMMA QT I2C** — OLED de status prin conectorul dedicat.
- 3.3V logic (ai nevoie de level shifter spre linia BSD).

## Structură

```
BSD_Marble_Pico/
├── README.md
├── firmware/
│   └── bsd_sniffer/bsd_sniffer.ino   ← ANALIZOR: capturează pulsuri, decodează timing
└── docs/
    ├── BSD_PROTOCOL.md               ← ce se știe + flux reverse-engineering + upgrade PIO
    └── HARDWARE_MARBLE_PICO.md       ← interfața single-wire + pinout Marble Pico
```

## Fluxul de lucru

BSD e **proprietar și nedocumentat public** — nu poți scrie decodorul până nu
măsori semnalul. De aceea proiectul începe cu un **analizor**:

1. **Hardware**: conectează linia BSD la RP2040 printr-un level shifter (vezi
   `docs/HARDWARE_MARBLE_PICO.md`). Confirmă tensiunea liniei cu osciloscopul întâi.
2. **Sniff**: încarci `bsd_sniffer.ino`, deschizi Serial Monitor (115200). Firmware-ul
   tipărește durata fiecărui puls (HIGH/LOW, µs), grupată pe frame-uri, și **loghează
   totul pe microSD** (`BSD_000.CSV`, `BSD_001.CSV`…). OLED-ul arată nr. de frame-uri,
   pulsul minim (candidat pentru TBIT) și fișierul de log curent. Trimite `n` pe serial
   pentru un fișier nou.
3. **Analiză**: din capturi determini **TBIT** (durata unui bit), structura frame-ului
   (start/stop, nr. biți, ordinea), idle-ul între frame-uri, checksum-ul.
4. **Decodare**: completezi funcția `decodeFrame()` din sketch cu framing-ul găsit
   (eșantionare la mijlocul fiecărui bit).
5. (Opțional) **Upgrade PIO** pentru captură de precizie — vezi `docs/BSD_PROTOCOL.md`.

### Format log microSD

CSV, o linie per puls (ușor de analizat în Python/Excel):

```
# BSD sniffer capture
t_us,frame,level,dur_us
1234567,0,1,100     <- timestamp absolut, index frame, nivel (1=HIGH/0=LOW), durata us
```

`level` e nivelul liniei ÎNAINTE de front, `dur_us` cât a durat. Frame-urile sunt
separate de un idle > `IDLE_END_US`.

## Stare

- `bsd_sniffer.ino` — **compilat și verificat** pentru Marble Pico (130 KB, 1% flash;
  28 KB RAM). Captură pe întrerupere + micros(), OLED de status, **logging microSD
  activ** (CSV per puls), comandă serial `n` = fișier nou, decodor schelet.
- De făcut: caracterizare timing pe hardware real → completare `decodeFrame()` →
  upgrade PIO pentru precizie.

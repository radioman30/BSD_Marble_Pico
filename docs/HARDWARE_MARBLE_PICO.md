# Hardware — interfața BSD single-wire pe Marble Pico

## GroundStudio Marble Pico (RP2040)

| Caracteristică | Valoare |
|---|---|
| MCU | RP2040 (dual-core Cortex-M0+, 3.3V) |
| Flash | 8 MB QSPI |
| SRAM | 264 kB |
| USB | Type-C |
| Wireless | **nu** (deci fără interfață web — output pe USB serial + OLED + SD) |
| microSD | pe **SPI0** |
| I2C | conector **STEMMA QT / Qwiic** |
| LED | GP25 |
| Pinout | compatibil Raspberry Pi Pico |

## Interfața spre linia BSD (critică)

Linia BSD e **un fir bidirectional**, open-drain, cu **pull-up**, la **nivel logic**
(nu 12V — dar **măsoară cu osciloscopul**, tipic ~5V). RP2040 e 3.3V, deci:

### Doar ascultare (sniff) — minim
- **Level shifter pe intrare** (5V→3.3V): divizor rezistiv (ex. 10k/20k) SAU un
  MOSFET level shifter. RP2040 GPIO **nu e tolerant la 5V**, deci nu conecta direct.
- Masă comună între modul și Marble Pico — obligatoriu.
- Rezistență serie (~1k) + eventual TVS pe intrare pentru protecție.

### Emisie + recepție (talk) — bidirectional
- **Level shifter bidirectional cu MOSFET** (schema clasică BSS138) pe singura linie,
  cu pull-up-urile aferente pe ambele domenii.
- Emisia se face **open-drain** (tragi linia la 0, pull-up-ul o aduce la 1) — nu
  conduce niciodată activ spre "1", ca să nu intri în conflict cu masterul/slave-ul.

> ⚠️ Confirmă întâi cu osciloscopul: tensiunea de idle a liniei, amplitudinea,
> și timing-ul. Abia apoi conectează RP2040-ul.

## Pinout folosit de firmware (configurabil în sketch)

| Semnal | GPIO | Note |
|---|---|---|
| **BSD line** (intrare/IO) | **GP15** | prin level shifter |
| OLED I2C SDA | GP4 (I2C0) | **confirmă** ce pini folosește conectorul STEMMA QT |
| OLED I2C SCL | GP5 (I2C0) | idem |
| microSD (SPI0) | GP16–19 | pentru logging capturi (pas următor) |
| LED | GP25 | onboard |

Notă STEMMA QT: dacă pe placa ta conectorul I2C e pe alți pini decât GP4/GP5,
schimbă `OLED_SDA` / `OLED_SCL` în `bsd_sniffer.ino`.

## Bibliotecile necesare (Arduino)

- Core: **Raspberry Pi Pico/RP2040** (Earle Philhower) — include board-ul
  „GroundStudio Marble Pico".
- **Adafruit SSD1306** + **Adafruit GFX** (pentru OLED-ul de status).
- Pentru logging microSD: `SD` / `SDFS` (incluse în core).

## Alimentare

- Modulul BSD (alternator/senzor) și Marble Pico trebuie să aibă **masă comună**.
- Marble Pico se alimentează pe USB-C; nu alimenta linia BSD din RP2040.

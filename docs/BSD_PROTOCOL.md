# BSD / BSS — protocol și flux de reverse-engineering

## Ce este BSD / BSS

**BSD** (Bit Serial Device / *Bitserielle Datenschnittstelle*), uneori scris **BSS**,
e o interfață **proprietară Bosch**, pe **un singur fir**, half-duplex, master-slave,
folosită pentru:
- alternatoare „inteligente" (regulator multifuncțional),
- senzori inteligenți de baterie (IBS / EBS),
- alte module Bosch.

Caracteristici electrice generale (**de confirmat cu osciloscopul pe piesa ta**):
- un singur fir bidirectional, cu **pull-up**, idle pe **nivel HIGH**;
- **nivel logic** (nu 12V), tipic în domeniul ~5V;
- masterul (ECU) inițiază comunicația; slave-ul răspunde.

> **Important:** timing-ul exact (TBIT), structura frame-ului, adresarea și
> checksum-ul **NU sunt publicate** și diferă între module. De aceea trebuie
> caracterizate empiric. Nu există transceiver BSD comercial (e interfață internă).

## Flux de reverse-engineering (cu `bsd_sniffer.ino`)

1. **Captează** cu firmware-ul de sniff. Serial Monitor tipărește, pentru fiecare
   frame, lista de pulsuri: `HIGH/LOW  durata_us`. OLED-ul arată nr. frame-uri și
   **pulsul minim** (bun candidat pentru un bit = TBIT).

2. **Determină TBIT**: pulsul cel mai scurt care se repetă e de obicei un bit.
   Duratele mai mari sunt multipli (2×, 3×… biți de același nivel la rând).

3. **Structura frame-ului**:
   - identifică **idle**-ul dintre frame-uri (pauza HIGH lungă) → setează
     `IDLE_END_US` în sketch ca să grupeze corect.
   - numără biții/frame, caută un **bit de start** (de obicei o tranziție LOW din idle),
     eventual bit(i) de **stop**, și un câmp de **checksum** la final.

4. **Encoding-ul biților**: verifică dacă e:
   - **NRZ** (nivel = valoarea bitului pe durata TBIT), sau
   - **auto-clock** (ex. Manchester / lățime de puls variabilă). Compară duratele
     HIGH vs LOW: dacă mereu una din două lățimi → e codare pe lățime de puls.

5. **Decodare**: completează `decodeFrame()` în sketch:
   - reconstruiește nivelul liniei pe axa timpului din pulsuri,
   - eșantionează la **mijlocul fiecărui bit** (pas TBIT),
   - asamblează octeții (verifică ordinea biților: LSB-first vs MSB-first),
   - validează **checksum-ul** (încearcă sumă/XOR/CRC simplu pe octeți).

6. **Mapare semnale**: după ce ai octeții, corelează cu starea reală a
   alternatorului (tensiune, excitație, temperatură, turație, erori) variind
   condițiile pe banc și urmărind ce octeți se schimbă.

## Upgrade PIO (precizie)

Captura actuală (întrerupere + `micros()`, rezoluție 1 µs) e suficientă pentru
RE la vitezele BSD. Pentru precizie mai mare / decodare hardware în timp real,
RP2040 are **PIO** — ideal pentru single-wire:

- Un program PIO care **măsoară intervalul dintre fronturi** (număr de cicluri PIO)
  și îl împinge în FIFO → rezoluție sub-µs, fără jitter de întrerupere.
- Un al doilea program PIO poate **emite** biți (open-drain) cu timing exact.
- În core-ul Earle Philhower, PIO se folosește prin `hardware/pio.h` (SDK) sau
  fișiere `.pio`. Se implementează **după** ce cunoști TBIT-ul (ca să setezi
  divizorul de clock PIO corect).

Pașii: (1) caracterizezi TBIT cu sniffer-ul actual → (2) scrii programul PIO de
captură cu clock potrivit → (3) muți decodarea pe FIFO-ul PIO.

## Resurse / punct de plecare

Există lucrări comunitare de reverse-engineering pe BSD (ex. citirea senzorului
IBS de baterie pe BMW). Caută capturi/timing publicate pentru **modelul tău exact**
de alternator/senzor — pot scurta mult pasul de caracterizare.

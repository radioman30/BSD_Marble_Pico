#!/usr/bin/env python3
"""
analyze_bsd.py
--------------
Analizeaza capturile BSD/BSS logate de bsd_sniffer.ino pe microSD
(BSD_NNN.CSV: coloane t_us,frame,level,dur_us).

Ce face — ajuta la reverse-engineering-ul protocolului:
  - histograma ASCII a duratelor de puls (fara dependinte externe),
  - estimeaza TBIT (durata unui bit) prin potrivire pe multipli,
  - arata cat de bine se incadreaza duratele in k*TBIT (1x,2x,3x...),
  - sumar per frame,
  - reconstructie NAIVA a fluxului de biti (fiecare puls -> round(dur/TBIT) biti),
    plus o incercare de grupare in octeti (LSB-first si MSB-first) ca punct de plecare.

Rulare:
    python analyze_bsd.py BSD_000.CSV
    python analyze_bsd.py <folder>        # ia cel mai nou BSD_*.CSV din folder
    python analyze_bsd.py                 # cauta in folderul curent
"""

import sys
import os
import glob
from collections import Counter


def load(path):
    rows = []
    with open(path, errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith("t_us"):
                continue
            p = line.split(",")
            if len(p) < 4:
                continue
            try:
                rows.append((int(p[0]), int(p[1]), int(p[2]), int(p[3])))
            except ValueError:
                continue
    return rows


def estimate_tbit(durs):
    """Cauta TBIT care minimizeaza reziduul |d - round(d/TBIT)*TBIT|."""
    ds = [d for d in durs if d > 0]
    if not ds:
        return None, None
    dmin = min(ds)
    lo, hi = max(1, int(dmin * 0.55)), int(dmin * 1.8) + 1
    best, best_cost = None, 1e18
    for tb in range(lo, hi + 1):
        cost = 0.0
        for d in ds:
            k = max(1, round(d / tb))
            cost += abs(d - k * tb)
        cost /= len(ds)
        if cost < best_cost:
            best_cost, best = cost, tb
    return best, best_cost


def ascii_hist(values, width=50, bins=24, unit="us"):
    if not values:
        print("  (fara valori)")
        return
    lo, hi = min(values), max(values)
    if lo == hi:
        print(f"  toate = {lo} {unit}  ({len(values)} valori)")
        return
    bw = (hi - lo) / bins
    counts = [0] * bins
    for v in values:
        i = min(bins - 1, int((v - lo) / bw))
        counts[i] += 1
    mx = max(counts) or 1
    for i, c in enumerate(counts):
        a = lo + i * bw
        b = a + bw
        bar = "#" * int(width * c / mx)
        if c:
            print(f"  {a:7.0f}-{b:7.0f} {unit} | {bar} {c}")


def naive_bits(rows, tbit, max_frames=3, max_bits=64):
    """Reconstruieste fluxul de biti per frame (schita)."""
    by_frame = {}
    for _, fr, lvl, d in rows:
        by_frame.setdefault(fr, []).append((lvl, d))
    for fr in sorted(by_frame)[:max_frames]:
        bits = []
        for lvl, d in by_frame[fr]:
            k = max(1, round(d / tbit))
            bits.extend([lvl] * k)
        bs = "".join(str(b) for b in bits[:max_bits])
        print(f"  frame {fr}: {len(bits)} biti  {bs}{'...' if len(bits) > max_bits else ''}")
        # grupare in octeti (doua conventii)
        full = bits[:(len(bits) // 8) * 8]
        if full:
            lsb = " ".join(f"{sum(full[i+j] << j for j in range(8)):02X}"
                           for i in range(0, len(full), 8))
            msb = " ".join(f"{sum(full[i+j] << (7-j) for j in range(8)):02X}"
                           for i in range(0, len(full), 8))
            print(f"    octeti LSB-first: {lsb}")
            print(f"    octeti MSB-first: {msb}")


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else "."
    if os.path.isdir(arg):
        files = sorted(glob.glob(os.path.join(arg, "BSD_*.CSV")) +
                       glob.glob(os.path.join(arg, "BSD_*.csv")))
        if not files:
            sys.exit(f"Niciun fisier BSD_*.CSV in {arg}")
        path = files[-1]
    else:
        path = arg
    print(f"Fisier: {path}")

    rows = load(path)
    if not rows:
        sys.exit("Fara pulsuri valide in fisier.")
    durs = [r[3] for r in rows]
    frames = sorted({r[1] for r in rows})
    highs = [d for _, _, lvl, d in rows if lvl == 1]
    lows = [d for _, _, lvl, d in rows if lvl == 0]

    print(f"\nPulsuri: {len(rows)} | frame-uri: {len(frames)} | "
          f"durata min/med/max: {min(durs)}/{sum(durs)//len(durs)}/{max(durs)} us")

    print("\n== Histograma durate puls (toate) ==")
    ascii_hist(durs)

    tbit, cost = estimate_tbit(durs)
    if tbit:
        print(f"\n== TBIT estimat: ~{tbit} us  (reziduu mediu {cost:.1f} us) ==")
        mult = [d / tbit for d in durs]
        print("  distributia dur/TBIT (ar trebui sa se grupeze pe 1,2,3...):")
        ascii_hist(mult, bins=20, unit="xTBIT")

        print("\n== Reconstructie NAIVA biti (schita, verifica framing-ul) ==")
        naive_bits(rows, tbit)

    print("\n== Pulsuri per frame (primele 10) ==")
    fc = Counter(r[1] for r in rows)
    for fr in frames[:10]:
        print(f"  frame {fr}: {fc[fr]} pulsuri")

    print("\n== Top 10 durate (candidate biti/simboluri) ==")
    for d, n in Counter(durs).most_common(10):
        print(f"  {d:5d} us : {n}")

    print(f"\n(HIGH: {len(highs)} pulsuri, LOW: {len(lows)} pulsuri)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Ajusta perfis genéricos às medições CC0 de pickupAnalyseR-data.

Uso: python3 tools/fit_pickup_profiles.py /caminho/para/pickupAnalyseR-data
Somente biblioteca padrão; não baixa nem inclui os CSV no projeto.
"""

import csv
import math
import statistics
import sys
from pathlib import Path


SINGLE = (
    "Guitar/Squier/Affinity-Strat/Squier-LD.csv",
    "Guitar/DiMarzio/TrueVelvet/TV-LD.csv",
    "Guitar/Squier/Tele-CV/bridge-LD.csv",
)
HUMBUCKER = (
    "Guitar/Epiphone/LP/bridge-LD.csv",
    "Guitar/Gibson/498T/498THBWG-LD.csv",
    "Guitar/SD/TB4-JB/JB-LD.csv",
    "Guitar/IronGear/SteamHammer/SteamHammer-LD.csv",
)


def measurements(path):
    with path.open(newline="") as source:
        rows = csv.DictReader(source)
        data = [
            (f, float(row["Magnitude [dB]"]) - 20.0 * math.log10(f))
            for row in rows
            if 180.0 <= (f := float(row["Frequency [Hz]"])) <= 9000.0
            and float(row["Quality"]) > 0
        ]
    if len(data) < 50:
        raise ValueError(f"Medição curta ou inválida: {path}")
    return data[::3]


def fit(path):
    data = measurements(path)
    best = (float("inf"), 0, 0.0)
    for frequency in range(1600, 4601, 50):
        for q_steps in range(40, 161, 2):
            q = q_steps / 100.0
            residuals = [
                measured + 10.0 * math.log10(
                    (1.0 - (f / frequency) ** 2) ** 2 + (f / frequency / q) ** 2
                )
                for f, measured in data
            ]
            offset = statistics.fmean(residuals)
            error = statistics.fmean((value - offset) ** 2 for value in residuals)
            if error < best[0]:
                best = (error, frequency, q)
    return best[1], best[2], math.sqrt(best[0])


def main(root):
    for label, paths in (("single", SINGLE), ("humbucker", HUMBUCKER)):
        results = []
        print(label)
        for relative in paths:
            frequency, q, error = fit(root / relative)
            results.append((frequency, q))
            print(f"  {relative}: {frequency} Hz, Q={q:.2f}, erro={error:.2f} dB")
        print(
            f"  mediana: {statistics.median(value[0] for value in results):g} Hz, "
            f"Q={statistics.median(value[1] for value in results):.2f}"
        )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Uso: fit_pickup_profiles.py /caminho/para/pickupAnalyseR-data")
    main(Path(sys.argv[1]))

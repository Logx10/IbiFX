"""Cria IRs genéricas do IbiFX com filtros simples, sem áudio de terceiros.

Execute na raiz do projeto: python3 tools/generate_factory_irs.py
"""

import cmath
import math
from pathlib import Path
import struct
import wave


SAMPLE_RATE = 48_000
TAPS = 512
OUTPUT = Path("irs/Factory")


def impulse_response(high_pass_hz: float, low_pass_hz: tuple[float, float]) -> list[float]:
    dt = 1.0 / SAMPLE_RATE
    rc = 1.0 / (2.0 * math.pi * high_pass_hz)
    high_pass_alpha = rc / (rc + dt)
    low_pass_alpha = [1.0 - math.exp(-2.0 * math.pi * hz * dt) for hz in low_pass_hz]

    previous_input = 0.0
    high_pass_state = 0.0
    low_pass_states = [0.0, 0.0]
    taps = []

    for n in range(TAPS):
        sample = 1.0 if n == 0 else 0.0
        high_pass_state = high_pass_alpha * (high_pass_state + sample - previous_input)
        previous_input = sample
        filtered = high_pass_state
        for stage, alpha in enumerate(low_pass_alpha):
            low_pass_states[stage] += alpha * (filtered - low_pass_states[stage])
            filtered = low_pass_states[stage]
        taps.append(filtered)

    # O cabinet não deve alterar arbitrariamente o nível de referência em 1 kHz.
    response_at_1k = sum(
        tap * cmath.exp(-2j * math.pi * 1000.0 * n / SAMPLE_RATE)
        for n, tap in enumerate(taps)
    )
    return [tap / abs(response_at_1k) for tap in taps]


def write_ir(name: str, high_pass_hz: float, low_pass_hz: tuple[float, float]) -> None:
    taps = impulse_response(high_pass_hz, low_pass_hz)
    with wave.open(str(OUTPUT / name), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(b"".join(struct.pack("<h", round(max(-1.0, min(1.0, x)) * 32767)) for x in taps))


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    write_ir("Open 1x12.wav", 70.0, (5500.0, 8000.0))
    write_ir("Brit 4x12.wav", 90.0, (4200.0, 6500.0))


if __name__ == "__main__":
    main()

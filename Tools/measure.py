#!/usr/bin/env python3
"""Arks Sonic 22 - measurement scaffold (Phase 2).

Offline analysis of rendered WAV files, used from Phase 3 on to characterise the model and (when
real-amp recordings exist) to compare against the hardware. Nothing here is on the audio path.

Implemented now (baseline, usable):
  - THD and harmonic spectrum of a steady sine render
  - magnitude frequency response from an impulse render (or a log sweep + inverse filter)
  - aliasing check: energy at non-harmonic frequencies for a sine input

Planned (Phase 9): time/level alignment against a hardware recording before comparing frequency
response, dynamics and harmonic structure (never a raw sample diff). See docs/known_unknowns.md.

Usage examples:
  python measure.py thd   out_sine.wav --f0 110
  python measure.py ir    out_impulse.wav
  python measure.py alias out_sine.wav --f0 220

Requires numpy. WAV I/O uses the standard library.
"""
import argparse
import sys
import wave

try:
    import numpy as np
except ImportError:
    sys.exit("numpy is required: pip install numpy")


def read_wav(path):
    """Return (samples float32 [-1,1], sample_rate). Mixes to mono."""
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        n = w.getnframes()
        width = w.getsampwidth()
        raw = w.readframes(n)
        ch = w.getnchannels()
    dtype = {1: np.int8, 2: np.int16, 4: np.int32}.get(width)
    if dtype is None:
        sys.exit(f"unsupported sample width: {width} bytes")
    data = np.frombuffer(raw, dtype=dtype).astype(np.float64)
    data /= float(np.iinfo(dtype).max)
    if ch > 1:
        data = data.reshape(-1, ch).mean(axis=1)
    return data, sr


def blackman_harris(n):
    i = np.arange(n)
    t = 2.0 * np.pi * i / (n - 1)
    return 0.35875 - 0.48829 * np.cos(t) + 0.14128 * np.cos(2 * t) - 0.01168 * np.cos(3 * t)


def dft_at(x, hz, sr):
    """Amplitude of the component at exactly `hz` (windowed goertzel-style DFT)."""
    n = len(x)
    w = blackman_harris(n)
    xw = x * w
    k = 2.0 * np.pi * hz / sr * np.arange(n)
    re = np.sum(xw * np.cos(k))
    im = -np.sum(xw * np.sin(k))
    return 2.0 * np.hypot(re, im) / np.sum(w)


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-20))


def cmd_thd(args):
    x, sr = read_wav(args.wav)
    x = x[-(1 << 16):] if len(x) > (1 << 16) else x
    f0 = args.f0
    fund = dft_at(x, f0, sr)
    harmonics = []
    k = 2
    while k * f0 < sr / 2:
        harmonics.append(dft_at(x, k * f0, sr))
        k += 1
    thd = np.sqrt(np.sum(np.square(harmonics))) / max(fund, 1e-20)
    print(f"f0 = {f0:.1f} Hz, fundamental {db(fund):.1f} dB")
    for i, h in enumerate(harmonics[:8], start=2):
        print(f"  harmonic {i:2d} ({i*f0:7.1f} Hz): {db(h / fund):6.1f} dB re fundamental")
    print(f"THD = {100.0 * thd:.3f} %  ({db(thd):.1f} dB)")


def cmd_ir(args):
    x, sr = read_wav(args.wav)
    n = 1 << int(np.ceil(np.log2(len(x))))
    spec = np.fft.rfft(x, n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    mag = db(np.abs(spec))
    mag -= np.max(mag)
    print(f"sample rate {sr} Hz, {len(x)} samples")
    for f in [40, 80, 160, 320, 640, 1280, 2560, 5120, 10240]:
        if f < sr / 2:
            idx = int(np.argmin(np.abs(freqs - f)))
            print(f"  {f:6d} Hz: {mag[idx]:6.1f} dB (re peak)")


def cmd_alias(args):
    x, sr = read_wav(args.wav)
    x = x[-(1 << 16):] if len(x) > (1 << 16) else x
    f0 = args.f0
    n = len(x)
    w = blackman_harris(n)
    spec = np.abs(np.fft.rfft(x * w)) * 2.0 / np.sum(w)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    binhz = freqs[1]
    nearest = np.round(freqs / f0) * f0
    off_harmonic = np.abs(freqs - nearest) > 8 * binhz
    harmonic = ~off_harmonic
    e_off = np.sqrt(np.sum(np.square(spec[off_harmonic])))
    e_harm = np.sqrt(np.sum(np.square(spec[harmonic])))
    print(f"f0 = {f0:.1f} Hz")
    print(f"  non-harmonic / aliasing energy: {db(e_off / max(e_harm, 1e-20)):.1f} dB re harmonic energy")


def main():
    p = argparse.ArgumentParser(description="Arks Sonic 22 measurement scaffold")
    sub = p.add_subparsers(dest="cmd", required=True)

    t = sub.add_parser("thd", help="THD + harmonic spectrum of a sine render")
    t.add_argument("wav"); t.add_argument("--f0", type=float, required=True)
    t.set_defaults(func=cmd_thd)

    i = sub.add_parser("ir", help="magnitude frequency response from an impulse render")
    i.add_argument("wav")
    i.set_defaults(func=cmd_ir)

    a = sub.add_parser("alias", help="non-harmonic / aliasing energy for a sine render")
    a.add_argument("wav"); a.add_argument("--f0", type=float, required=True)
    a.set_defaults(func=cmd_alias)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

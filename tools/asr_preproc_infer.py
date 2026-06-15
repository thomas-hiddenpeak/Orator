#!/usr/bin/env python3
import argparse
import os
import tempfile
import wave

import numpy as np
from modelscope.pipelines import pipeline
from modelscope.utils.constant import Tasks


_FRCRN_CACHE = {}


def _load_frcrn(model_name: str):
    key = model_name.strip() if model_name else "damo/speech_frcrn_ans_cirm_16k"
    if key not in _FRCRN_CACHE:
        _FRCRN_CACHE[key] = pipeline(
            task=Tasks.acoustic_noise_suppression,
            model=key,
            trust_remote_code=True,
        )
    return _FRCRN_CACHE[key]


def _write_wav(samples: np.ndarray, sample_rate: int) -> str:
    fd, path = tempfile.mkstemp(prefix="orator_preproc_", suffix=".wav")
    os.close(fd)
    pcm16 = np.clip(samples, -1.0, 1.0)
    pcm16 = np.round(pcm16 * 32767.0).astype(np.int16)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm16.tobytes())
    return path


def _run_frcrn(samples: np.ndarray, sample_rate: int, model_name: str) -> np.ndarray:
    p = _load_frcrn(model_name)
    wav_path = _write_wav(samples, sample_rate)
    try:
        out = p(wav_path)
    finally:
        try:
            os.remove(wav_path)
        except OSError:
            pass
    if not isinstance(out, dict) or "output_pcm" not in out:
        raise RuntimeError("FRCRN output format unexpected")
    enhanced_i16 = np.frombuffer(out["output_pcm"], dtype=np.int16)
    enhanced = (enhanced_i16.astype(np.float32) / 32768.0).astype(np.float32)
    return enhanced


def main() -> int:
    parser = argparse.ArgumentParser(description="ASR preprocessor model inference")
    parser.add_argument("--mode", required=True, choices=["frcrn"])
    parser.add_argument("--in-f32", required=True)
    parser.add_argument("--out-f32", required=True)
    parser.add_argument("--sample-rate", type=int, default=16000)
    parser.add_argument("--frcrn-model", default="damo/speech_frcrn_ans_cirm_16k")
    args = parser.parse_args()

    x = np.fromfile(args.in_f32, dtype=np.float32)
    if x.size == 0:
        np.array([], dtype=np.float32).tofile(args.out_f32)
        return 0

    if args.mode == "frcrn":
        y = _run_frcrn(x, args.sample_rate, args.frcrn_model)
    else:
        raise RuntimeError(f"unsupported mode: {args.mode}")

    # Keep strict shape contract for C++ caller.
    if y.size < x.size:
        y = np.pad(y, (0, x.size - y.size), mode="constant")
    elif y.size > x.size:
        y = y[: x.size]

    y.astype(np.float32).tofile(args.out_f32)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run NeMo streaming Sortformer and dump per-frame speaker probabilities.

The oracle can consume audio or the exact processed-signal fixture used by the
C++ numerical tests. With ``--config``, it applies the checked TOML streaming
profile before inference so asynchronous FIFO behavior is compared directly.
Runs only in the isolated tools/.venv-nemo environment.

Usage:
  tools/.venv-nemo/bin/python tools/reference/nemo_sortformer_ref.py \
      [--nemo models/diar_streaming_sortformer_4spk-v2.1.nemo] \
      [--audio test/data/audio/test.mp3] [--out /tmp/nemo_probs.npy]
"""
import argparse
import datetime
import hashlib
import json
import os
import struct
import subprocess
import sys
import tomllib

import numpy as np
import torch

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
STREAMING_MAPPING = {
    "spkcache_len": "spkcache_len",
    "fifo_len": "fifo_len",
    "chunk_len": "chunk_len",
    "spkcache_update_period": "spkcache_update_period",
    "chunk_left_context": "chunk_left_context",
    "chunk_right_context": "chunk_right_context",
    "spkcache_sil_frames": "spkcache_sil_frames_per_spk",
}


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_record(path):
    return {
        "path": os.path.abspath(path),
        "sha256": sha256_file(path),
        "bytes": os.path.getsize(path),
    }


def streaming_profile(model):
    modules = model.sortformer_modules
    return {
        source_name: int(getattr(modules, target_name))
        for source_name, target_name in STREAMING_MAPPING.items()
    } | {"async_streaming": bool(model.async_streaming)}


def apply_streaming_config(model, path):
    if not path:
        return {}
    with open(path, "rb") as source:
        config = tomllib.load(source)
    diar = config["diarizer"]
    modules = model.sortformer_modules
    applied = {}
    for source_name, target_name in STREAMING_MAPPING.items():
        if source_name not in diar:
            continue
        value = int(diar[source_name])
        setattr(modules, target_name, value)
        applied[source_name] = value
    model.async_streaming = int(diar.get("fifo_len", 0)) > 0
    modules._check_streaming_parameters()
    applied["async_streaming"] = model.async_streaming
    return applied


def processed_signal(path, meta_path, repeat):
    with open(meta_path, "rb") as source:
        raw = source.read(12)
    if len(raw) != 12:
        raise ValueError("processed-signal metadata must contain 3 int32 values")
    t_mel, valid_mel, _ = struct.unpack("<iii", raw)
    values = np.fromfile(path, dtype="<f4")
    expected = 128 * t_mel
    if values.size != expected:
        raise ValueError(
            f"processed signal has {values.size} values, expected {expected}")
    if repeat < 1:
        raise ValueError("--processed-repeat must be at least 1")
    signal_values = values.reshape(1, 128, t_mel)
    if repeat > 1:
        valid = np.tile(signal_values[:, :, :valid_mel], (1, 1, repeat))
        padding = signal_values[:, :, valid_mel:]
        signal_values = np.concatenate((valid, padding), axis=2)
        valid_mel *= repeat
    signal = torch.from_numpy(signal_values.copy())
    length = torch.tensor([valid_mel], dtype=torch.long)
    return signal, length


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nemo", default=os.path.join(
        ROOT, "models", "diar_streaming_sortformer_4spk-v2.1.nemo"))
    ap.add_argument("--audio", default=os.path.join(
        ROOT, "test", "data", "audio", "test.mp3"))
    ap.add_argument("--config")
    ap.add_argument("--processed-signal")
    ap.add_argument("--processed-meta")
    ap.add_argument("--processed-repeat", type=int, default=1)
    ap.add_argument("--out", default="/tmp/nemo_probs.npy")
    ap.add_argument("--raw-out")
    ap.add_argument("--runtime-weights")
    ap.add_argument("--source-revision")
    ap.add_argument("--source-url")
    ap.add_argument("--manifest-out")
    args = ap.parse_args()

    import nemo
    from nemo.collections.asr.models import SortformerEncLabelModel
    m = SortformerEncLabelModel.restore_from(args.nemo, map_location="cpu")
    m.eval()
    m._cfg.streaming_mode = True
    if hasattr(m, "streaming_mode"):
        m.streaming_mode = True
    checkpoint_profile = streaming_profile(m)
    applied = apply_streaming_config(m, args.config)
    torch.set_grad_enabled(False)

    if bool(args.processed_signal) != bool(args.processed_meta):
        raise ValueError(
            "--processed-signal and --processed-meta must be used together")
    if args.processed_signal:
        signal, length = processed_signal(
            args.processed_signal, args.processed_meta,
            args.processed_repeat)
        probs = m.forward_streaming(signal, length)[0].cpu().numpy()
    elif args.processed_repeat != 1:
        raise ValueError("--processed-repeat requires --processed-signal")
    else:
        wav = "/tmp/_nemo_ref_16k.wav"
        subprocess.run([
            "ffmpeg", "-v", "quiet", "-y", "-i", args.audio,
            "-ar", "16000", "-ac", "1", wav,
        ], check=True)
        out = m.diarize(
            audio=[wav], batch_size=1, include_tensor_outputs=True,
            verbose=False)
        probs = out[1][0].cpu().numpy()

    with open(args.out, "wb") as output:
        np.save(output, probs)
    if args.raw_out:
        probs.astype("<f4").tofile(args.raw_out)
    print("streaming profile:", applied)
    print("nemo probs shape:", probs.shape, "-> saved", args.out)
    if args.raw_out:
        print("raw probabilities ->", args.raw_out)
    if args.manifest_out:
        sources = {
            "nemo_checkpoint": file_record(args.nemo),
            "config": file_record(args.config) if args.config else None,
            "runtime_weights": (
                file_record(args.runtime_weights)
                if args.runtime_weights else None),
            "processed_signal": (
                file_record(args.processed_signal)
                if args.processed_signal else None),
            "processed_metadata": (
                file_record(args.processed_meta)
                if args.processed_meta else None),
            "audio": (
                file_record(args.audio)
                if not args.processed_signal else None),
            "script": file_record(__file__),
        }
        outputs = {"numpy": file_record(args.out)}
        if args.raw_out:
            outputs["raw_f32"] = file_record(args.raw_out)
        manifest = {
            "schema_version": 1,
            "kind": "orator_nemo_sortformer_oracle",
            "generated_utc": datetime.datetime.now(
                datetime.timezone.utc).isoformat(),
            "source_revision": args.source_revision,
            "source_url": args.source_url,
            "checkpoint_profile": checkpoint_profile,
            "applied_profile": streaming_profile(m),
            "processed_repeat": args.processed_repeat,
            "output_shape": list(probs.shape),
            "output_dtype": str(probs.dtype),
            "sources": sources,
            "outputs": outputs,
            "versions": {
                "python": sys.version.split()[0],
                "torch": torch.__version__,
                "nemo": getattr(nemo, "__version__", "unknown"),
                "numpy": np.__version__,
            },
            "argv": sys.argv,
        }
        with open(args.manifest_out, "w", encoding="utf-8") as output:
            json.dump(manifest, output, ensure_ascii=False, indent=2)
            output.write("\n")
        print("oracle manifest ->", args.manifest_out)


if __name__ == "__main__":
    main()

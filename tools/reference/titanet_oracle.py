#!/usr/bin/env python3
"""Reference oracle for the TitaNet-Large speaker embedder (CPU PyTorch / NeMo).

Runs the canonical NeMo `EncDecSpeakerLabelModel` (titanet_large) on CPU over a
few fixed spans of the project test audio and dumps each forward stage so the
C++/CUDA `model::TitaNetEmbedder` port can be validated numerically (Art. II):
  waveform (f32 16 kHz mono)  -> the exact input the C++ test must feed
  mel (preprocessor output)   -> [features, frames]
  encoder output              -> [3072, frames]
  embedding                   -> [192]  (the acceptance target; L2-normalized
                                          cosine between C++ and oracle ~ 1.0)

This oracle requires NeMo, which is heavy and lives in a SEPARATE venv
(tools/.venv-nemo) so it never pollutes the runtime tool venv. It is an offline
one-shot: the dumps are committed/regenerated, the C++ runtime never imports it.

Usage:
  tools/.venv-nemo/bin/python tools/reference/titanet_oracle.py \
      [--nemo models/speaker/speakerverification_en_titanet_large.nemo] \
      [--audio test/data/audio/test.mp3] \
      [--spans 0:3,30:33,60:63]
Writes JSON + raw float32 tensors under models/reference/speaker/.
"""

import argparse
import json
import os
import subprocess

import numpy as np
import torch

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_NEMO = os.path.join(
    ROOT, "models", "speaker", "speakerverification_en_titanet_large.nemo")
DEFAULT_AUDIO = os.path.join(ROOT, "test", "data", "audio", "test.mp3")
OUT_DIR = os.path.join(ROOT, "models", "reference", "speaker")


def load_audio(path, sr=16000, start=0.0, end=None):
    cmd = ["ffmpeg", "-v", "quiet", "-i", path, "-ar", str(sr), "-ac", "1",
           "-ss", str(start)]
    if end is not None:
        cmd += ["-t", str(end - start)]
    cmd += ["-f", "f32le", "-"]
    raw = subprocess.run(cmd, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def write_f32(name, arr):
    arr = np.ascontiguousarray(arr, dtype=np.float32)
    with open(os.path.join(OUT_DIR, name), "wb") as f:
        f.write(arr.tobytes())
    return list(arr.shape)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nemo", default=DEFAULT_NEMO)
    ap.add_argument("--audio", default=DEFAULT_AUDIO)
    ap.add_argument("--spans", default="0:3,30:33,60:63",
                    help="comma list of start:end seconds")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    from nemo.collections.asr.models import EncDecSpeakerLabelModel

    model = EncDecSpeakerLabelModel.restore_from(args.nemo, map_location="cpu")
    model.eval()
    torch.set_grad_enabled(False)

    spans = []
    for tok in args.spans.split(","):
        a, b = tok.split(":")
        spans.append((float(a), float(b)))

    index = {"model": os.path.basename(args.nemo), "sr": 16000,
             "embedding_dim": 192, "spans": []}
    embeddings = []
    for i, (s, e) in enumerate(spans):
        wav = load_audio(args.audio, 16000, s, e)
        sig = torch.from_numpy(wav).unsqueeze(0)            # [1, T]
        sig_len = torch.tensor([wav.shape[0]])

        mel, mel_len = model.preprocessor(
            input_signal=sig, length=sig_len)               # [1, 80, F]
        enc, enc_len = model.encoder(
            audio_signal=mel, length=mel_len)               # [1, 3072, F]
        logits, emb = model.decoder(
            encoder_output=enc, length=enc_len)             # emb [1, 192]
        emb = emb.squeeze(0).cpu().numpy()
        emb_n = emb / (np.linalg.norm(emb) + 1e-12)
        embeddings.append(emb_n)

        pfx = f"span{i}"
        wav_shape = write_f32(f"{pfx}_wave.f32", wav)
        mel_shape = write_f32(f"{pfx}_mel.f32", mel.squeeze(0).cpu().numpy())
        enc_shape = write_f32(f"{pfx}_enc.f32", enc.squeeze(0).cpu().numpy())
        emb_shape = write_f32(f"{pfx}_emb.f32", emb_n)
        index["spans"].append({
            "i": i, "start": s, "end": e,
            "wave": {"file": f"{pfx}_wave.f32", "shape": wav_shape},
            "mel": {"file": f"{pfx}_mel.f32", "shape": mel_shape},
            "enc": {"file": f"{pfx}_enc.f32", "shape": enc_shape},
            "emb": {"file": f"{pfx}_emb.f32", "shape": emb_shape},
        })
        print(f"span{i} [{s},{e}]s  wav{wav_shape} mel{mel_shape} "
              f"enc{enc_shape} emb{emb_shape}")

    # Cross-span cosine matrix: same-speaker spans should score high, different
    # speakers low -- a sanity check on the reference embeddings themselves.
    n = len(embeddings)
    cos = [[float(np.dot(embeddings[a], embeddings[b]))
            for b in range(n)] for a in range(n)]
    index["cosine_matrix"] = cos
    with open(os.path.join(OUT_DIR, "titanet_oracle.json"), "w") as f:
        json.dump(index, f, indent=2)
    print("cosine matrix:")
    for row in cos:
        print("  " + "  ".join(f"{v:+.4f}" for v in row))
    print("wrote", os.path.join(OUT_DIR, "titanet_oracle.json"))


if __name__ == "__main__":
    main()

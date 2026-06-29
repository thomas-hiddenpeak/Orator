#!/usr/bin/env python3
"""Reference: run NeMo's own streaming Sortformer on an audio file and dump
per-frame speaker probabilities. Used to validate the C++ port at full length —
the per-window diarization ceiling (see speaker_attrib_eval.py --windows / the
nemo_window analysis) must track NeMo's, proving any long-session degradation is
the audio/model, not the port. Runs in the isolated tools/.venv-nemo.

Usage:
  tools/.venv-nemo/bin/python tools/reference/nemo_sortformer_ref.py \
      [--nemo models/diar_streaming_sortformer_4spk-v2.1.nemo] \
      [--audio test/data/audio/test.mp3] [--out /tmp/nemo_probs.npy]
"""
import argparse, os, subprocess
import numpy as np
import torch

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nemo", default=os.path.join(
        ROOT, "models", "diar_streaming_sortformer_4spk-v2.1.nemo"))
    ap.add_argument("--audio", default=os.path.join(
        ROOT, "test", "data", "audio", "test.mp3"))
    ap.add_argument("--out", default="/tmp/nemo_probs.npy")
    args = ap.parse_args()

    wav = "/tmp/_nemo_ref_16k.wav"
    subprocess.run(["ffmpeg", "-v", "quiet", "-y", "-i", args.audio,
                    "-ar", "16000", "-ac", "1", wav], check=True)

    from nemo.collections.asr.models import SortformerEncLabelModel
    m = SortformerEncLabelModel.restore_from(args.nemo, map_location="cpu")
    m.eval()
    m._cfg.streaming_mode = True
    if hasattr(m, "streaming_mode"):
        m.streaming_mode = True
    torch.set_grad_enabled(False)
    out = m.diarize(audio=[wav], batch_size=1, include_tensor_outputs=True,
                    verbose=False)
    probs = out[1][0].cpu().numpy()  # [T, n_spk] frame probs @ 0.08 s
    np.save(args.out, probs)
    print("nemo probs shape:", probs.shape, "-> saved", args.out)


if __name__ == "__main__":
    main()

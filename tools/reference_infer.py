#!/usr/bin/env python3
"""Reference Sortformer inference using the REAL .nemo via vendored NeMo.

Purpose: produce authentic per-frame speaker-activity output from the actual
pretrained model, to (a) compare manually against asrTest2Final.txt and
(b) serve as the numerical oracle for the C++/CUDA forward implementation.

This script patches a few NeMo *training-infrastructure* import gaps (telemetry
loggers that differ across lightning versions) that are irrelevant to inference.
"""
import os
import sys

NEMO_DIR = os.path.join(os.path.dirname(__file__), "..", "third_party",
                        "streaming_sortformer", "NeMo")
sys.path.insert(0, os.path.abspath(NEMO_DIR))


def patch_training_infra():
    # lightning loggers vary by version; NeMo's exp_manager imports names that
    # may not exist. Alias missing ones to an existing logger (inference unused).
    import lightning.pytorch.loggers as L
    for missing in ["NeptuneLogger"]:
        if not hasattr(L, missing):
            setattr(L, missing, getattr(L, "CSVLogger"))


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--nemo", default=os.path.join(
        os.path.dirname(__file__), "..", "third_party", "streaming_sortformer",
        "diar_streaming_sortformer_4spk-v2",
        "diar_streaming_sortformer_4spk-v2.nemo.bin"))
    ap.add_argument("--audio", default=os.path.join(
        os.path.dirname(__file__), "..", "test.mp3"))
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(__file__), "..", "models", "reference_diar.npz"))
    ap.add_argument("--device", default="cuda")
    args = ap.parse_args()

    patch_training_infra()

    import numpy as np
    import torch
    from nemo.collections.asr.models import SortformerEncLabelModel

    print(">> loading model from", args.nemo)
    model = SortformerEncLabelModel.restore_from(
        restore_path=args.nemo, map_location=args.device)
    model.eval()
    print(">> model loaded:", type(model).__name__)

    print(">> diarizing:", args.audio)
    with torch.inference_mode():
        segments, tensors = model.diarize(
            audio=args.audio,
            batch_size=1,
            include_tensor_outputs=True,
            num_workers=0,
            verbose=True,
        )

    # segments: List[List["begin end spk"]] for each audio file (we passed one).
    segs = segments[0] if segments else []
    probs = tensors[0] if tensors else None
    print(">> num segments:", len(segs))
    if probs is not None:
        probs = probs.detach().float().cpu().numpy()
        print(">> probs shape:", probs.shape)

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    # Save raw probabilities + parsed segments.
    seg_rows = []
    for s in segs:
        # entries are strings "begin end speaker" or lists
        if isinstance(s, str):
            parts = s.split()
        else:
            parts = list(s)
        b = float(parts[0]); e = float(parts[1])
        spk_tok = str(parts[2])
        spk = int("".join(ch for ch in spk_tok if ch.isdigit()) or "0")
        seg_rows.append((b, e, spk))
    np.savez(args.out,
             probs=probs if probs is not None else np.zeros((0,)),
             segments=np.array(seg_rows, dtype=np.float64) if seg_rows
             else np.zeros((0, 3)))
    print(">> saved", args.out)

    # Print a human-readable segment summary.
    txt = os.path.splitext(args.out)[0] + ".segments.txt"
    with open(txt, "w") as f:
        for b, e, spk in seg_rows:
            f.write(f"{b:8.2f} {e:8.2f} spk{spk}\n")
    print(">> wrote", txt)
    print(">> done")


if __name__ == "__main__":
    main()

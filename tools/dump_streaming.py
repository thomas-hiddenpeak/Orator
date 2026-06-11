#!/usr/bin/env python3
"""Dump NeMo Sortformer *streaming* oracle for C++ verification.

Runs the REAL model's streaming forward (forward_streaming) on an audio slice
long enough to exercise multiple chunks + speaker-cache compression, and saves:
  processed_signal   -> [B,128,T_mel]  (the exact mel fed to the encoder)
  total_preds        -> [B,T_diar,4]   (final streaming sigmoid output)
  offline_preds      -> [B,T_diar,4]   (offline path for reference/contrast)
Also monkey-patches forward_streaming_step to record per-chunk diagnostics
(chunk_preds, spkcache/fifo lengths) into ref_streaming.npz for debugging.

TF32 is DISABLED so the reference is full fp32 (matches our fp32 CUDA kernels).
"""
import os
import sys

NEMO_DIR = os.path.join(os.path.dirname(__file__), "..", "third_party",
                        "streaming_sortformer", "NeMo")
sys.path.insert(0, os.path.abspath(NEMO_DIR))


def patch_training_infra():
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
    ap.add_argument("--seconds", type=float, default=40.0)
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(__file__), "..", "models", "ref_streaming.npz"))
    ap.add_argument("--device", default="cuda")
    args = ap.parse_args()

    patch_training_infra()
    import numpy as np
    import torch
    torch.backends.cudnn.allow_tf32 = False
    torch.backends.cuda.matmul.allow_tf32 = False
    from nemo.collections.asr.models import SortformerEncLabelModel

    md = np.load(os.path.join(os.path.dirname(args.out), "ref_mel_10s.npz"))
    wav = md["wav"].astype(np.float32)
    # ref_mel_10s.npz only holds 10s; if we need more, tile is wrong. Instead
    # load the full librosa wav if present, else use what we have.
    full_path = os.path.join(os.path.dirname(args.out), "ref_wav_full.npy")
    if os.path.exists(full_path):
        wav = np.load(full_path).astype(np.float32)
        print(">> using full wav:", wav.shape)
    n = int(args.seconds * 16000)
    wav = wav[:n]
    print(">> input samples:", wav.shape, "=", wav.shape[0] / 16000.0, "s")

    model = SortformerEncLabelModel.restore_from(
        restore_path=args.nemo, map_location=args.device)
    model.eval()
    dev = args.device
    model.async_streaming = False

    sig = torch.tensor(wav, device=dev).unsqueeze(0)
    sig_len = torch.tensor([wav.shape[0]], device=dev)

    # Per-chunk diagnostics via monkey-patch.
    chunk_diag = []
    orig_step = model.forward_streaming_step

    def patched_step(processed_signal, processed_signal_length, streaming_state,
                     total_preds, drop_extra_pre_encoded=0, left_offset=0,
                     right_offset=0):
        ss, tp = orig_step(processed_signal, processed_signal_length,
                           streaming_state, total_preds,
                           drop_extra_pre_encoded, left_offset, right_offset)
        chunk_diag.append({
            "spkcache_len": int(ss.spkcache.shape[1]),
            "fifo_len": int(ss.fifo.shape[1]),
            "total_len": int(tp.shape[1]),
            "left_offset": int(left_offset),
            "right_offset": int(right_offset),
        })
        return ss, tp

    model.forward_streaming_step = patched_step

    acts = {}
    with torch.inference_mode():
        proc, proc_len = model.process_signal(sig, sig_len)
        acts["processed_signal"] = proc.detach().float().cpu().numpy()
        print(">> processed_signal:", acts["processed_signal"].shape,
              "len", int(proc_len.item()))

        total_preds = model.forward_streaming(proc, proc_len)
        acts["total_preds"] = total_preds.detach().float().cpu().numpy()
        print(">> streaming total_preds:", acts["total_preds"].shape)

        # Offline path for contrast (may exceed attn limits on long audio; guard).
        try:
            emb_seq, emb_len = model.frontend_encoder(proc, proc_len)
            off = model.forward_infer(emb_seq, emb_len)
            acts["offline_preds"] = off.detach().float().cpu().numpy()
            print(">> offline preds:", acts["offline_preds"].shape)
        except Exception as e:  # noqa
            print(">> offline path skipped:", e)

    print(">> per-chunk diagnostics:")
    for i, d in enumerate(chunk_diag):
        print(f"   chunk {i}: spkcache={d['spkcache_len']} fifo={d['fifo_len']} "
              f"total={d['total_len']} lc={d['left_offset']} rc={d['right_offset']}")
    acts["num_chunks"] = np.array([len(chunk_diag)])
    acts["chunk_spkcache_len"] = np.array([d["spkcache_len"] for d in chunk_diag])
    acts["chunk_total_len"] = np.array([d["total_len"] for d in chunk_diag])
    acts["chunk_left_offset"] = np.array([d["left_offset"] for d in chunk_diag])
    acts["chunk_right_offset"] = np.array([d["right_offset"] for d in chunk_diag])

    np.savez(args.out, **acts)
    print(">> saved", args.out)


if __name__ == "__main__":
    main()

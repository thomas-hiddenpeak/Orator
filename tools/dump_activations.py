#!/usr/bin/env python3
"""Dump NeMo Sortformer intermediate activations as numerical oracles.

Runs the REAL model's *offline* forward path on a short audio slice and saves
stage-boundary tensors so the C++/CUDA forward can be verified stage by stage:
  processed_signal (mel)  -> [B,128,T]
  pre_encode out          -> [B,T',512]
  encoder out (pre-proj)  -> [B,T',512]
  encoder_proj out        -> [B,T',192]
  transformer out         -> [B,T',192]
  preds (sigmoid)         -> [B,T',4]
Also dumps the first conformer layer's input/output and the first transformer
layer's output for finer-grained debugging.
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
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(__file__), "..", "models", "reference", "ref_activations.npz"))
    ap.add_argument("--device", default="cuda")
    args = ap.parse_args()

    patch_training_infra()
    import numpy as np
    import torch
    # Disable TF32 so the reference uses full fp32 (matches our fp32 CUDA kernels;
    # otherwise cuDNN/cuBLAS TF32 introduces ~1e-3 relative error on Ampere+).
    torch.backends.cudnn.allow_tf32 = False
    torch.backends.cuda.matmul.allow_tf32 = False
    from nemo.collections.asr.models import SortformerEncLabelModel

    # Reuse the exact librosa samples already saved for mel verification so the
    # whole oracle is consistent with ref_mel_10s.npz.
    md = np.load(os.path.join(os.path.dirname(args.out), "ref_mel_10s.npz"))
    wav = md["wav"].astype(np.float32)
    n = int(args.seconds * 16000)
    wav = wav[:n]
    print(">> input samples:", wav.shape)

    model = SortformerEncLabelModel.restore_from(
        restore_path=args.nemo, map_location=args.device)
    model.eval()
    dev = args.device

    sig = torch.tensor(wav, device=dev).unsqueeze(0)
    sig_len = torch.tensor([wav.shape[0]], device=dev)

    acts = {}
    hooks = []

    def save(name):
        def hook(mod, inp, out):
            o = out[0] if isinstance(out, tuple) else out
            if isinstance(o, torch.Tensor):
                acts[name] = o.detach().float().cpu().numpy()
        return hook

    enc = model.encoder
    hooks.append(enc.pre_encode.register_forward_hook(save("pre_encode")))
    # first and last conformer layers
    hooks.append(enc.layers[0].register_forward_hook(save("conformer_l0")))
    hooks.append(enc.layers[-1].register_forward_hook(save("conformer_l16")))
    tenc = model.transformer_encoder
    # transformer layers container name varies; try common ones.
    tlayers = None
    for attr in ["layers", "layer", "blocks"]:
        if hasattr(tenc, attr):
            tlayers = getattr(tenc, attr)
            break
    if tlayers is not None and len(tlayers) > 0:
        hooks.append(tlayers[0].register_forward_hook(save("trans_l0")))
        hooks.append(tlayers[-1].register_forward_hook(save("trans_l17")))

    with torch.inference_mode():
        proc, proc_len = model.process_signal(sig, sig_len)
        acts["processed_signal"] = proc.detach().float().cpu().numpy()
        print(">> processed_signal:", acts["processed_signal"].shape,
              "len", int(proc_len.item()))
        emb_seq, emb_len = model.frontend_encoder(proc, proc_len)
        acts["encoder_proj"] = emb_seq.detach().float().cpu().numpy()
        print(">> encoder_proj:", acts["encoder_proj"].shape)
        preds = model.forward_infer(emb_seq, emb_len)
        acts["preds"] = preds.detach().float().cpu().numpy()
        acts["emb_len"] = np.array([int(emb_len.item())])
        print(">> preds:", acts["preds"].shape)

    for h in hooks:
        h.remove()

    for k, v in acts.items():
        if hasattr(v, "shape"):
            print(f"   {k}: {v.shape}")
    np.savez(args.out, **acts)
    print(">> saved", args.out)


if __name__ == "__main__":
    main()

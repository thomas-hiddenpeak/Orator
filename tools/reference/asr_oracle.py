#!/usr/bin/env python3
"""PyTorch reference oracle for Qwen3-ASR-1.7B.

Runs the official reference modeling (cloned to /tmp/Qwen3-ASR) on the local
weights to produce (a) a ground-truth transcript for a clip of test.mp3 and
(b) a place to dump intermediate activations for verifying the C++/CUDA engine
stage by stage. This is the authoritative oracle: the framework Qwen3-ASR
actually runs in, on the GPU.

Usage:
  source tools/torchenv.sh
  python tools/asr_oracle.py --start 0 --dur 20

Requires the reference code at /tmp/Qwen3-ASR (git clone QwenLM/Qwen3-ASR).
"""
import argparse
import os
import sys

import numpy as np
import torch

REF = "/tmp/Qwen3-ASR/qwen_asr/core/transformers_backend"
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models", "asr", "Qwen", "Qwen3-ASR-1.7B")


def register():
    """Register the custom Qwen3-ASR classes with transformers Auto* APIs.

    The reference files use intra-package relative imports; flatten them into a
    temp dir so they import standalone without pulling qwen_asr's heavy deps.
    """
    import re
    import tempfile
    tmp = tempfile.mkdtemp(prefix="qwen3asr_ref_")
    for fn in ("configuration_qwen3_asr.py", "modeling_qwen3_asr.py",
               "processing_qwen3_asr.py"):
        src = open(os.path.join(REF, fn)).read()
        src = re.sub(r"from \.(\w+) import", r"from \1 import", src)
        src = re.sub(r"from \.(\w+) import \(", r"from \1 import (", src)
        open(os.path.join(tmp, fn), "w").write(src)
    sys.path.insert(0, tmp)
    import configuration_qwen3_asr as cfg
    import modeling_qwen3_asr as mdl
    import processing_qwen3_asr as proc
    from transformers import AutoConfig, AutoModel, AutoProcessor

    AutoConfig.register("qwen3_asr", cfg.Qwen3ASRConfig)
    try:
        AutoConfig.register("qwen3_asr_audio_encoder", cfg.Qwen3ASRAudioEncoderConfig)
    except Exception:
        pass
    AutoModel.register(cfg.Qwen3ASRConfig, mdl.Qwen3ASRForConditionalGeneration)
    AutoProcessor.register(cfg.Qwen3ASRConfig, proc.Qwen3ASRProcessor)
    return cfg, mdl, proc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audio", default=os.path.join(os.path.dirname(__file__), "..", "test.mp3"))
    ap.add_argument("--start", type=float, default=0.0, help="clip start seconds")
    ap.add_argument("--dur", type=float, default=20.0, help="clip duration seconds")
    ap.add_argument("--language", default=None, help="force language, e.g. Chinese")
    ap.add_argument("--max-new-tokens", type=int, default=256)
    ap.add_argument("--dump", action="store_true",
                    help="dump per-stage reference activations to models/reference/asr/")
    ap.add_argument("--dtype", default="bf16", choices=["bf16", "fp32"],
                    help="model compute dtype; fp32 gives a clean (rounding-free) encoder ref")
    args = ap.parse_args()

    register()
    from transformers import AutoModel, AutoProcessor

    dt = torch.bfloat16 if args.dtype == "bf16" else torch.float32
    print(f"loading model from {os.path.normpath(MODEL_DIR)} (dtype={args.dtype}) ...", flush=True)
    model = AutoModel.from_pretrained(MODEL_DIR, dtype=dt).eval().to("cuda:0")
    processor = AutoProcessor.from_pretrained(MODEL_DIR)

    import librosa
    wav, sr = librosa.load(args.audio, sr=16000, offset=args.start, duration=args.dur, mono=True)
    wav = np.asarray(wav, dtype=np.float32)
    print(f"clip: start={args.start}s dur={args.dur}s -> {wav.shape[0]} samples @ {sr}Hz", flush=True)

    # Build the prompt via the model's chat template.
    msgs = [
        {"role": "system", "content": ""},
        {"role": "user", "content": [{"type": "audio", "audio": ""}]},
    ]
    text = processor.apply_chat_template(msgs, add_generation_prompt=True, tokenize=False)
    if args.language:
        text = text + f"language {args.language}<asr_text>"

    inputs = processor(text=text, audio=[wav], return_tensors="pt", padding=True)
    inputs = {k: (v.to("cuda:0") if hasattr(v, "to") else v) for k, v in inputs.items()}
    if "input_features" in inputs:
        inputs["input_features"] = inputs["input_features"].to(dt)

    dump_dir = os.path.join(os.path.dirname(__file__), "..", "models", "reference", "asr")
    suffix = "" if args.dtype == "bf16" else "_fp32"
    captured = {}
    hooks = []
    if args.dump:
        os.makedirs(dump_dir, exist_ok=True)

        def save(name, arr):
            arr = np.ascontiguousarray(np.asarray(arr, dtype="<f4"))
            arr.tofile(os.path.join(dump_dir, name))
            print(f"  dumped {name:24s} {list(arr.shape)}", flush=True)

        # Raw wav + mel input_features (reference for the C++ Whisper mel).
        save("wav.f32", wav)
        if "input_features" in inputs:
            save("input_features.f32", inputs["input_features"].float().cpu().numpy())
        # Hook the audio tower to capture its [N, 2048] output (encoder ref).
        for mod_name, mod in model.named_modules():
            if mod_name.endswith("audio_tower"):
                def _hook(m, i, o, _n=mod_name):
                    t = o[0] if isinstance(o, (tuple, list)) else (
                        o.last_hidden_state if hasattr(o, "last_hidden_state") else o)
                    captured["audio_features"] = t.detach().float().cpu().numpy()
                hooks.append(mod.register_forward_hook(_hook))
                print(f"  hooked {mod_name}", flush=True)
                break
        # Hook audio_tower.layers.0 to capture pre-layer hidden (its input).
        for mod_name, mod in model.named_modules():
            if mod_name.endswith("audio_tower.layers.0"):
                def _pre(m, args, kwargs, _n=mod_name):
                    hs = args[0] if args else kwargs.get("hidden_states")
                    captured["prelayer"] = hs.detach().float().cpu().numpy()
                def _post0(m, i, o, _n=mod_name):
                    t = o[0] if isinstance(o, (tuple, list)) else o
                    captured["layer0"] = t.detach().float().cpu().numpy()
                hooks.append(mod.register_forward_pre_hook(_pre, with_kwargs=True))
                hooks.append(mod.register_forward_hook(_post0))
                print(f"  hooked {mod_name}", flush=True)
                break
        for mod_name, mod in model.named_modules():
            if mod_name.endswith("audio_tower.layers.0.self_attn_layer_norm"):
                def _ln(m, i, o, _n=mod_name):
                    captured["ln1"] = o.detach().float().cpu().numpy()
                hooks.append(mod.register_forward_hook(_ln))
            if mod_name.endswith("audio_tower.layers.0.self_attn"):
                def _att(m, i, o, _n=mod_name):
                    t = o[0] if isinstance(o, (tuple, list)) else o
                    captured["attn0"] = t.detach().float().cpu().numpy()
                hooks.append(mod.register_forward_hook(_att))
        # Hook the text model to capture inputs_embeds (after audio injection).
        for mod_name, mod in model.named_modules():
            if mod_name.endswith("thinker.model") and type(mod).__name__.endswith("TextModel"):
                def _txt(m, args, kwargs, _n=mod_name):
                    ie = kwargs.get("inputs_embeds")
                    if ie is not None and "text_embeds" not in captured:
                        captured["text_embeds"] = ie.detach().float().cpu().numpy()
                hooks.append(mod.register_forward_pre_hook(_txt, with_kwargs=True))
                print(f"  hooked {mod_name} (text model)", flush=True)
                break

    with torch.no_grad():
        out = model.generate(**inputs, max_new_tokens=args.max_new_tokens)

    if args.dump:
        for h in hooks:
            h.remove()
        if "audio_features" in captured:
            af = captured["audio_features"]
            af = np.ascontiguousarray(af.reshape(-1, af.shape[-1]).astype("<f4"))
            af.tofile(os.path.join(dump_dir, f"audio_features{suffix}.f32"))
            print(f"  dumped audio_features{suffix}.f32 {list(af.shape)}", flush=True)
        for key in ("prelayer", "layer0"):
            if key in captured:
                a = np.ascontiguousarray(
                    captured[key].reshape(-1, captured[key].shape[-1]).astype("<f4"))
                a.tofile(os.path.join(dump_dir, f"{key}{suffix}.f32"))
                print(f"  dumped {key}{suffix}.f32 {list(a.shape)}", flush=True)
        for key in ("ln1", "attn0"):
            if key in captured:
                a = np.ascontiguousarray(
                    captured[key].reshape(-1, captured[key].shape[-1]).astype("<f4"))
                a.tofile(os.path.join(dump_dir, f"{key}{suffix}.f32"))
                print(f"  dumped {key}{suffix}.f32 {list(a.shape)}", flush=True)
        if "text_embeds" in captured:
            te = np.ascontiguousarray(
                captured["text_embeds"].reshape(-1, captured["text_embeds"].shape[-1]).astype("<f4"))
            te.tofile(os.path.join(dump_dir, f"text_embeds{suffix}.f32"))
            print(f"  dumped text_embeds{suffix}.f32 {list(te.shape)}", flush=True)
        # Prefill logits for the prompt (last token row) via a single forward.
        with torch.no_grad():
            fwd = model.thinker(**inputs, use_cache=False)
        lg = fwd.logits if hasattr(fwd, "logits") else fwd[0]
        last = lg[0, -1].float().cpu().numpy().astype("<f4")
        last.tofile(os.path.join(dump_dir, f"prefill_last_logits{suffix}.f32"))
        print(f"  dumped prefill_last_logits{suffix}.f32 {list(last.shape)} "
              f"argmax={int(last.argmax())}", flush=True)
        # Prompt token ids (int32) for verifying tokenizer + prompt format.
        pid = inputs["input_ids"][0].cpu().numpy().astype("<i4")
        pid.tofile(os.path.join(dump_dir, "prompt_ids.i32"))
        print(f"  dumped prompt_ids.i32 {list(pid.shape)}", flush=True)

    seq = out.sequences if hasattr(out, "sequences") else out
    gen = seq[0, inputs["input_ids"].shape[1]:]
    transcript = processor.tokenizer.decode(gen, skip_special_tokens=True)
    if args.dump:
        gen_ids = gen.cpu().numpy().astype("<i4")
        gen_ids.tofile(os.path.join(dump_dir, "gen_ids.i32"))
        with open(os.path.join(dump_dir, "transcript.txt"), "w") as f:
            f.write(transcript)
        print(f"  dumped gen_ids.i32 {list(gen_ids.shape)} + transcript.txt", flush=True)
    print("\n===== ORACLE TRANSCRIPT =====")
    print(transcript)
    print("=============================")


if __name__ == "__main__":
    main()

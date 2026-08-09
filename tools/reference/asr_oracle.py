#!/usr/bin/env python3
"""Numerical reference runner for the native Qwen3-ASR implementation.

The tool loads the pinned official Transformers backend with the project's local
weights. It can emit raw transcripts and stage tensors for direct inspection and
native implementation parity checks. It never scores, ranks, or labels product
output and does not provide an ASR accuracy verdict.

Usage:
  source tools/torchenv.sh
  python tools/reference/asr_oracle.py --start 0 --dur 20

The official source checkout defaults to /tmp/Qwen3-ASR and must match
EXPECTED_REFERENCE_REVISION. Set QWEN3_ASR_REFERENCE_ROOT when the pinned
checkout lives elsewhere.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REFERENCE_ROOT = Path(
    os.environ.get("QWEN3_ASR_REFERENCE_ROOT", "/tmp/Qwen3-ASR"))
DEFAULT_MODEL_DIR = PROJECT_ROOT / "models/asr/Qwen/Qwen3-ASR-1.7B"
DEFAULT_AUDIO = PROJECT_ROOT / "test/data/audio/test.mp3"
DEFAULT_CONFIG = PROJECT_ROOT / "orator.toml"
DEFAULT_ARTIFACT_DIR = PROJECT_ROOT / "models/reference/asr"
EXPECTED_REFERENCE_REVISION = (
    "7c6daf77a2421100f5fb066495372c00129d39ff")
REFERENCE_FILES = (
    "configuration_qwen3_asr.py",
    "modeling_qwen3_asr.py",
    "processing_qwen3_asr.py",
)


def reference_backend(reference_root):
    """Return the official backend directory or raise on an incomplete tree."""
    backend = (Path(reference_root).resolve()
               / "qwen_asr/core/transformers_backend")
    missing = [name for name in REFERENCE_FILES
               if not (backend / name).is_file()]
    if missing:
        raise RuntimeError(
            f"incomplete Qwen3-ASR reference at {backend}: "
            f"missing {', '.join(missing)}")
    return backend


def git_revision(reference_root):
    """Read the exact source revision without modifying the checkout."""
    result = subprocess.run(
        ["git", "-C", str(Path(reference_root).resolve()), "rev-parse", "HEAD"],
        check=False, capture_output=True, text=True)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(f"cannot read Qwen3-ASR revision: {detail}")
    return result.stdout.strip()


def git_worktree_status(reference_root):
    """Return porcelain status so a pinned revision cannot hide local edits."""
    result = subprocess.run(
        ["git", "-C", str(Path(reference_root).resolve()), "status",
         "--short"],
        check=False, capture_output=True, text=True)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(f"cannot inspect Qwen3-ASR worktree: {detail}")
    return result.stdout.strip()


def verify_reference(reference_root,
                     expected_revision=EXPECTED_REFERENCE_REVISION):
    """Validate required source files and the pinned Git revision."""
    backend = reference_backend(reference_root)
    revision = git_revision(reference_root)
    if revision != expected_revision:
        raise RuntimeError(
            "Qwen3-ASR reference revision mismatch: "
            f"expected {expected_revision}, found {revision}")
    status = git_worktree_status(reference_root)
    if status:
        raise RuntimeError(
            f"Qwen3-ASR reference worktree is not clean: {status}")
    return backend, revision


def load_asr_config(config_path):
    """Read production ASR behavior from TOML without importing project code."""
    path = Path(config_path).resolve()
    with path.open("rb") as handle:
        data = tomllib.load(handle)
    section = data.get("asr")
    if not isinstance(section, dict):
        raise RuntimeError(f"missing [asr] section in {path}")
    return {
        "language": str(section.get("language", "")),
        "system_prompt": str(section.get("system_prompt", "")),
        "max_new_tokens": int(section.get("max_new_tokens", 256)),
    }


def build_provenance(reference_root, revision, model_dir, audio_path,
                     config_path, artifact_dir, asr_config):
    """Build a mechanical provenance record; no product result is evaluated."""
    return {
        "reference_root": str(Path(reference_root).resolve()),
        "reference_revision": revision,
        "expected_reference_revision": EXPECTED_REFERENCE_REVISION,
        "model_dir": str(Path(model_dir).resolve()),
        "audio": str(Path(audio_path).resolve()),
        "config": str(Path(config_path).resolve()),
        "artifact_dir": str(Path(artifact_dir).resolve()),
        "asr_config": asr_config,
    }


def register(backend):
    """Register official custom Qwen3-ASR classes with Transformers Auto APIs."""
    temp_dir = Path(tempfile.mkdtemp(prefix="qwen3asr_ref_"))
    for filename in REFERENCE_FILES:
        source = (Path(backend) / filename).read_text(encoding="utf-8")
        source = re.sub(r"from \.(\w+) import", r"from \1 import", source)
        source = re.sub(r"from \.(\w+) import \(", r"from \1 import (", source)
        (temp_dir / filename).write_text(source, encoding="utf-8")
    sys.path.insert(0, str(temp_dir))

    import configuration_qwen3_asr as cfg
    import modeling_qwen3_asr as mdl
    import processing_qwen3_asr as proc
    from transformers import AutoConfig, AutoModel, AutoProcessor

    AutoConfig.register("qwen3_asr", cfg.Qwen3ASRConfig)
    try:
        AutoConfig.register(
            "qwen3_asr_audio_encoder", cfg.Qwen3ASRAudioEncoderConfig)
    except ValueError:
        pass
    AutoModel.register(
        cfg.Qwen3ASRConfig, mdl.Qwen3ASRForConditionalGeneration)
    AutoProcessor.register(cfg.Qwen3ASRConfig, proc.Qwen3ASRProcessor)
    return cfg, mdl, proc


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--audio", default=str(DEFAULT_AUDIO))
    parser.add_argument("--start", type=float, default=0.0,
                        help="clip start seconds")
    parser.add_argument("--dur", type=float, default=20.0,
                        help="clip duration seconds")
    parser.add_argument("--config", default=str(DEFAULT_CONFIG),
                        help="TOML supplying ASR language, prompt, and limit")
    parser.add_argument("--reference-root",
                        default=str(DEFAULT_REFERENCE_ROOT))
    parser.add_argument("--model-dir", default=str(DEFAULT_MODEL_DIR))
    parser.add_argument("--artifact-dir", default=str(DEFAULT_ARTIFACT_DIR))
    parser.add_argument("--dump", action="store_true",
                        help="dump raw stage evidence to --artifact-dir")
    parser.add_argument("--dtype", default="bf16",
                        choices=["bf16", "fp32"],
                        help="model compute dtype for numerical evidence")
    parser.add_argument("--check-only", action="store_true",
                        help="validate paths/provenance without loading Torch")
    return parser.parse_args()


def main():
    args = parse_args()
    reference_root = Path(args.reference_root).resolve()
    model_dir = Path(args.model_dir).resolve()
    audio_path = Path(args.audio).resolve()
    config_path = Path(args.config).resolve()
    artifact_dir = Path(args.artifact_dir).resolve()

    backend, revision = verify_reference(reference_root)
    for label, path in (("model directory", model_dir),
                        ("audio input", audio_path),
                        ("TOML config", config_path)):
        if not path.exists():
            raise RuntimeError(f"missing {label}: {path}")
    asr_config = load_asr_config(config_path)
    provenance = build_provenance(
        reference_root, revision, model_dir, audio_path, config_path,
        artifact_dir, asr_config)
    print("===== ORACLE PROVENANCE (MECHANICAL ONLY) =====", flush=True)
    print(json.dumps(provenance, ensure_ascii=False, indent=2), flush=True)
    if args.check_only:
        return

    import numpy as np
    import torch

    register(backend)
    from transformers import AutoModel, AutoProcessor

    if not torch.cuda.is_available():
        raise RuntimeError(
            "CUDA-enabled Torch is required for the Qwen3-ASR numerical oracle")
    dtype = torch.bfloat16 if args.dtype == "bf16" else torch.float32
    print(f"loading model from {model_dir} (dtype={args.dtype}) ...", flush=True)
    model = AutoModel.from_pretrained(
        model_dir, dtype=dtype).eval().to("cuda:0")
    processor = AutoProcessor.from_pretrained(model_dir)

    import librosa
    wav, sample_rate = librosa.load(
        audio_path, sr=16000, offset=args.start, duration=args.dur, mono=True)
    wav = np.asarray(wav, dtype=np.float32)
    print(
        f"clip: start={args.start}s dur={args.dur}s -> "
        f"{wav.shape[0]} samples @ {sample_rate}Hz", flush=True)

    messages = [
        {"role": "system", "content": asr_config["system_prompt"]},
        {"role": "user", "content": [{"type": "audio", "audio": ""}]},
    ]
    text = processor.apply_chat_template(
        messages, add_generation_prompt=True, tokenize=False)
    if asr_config["language"]:
        text += f"language {asr_config['language']}<asr_text>"

    inputs = processor(
        text=text, audio=[wav], return_tensors="pt", padding=True)
    inputs = {key: (value.to("cuda:0") if hasattr(value, "to") else value)
              for key, value in inputs.items()}
    if "input_features" in inputs:
        inputs["input_features"] = inputs["input_features"].to(dtype)

    suffix = "" if args.dtype == "bf16" else "_fp32"
    captured = {}
    hooks = []
    if args.dump:
        artifact_dir.mkdir(parents=True, exist_ok=True)

        def save(name, array):
            array = np.ascontiguousarray(np.asarray(array, dtype="<f4"))
            array.tofile(artifact_dir / name)
            print(f"  dumped {name:24s} {list(array.shape)}", flush=True)

        save("wav.f32", wav)
        if "input_features" in inputs:
            save("input_features.f32",
                 inputs["input_features"].float().cpu().numpy())
        for module_name, module in model.named_modules():
            if module_name.endswith("audio_tower"):
                def audio_hook(_module, _inputs, output):
                    tensor = output[0] if isinstance(output, (tuple, list)) else (
                        output.last_hidden_state
                        if hasattr(output, "last_hidden_state") else output)
                    captured["audio_features"] = (
                        tensor.detach().float().cpu().numpy())
                hooks.append(module.register_forward_hook(audio_hook))
                print(f"  hooked {module_name}", flush=True)
                break
        for module_name, module in model.named_modules():
            if module_name.endswith("audio_tower.layers.0"):
                def layer_pre_hook(_module, positional, keyword):
                    hidden = (positional[0] if positional
                              else keyword.get("hidden_states"))
                    captured["prelayer"] = (
                        hidden.detach().float().cpu().numpy())

                def layer_post_hook(_module, _inputs, output):
                    tensor = output[0] if isinstance(output, (tuple, list)) else output
                    captured["layer0"] = tensor.detach().float().cpu().numpy()

                hooks.append(module.register_forward_pre_hook(
                    layer_pre_hook, with_kwargs=True))
                hooks.append(module.register_forward_hook(layer_post_hook))
                print(f"  hooked {module_name}", flush=True)
                break
        for module_name, module in model.named_modules():
            if module_name.endswith(
                    "audio_tower.layers.0.self_attn_layer_norm"):
                def layer_norm_hook(_module, _inputs, output):
                    captured["ln1"] = output.detach().float().cpu().numpy()
                hooks.append(module.register_forward_hook(layer_norm_hook))
            if module_name.endswith("audio_tower.layers.0.self_attn"):
                def attention_hook(_module, _inputs, output):
                    tensor = output[0] if isinstance(output, (tuple, list)) else output
                    captured["attn0"] = tensor.detach().float().cpu().numpy()
                hooks.append(module.register_forward_hook(attention_hook))
        for module_name, module in model.named_modules():
            if (module_name.endswith("thinker.model")
                    and type(module).__name__.endswith("TextModel")):
                def text_hook(_module, _positional, keyword):
                    embeds = keyword.get("inputs_embeds")
                    if embeds is not None and "text_embeds" not in captured:
                        captured["text_embeds"] = (
                            embeds.detach().float().cpu().numpy())
                hooks.append(module.register_forward_pre_hook(
                    text_hook, with_kwargs=True))
                print(f"  hooked {module_name} (text model)", flush=True)
                break

    with torch.no_grad():
        output = model.generate(
            **inputs, max_new_tokens=asr_config["max_new_tokens"])

    if args.dump:
        for hook in hooks:
            hook.remove()
        if "audio_features" in captured:
            features = captured["audio_features"]
            features = np.ascontiguousarray(
                features.reshape(-1, features.shape[-1]).astype("<f4"))
            features.tofile(artifact_dir / f"audio_features{suffix}.f32")
            print(
                f"  dumped audio_features{suffix}.f32 "
                f"{list(features.shape)}", flush=True)
        for key in ("prelayer", "layer0", "ln1", "attn0"):
            if key in captured:
                array = captured[key]
                array = np.ascontiguousarray(
                    array.reshape(-1, array.shape[-1]).astype("<f4"))
                array.tofile(artifact_dir / f"{key}{suffix}.f32")
                print(
                    f"  dumped {key}{suffix}.f32 {list(array.shape)}",
                    flush=True)
        if "text_embeds" in captured:
            embeds = captured["text_embeds"]
            embeds = np.ascontiguousarray(
                embeds.reshape(-1, embeds.shape[-1]).astype("<f4"))
            embeds.tofile(artifact_dir / f"text_embeds{suffix}.f32")
            print(
                f"  dumped text_embeds{suffix}.f32 {list(embeds.shape)}",
                flush=True)
        with torch.no_grad():
            forward = model.thinker(**inputs, use_cache=False)
        logits = forward.logits if hasattr(forward, "logits") else forward[0]
        last = logits[0, -1].float().cpu().numpy().astype("<f4")
        last.tofile(artifact_dir / f"prefill_last_logits{suffix}.f32")
        print(
            f"  dumped prefill_last_logits{suffix}.f32 {list(last.shape)} "
            f"argmax={int(last.argmax())}", flush=True)
        prompt_ids = inputs["input_ids"][0].cpu().numpy().astype("<i4")
        prompt_ids.tofile(artifact_dir / "prompt_ids.i32")
        print(
            f"  dumped prompt_ids.i32 {list(prompt_ids.shape)}", flush=True)

    sequence = output.sequences if hasattr(output, "sequences") else output
    generated = sequence[0, inputs["input_ids"].shape[1]:]
    transcript = processor.tokenizer.decode(
        generated, skip_special_tokens=True)
    if args.dump:
        generated_ids = generated.cpu().numpy().astype("<i4")
        generated_ids.tofile(artifact_dir / "gen_ids.i32")
        (artifact_dir / "transcript.txt").write_text(
            transcript, encoding="utf-8")
        (artifact_dir / "provenance.json").write_text(
            json.dumps(provenance, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")
        print(
            f"  dumped gen_ids.i32 {list(generated_ids.shape)} + "
            "transcript.txt + provenance.json", flush=True)
    print("\n===== RAW ORACLE TRANSCRIPT (NOT AN ACCURACY VERDICT) =====")
    print(transcript)
    print("===========================================================")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Reference oracle for the Qwen3 Forced Aligner (CPU PyTorch).

Two layers:
  1. Torch-free I/O + decode ground truth (special ids, word tokenisation,
     `_fix_timestamps` cases) -- always produced.
  2. Full forward ground truth (when torch is importable): runs the real
     `Qwen3ASRForTokenClassification` on CPU over a fixed (audio, transcript)
     and dumps stage tensors for numerical validation of the C++ port:
       mel (input_features), input_ids, audio_features (post-projector),
       lm last_hidden_state, score logits at <timestamp> positions, argmax
       labels, and decoded word timestamps.

CUDA is not required: the C++ runtime uses CUDA, but validation only needs a CPU
reference to compare against.

Usage:
  tools/.venv/bin/python tools/reference/aligner_oracle.py \
      [--audio test/data/audio/test.mp3] [--dur 6.0] [--transcript "..."]
Writes JSON + raw float32 tensors under tools/reference/aligner_dump/.
"""

import argparse
import json
import os
import subprocess

import numpy as np
from transformers import AutoTokenizer
from transformers.models.qwen3_asr import processing_qwen3_asr as P

HERE = os.path.dirname(__file__)
MODEL_DIR = os.path.join(HERE, "..", "..", "models", "ForcedAligner")
OUT_DIR = os.path.join(HERE, "aligner_dump")
DEFAULT_AUDIO = os.path.join(HERE, "..", "..", "test", "data", "audio", "test.mp3")
DEFAULT_TRANSCRIPT = "比较理想化的一个人吧其实是这样的"
DEFAULT_LANGUAGE = "Chinese"


def split_words(text, lang):
    # split_words_for_alignment does not use `self` on the default CJK/space path.
    return P.Qwen3ASRProcessor.split_words_for_alignment(object(), text, lang)


def write_f32(name, arr):
    arr = np.ascontiguousarray(arr, dtype=np.float32)
    with open(os.path.join(OUT_DIR, name), "wb") as f:
        f.write(arr.tobytes())
    return list(arr.shape)


def load_audio(path, sr=16000, dur=None):
    cmd = ["ffmpeg", "-v", "quiet", "-i", path, "-ar", str(sr), "-ac", "1", "-f", "f32le"]
    if dur:
        cmd += ["-t", str(dur)]
    cmd += ["-"]
    raw = subprocess.run(cmd, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def io_oracle(tok):
    special = {
        t: tok.convert_tokens_to_ids(t)
        for t in ["<|audio_start|>", "<|audio_pad|>", "<|audio_end|>", "<timestamp>"]
    }
    transcripts = [
        ("Chinese", "你好，世界123 abc。"),
        ("English", "Mr. Quilter's the apostle of the middle classes."),
        ("Chinese", "我觉得十五是差不多的。"),
    ]
    word_cases = []
    for lang, text in transcripts:
        words = split_words(text, lang)
        word_cases.append({
            "language": lang, "text": text, "words": words,
            "word_ids": [tok.encode(w, add_special_tokens=False) for w in words],
        })
    raw_cases = [
        [0, 80, 160, 240, 320],
        [0, 80, 60, 240, 320],
        [0, 500, 80, 160, 240, 600],
        [100, 90, 80, 200, 210, 50, 300],
        [0, 0, 80, 80, 160, 160],
    ]
    fix_cases = [
        {"raw": r, "fixed": list(P._fix_timestamps(np.array(r, dtype=np.float64)))}
        for r in raw_cases
    ]
    return {
        "timestamp_segment_time_ms": 80,
        "special_ids": special,
        "word_cases": word_cases,
        "fix_timestamps_cases": fix_cases,
    }


def forward_oracle(args, out):
    try:
        import torch
        from transformers import AutoModelForTokenClassification, AutoProcessor
    except Exception as e:  # noqa: BLE001
        out["forward"] = {"skipped": f"torch/model unavailable: {e}"}
        return

    torch.manual_seed(0)
    proc = AutoProcessor.from_pretrained(MODEL_DIR)
    model = AutoModelForTokenClassification.from_pretrained(
        MODEL_DIR, dtype=torch.float32
    ).eval()

    audio = load_audio(args.audio, dur=args.dur)
    aligner_inputs, word_lists = proc.prepare_forced_aligner_inputs(
        audio=audio, transcript=args.transcript, language=args.language,
    )

    input_ids = aligner_inputs["input_ids"][0].tolist()
    mel = aligner_inputs["input_features"][0].numpy()  # [128, T]
    ts_id = model.config.timestamp_token_id

    with torch.inference_mode():
        base = model.model(
            input_ids=aligner_inputs["input_ids"],
            input_features=aligner_inputs["input_features"],
            input_features_mask=aligner_inputs["input_features_mask"],
        )
        last_hidden = base.last_hidden_state  # [1, seq, 1024]
        audio_feats = base.audio_hidden_states  # [N, 1024] post-projector
        logits = model.score(last_hidden)  # [1, seq, 5000]

    seq = last_hidden.shape[1]
    ids = np.array(input_ids)
    ts_pos = np.nonzero(ids == ts_id)[0]
    ts_logits = logits[0, ts_pos, :].float().numpy()  # [2K, 5000]
    ts_labels = ts_logits.argmax(-1).tolist()

    timestamps = proc.decode_forced_alignment(
        logits=logits, input_ids=aligner_inputs["input_ids"],
        word_lists=word_lists, timestamp_token_id=ts_id,
    )[0]

    shapes = {
        "mel": write_f32("mel.f32", mel),
        "audio_feats": write_f32("audio_feats.f32", audio_feats.float().numpy()),
        "lm_hidden": write_f32("lm_hidden.f32", last_hidden[0].float().numpy()),
        "ts_logits": write_f32("ts_logits.f32", ts_logits),
    }
    np.array(input_ids, dtype=np.int32).tofile(os.path.join(OUT_DIR, "input_ids.i32"))
    np.array(ts_labels, dtype=np.int32).tofile(os.path.join(OUT_DIR, "ts_labels.i32"))
    # End-to-end fixtures: the exact PCM and decoded per-word times (ms).
    write_f32("audio.f32", audio)
    word_ms = []
    for it in timestamps:
        word_ms.append(int(round(it["start_time"] * 1000)))
        word_ms.append(int(round(it["end_time"] * 1000)))
    np.array(word_ms, dtype=np.int32).tofile(os.path.join(OUT_DIR, "word_times.i32"))
    write_f32("audio.f32", audio)  # the exact PCM fed (for the C++ end-to-end test)
    # word timestamps as [start_ms, end_ms] int32 pairs.
    wt = []
    for it in timestamps:
        wt += [int(round(it["start_time"] * 1000)), int(round(it["end_time"] * 1000))]
    np.array(wt, dtype=np.int32).tofile(os.path.join(OUT_DIR, "word_times.i32"))
    out["forward"] = {
        "audio": os.path.relpath(args.audio, os.path.join(HERE, "..", "..")),
        "dur": args.dur, "transcript": args.transcript, "language": args.language,
        "num_audio_tokens": int(aligner_inputs["num_audio_tokens"][0])
            if "num_audio_tokens" in aligner_inputs else None,
        "seq_len": int(seq),
        "timestamp_token_id": int(ts_id),
        "timestamp_positions": ts_pos.tolist(),
        "ts_labels": ts_labels,
        "input_ids": input_ids,
        "word_lists": word_lists,
        "timestamps": timestamps,
        "shapes": shapes,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audio", default=DEFAULT_AUDIO)
    ap.add_argument("--dur", type=float, default=6.0)
    ap.add_argument("--transcript", default=DEFAULT_TRANSCRIPT)
    ap.add_argument("--language", default=DEFAULT_LANGUAGE)
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    tok = AutoTokenizer.from_pretrained(MODEL_DIR)
    out = io_oracle(tok)
    forward_oracle(args, out)

    path = os.path.join(OUT_DIR, "oracle.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
    print("wrote", path)
    print("special_ids:", out["special_ids"])
    fw = out.get("forward", {})
    if "skipped" in fw:
        print("forward:", fw["skipped"])
    else:
        print(f"forward: seq={fw['seq_len']} N_audio={fw['num_audio_tokens']} "
              f"words={len(fw['word_lists'][0])} ts={len(fw['ts_labels'])}")
        for it in fw["timestamps"][:8]:
            print(f"  {it['text']:<8} {it['start_time']:.3f} -> {it['end_time']:.3f}")
        print("shapes:", fw["shapes"])


if __name__ == "__main__":
    main()

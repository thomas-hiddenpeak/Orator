#!/usr/bin/env python3
"""Extract vocab.json + merges.txt from a HF tokenizer.json (byte-level BPE).

The Qwen3 Forced Aligner ships only `tokenizer.json`, but the C++
`io::BpeTokenizer` loads the classic `vocab.json` + `merges.txt` pair (same
byte-level Qwen2 BPE space). This offline converter writes those two files into
the model directory so the validated runtime tokenizer can be reused unchanged.

Usage:
  tools/.venv/bin/python tools/convert/aligner_extract_tokenizer.py \
      [--model-dir models/ForcedAligner]
"""

import argparse
import json
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model-dir", default="models/ForcedAligner")
    args = ap.parse_args()

    tj = os.path.join(args.model_dir, "tokenizer.json")
    with open(tj, encoding="utf-8") as f:
        tok = json.load(f)
    model = tok["model"]
    assert model.get("type") == "BPE", f"expected BPE, got {model.get('type')}"

    vocab = model["vocab"]  # {token: id}
    merges = model["merges"]  # list of [a, b] (or "a b")

    vocab_path = os.path.join(args.model_dir, "vocab.json")
    with open(vocab_path, "w", encoding="utf-8") as f:
        json.dump(vocab, f, ensure_ascii=False)

    merges_path = os.path.join(args.model_dir, "merges.txt")
    with open(merges_path, "w", encoding="utf-8") as f:
        f.write("#version: 0.2\n")
        for m in merges:
            if isinstance(m, (list, tuple)):
                f.write(f"{m[0]} {m[1]}\n")
            else:
                f.write(f"{m}\n")

    print(f"wrote {vocab_path} ({len(vocab)} tokens)")
    print(f"wrote {merges_path} ({len(merges)} merges)")


if __name__ == "__main__":
    main()

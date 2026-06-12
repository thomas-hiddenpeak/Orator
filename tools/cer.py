#!/usr/bin/env python3
"""Character error rate (CER) between an ASR hypothesis and the gold transcript.

Standard library only (offline analysis tool; not a runtime path). It compares
normalized character sequences using Levenshtein edit distance:

    CER = (substitutions + insertions + deletions) / len(reference)

Reference: asrTest2Final.txt has "HH:MM:SS speaker" header lines followed by
text lines. With --max-sec, only segments whose header time is < max-sec are
included, so the reference matches a bounded hypothesis span.

Hypothesis: either a streamed JSON file (reads the comprehensive view text, or
the asr-track entries) or a plain text file/string.

Normalization (both sides): keep CJK characters and alphanumerics, drop
punctuation, spaces, and newlines, lowercase ASCII. This measures content
accuracy independent of segmentation and punctuation.

Usage:
  python3 cer.py --gold asrTest2Final.txt --hyp-json /tmp/out.json [--max-sec 120]
  python3 cer.py --gold asrTest2Final.txt --hyp-text "..." [--max-sec 120]
"""

import argparse
import json
import re
import sys


def normalize(text: str) -> str:
    # Keep CJK unified ideographs and ASCII alphanumerics; drop everything else.
    out = []
    for ch in text:
        o = ord(ch)
        if (0x4E00 <= o <= 0x9FFF) or ch.isalnum():
            out.append(ch.lower())
    return "".join(out)


def parse_gold(path: str, max_sec: float) -> str:
    header = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s")
    parts = []
    include = True
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            m = header.match(line)
            if m:
                h, mm, s = int(m.group(1)), int(m.group(2)), int(m.group(3))
                t = h * 3600 + mm * 60 + s
                include = (max_sec <= 0) or (t < max_sec)
                continue  # header line itself carries no transcript text
            if include:
                parts.append(line)
    return "".join(parts)


def hyp_from_json(path: str) -> str:
    d = json.load(open(path, encoding="utf-8"))
    tl = d.get("timeline", d)
    # Prefer the comprehensive view; fall back to the asr track.
    comp = tl.get("comprehensive")
    if comp:
        return "".join(seg.get("text", "") for seg in comp)
    for t in tl.get("tracks", []):
        if t.get("kind") == "asr":
            return "".join(e.get("text", "") for e in t.get("entries", []))
    return ""


def edit_distance(a: str, b: str) -> int:
    n, m = len(a), len(b)
    if n == 0:
        return m
    if m == 0:
        return n
    prev = list(range(m + 1))
    for i in range(1, n + 1):
        cur = [i] + [0] * m
        ai = a[i - 1]
        for j in range(1, m + 1):
            cost = 0 if ai == b[j - 1] else 1
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost)
        prev = cur
    return prev[m]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--gold", required=True)
    ap.add_argument("--hyp-json")
    ap.add_argument("--hyp-text")
    ap.add_argument("--max-sec", type=float, default=0.0,
                    help="include gold segments with header time < max-sec (0 = all)")
    args = ap.parse_args()

    ref = normalize(parse_gold(args.gold, args.max_sec))
    if args.hyp_json:
        hyp = normalize(hyp_from_json(args.hyp_json))
    elif args.hyp_text is not None:
        hyp = normalize(args.hyp_text)
    else:
        print("error: need --hyp-json or --hyp-text", file=sys.stderr)
        return 2

    dist = edit_distance(ref, hyp)
    cer = dist / len(ref) if ref else 0.0
    print(f"ref_chars={len(ref)} hyp_chars={len(hyp)} edits={dist} CER={cer:.4f} "
          f"({100*cer:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

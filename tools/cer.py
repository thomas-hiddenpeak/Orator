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


def _cn_to_arabic(s: str) -> str:
    """Convert a matched Chinese-numeral sequence to Arabic digits."""
    units = {'十': 10, '百': 100, '千': 1000, '万': 10000, '亿': 100000000}
    digits = {'零': 0, '一': 1, '二': 2, '三': 3, '四': 4,
              '五': 5, '六': 6, '七': 7, '八': 8, '九': 9, '两': 2}
    if not s:
        return s
    if s.isdigit():
        return s
    if '点' in s:
        left, right = s.split('点', 1)
        int_part = _cn_to_arabic(left) if left else '0'
        frac_part = ''.join(str(digits.get(c, c)) if c in digits else c for c in right)
        return int_part + '.' + frac_part
    result = 0
    section = 0
    tmp = 0
    for ch in s:
        if ch.isdigit():
            tmp = tmp * 10 + int(ch)
        elif ch in digits:
            tmp = tmp * 10 + digits[ch]
        elif ch in units:
            u = units[ch]
            if u >= 10000:
                section = (section + (tmp if tmp else 0)) * u
                result += section
                section = 0
                tmp = 0
            else:
                section += (tmp if tmp else 1) * u
                tmp = 0
        else:
            return s
    result += section + tmp
    return str(result) if result or section or tmp else s


_CN_NUM_RE = re.compile(
    r'百分之([零一二三四五六七八九十百千两\d点]+)'
    r'|([零一二三四五六七八九十百千万亿两点]+)'
)

_FILLER_RE = re.compile(r'[嗯呃啊额哦唉]')


def _unify_numbers(text: str) -> str:
    """Normalise number representation: cn numerals → arabic."""
    def _repl(m: re.Match) -> str:
        if m.group(1) is not None:
            return _cn_to_arabic(m.group(1)) + '%'
        return _cn_to_arabic(m.group(2))
    return _CN_NUM_RE.sub(_repl, text)


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


def _normalize_with_numbers(text: str) -> str:
    text = _FILLER_RE.sub('', text)
    text = _unify_numbers(text)
    return normalize(text)


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

    ref = _normalize_with_numbers(parse_gold(args.gold, args.max_sec))
    if args.hyp_json:
        hyp = _normalize_with_numbers(hyp_from_json(args.hyp_json))
    elif args.hyp_text is not None:
        hyp = _normalize_with_numbers(args.hyp_text)
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

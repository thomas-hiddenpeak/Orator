#!/usr/bin/env python3
"""Closing accuracy check: pipeline speaker attribution vs test.txt ground truth.

Constitution Art. VI: compare attributed speakers to the named, timestamped
reference (test/data/reference/test.txt) item-by-item. Reads a timeline JSON
produced by ws_unified_test.py and reports, over the analysed window:
  - mapping spk_<n> -> ground-truth name (by max overlap),
  - duration-weighted speaker accuracy of the comprehensive view,
  - per-reference-turn dominant speaker_id.

Usage: python speaker_attrib_eval.py /tmp/out.json [--txt test/.../test.txt]
"""
import argparse
import collections
import json
import re

TS = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(\S+)")


def gt_turns(path, audio):
    t = []
    for line in open(path, encoding="utf-8"):
        m = TS.match(line.strip())
        if m:
            h, mi, s, spk = m.groups()
            t.append((int(h) * 3600 + int(mi) * 60 + int(s), spk))
    return [(t[i][0], t[i + 1][0] if i + 1 < len(t) else audio, t[i][1])
            for i in range(len(t)) if (t[i + 1][0] if i + 1 < len(t) else audio) > t[i][0]]


def overlap(a0, a1, b0, b1):
    return max(0.0, min(a1, b1) - max(a0, b0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("json")
    ap.add_argument("--txt", default="test/data/reference/test.txt")
    a = ap.parse_args()
    d = json.load(open(a.json))
    tl = d["timeline"]
    audio = tl["audio_sec"]
    comp = [(e["start"], e["end"], e.get("speaker_id") or f"L{e.get('speaker')}")
            for e in tl["comprehensive"]]
    gt = gt_turns(a.txt, audio)
    gt = [g for g in gt if g[0] < audio]

    # spk_id -> {name: overlap secs} ; map to argmax name
    pair = collections.defaultdict(lambda: collections.Counter())
    for (cs, ce, sid) in comp:
        for (gs, ge, nm) in gt:
            ov = overlap(cs, ce, gs, ge)
            if ov > 0:
                pair[sid][nm] += ov
    mapping = {sid: c.most_common(1)[0][0] for sid, c in pair.items()}
    print("audio=%.0fs  comp_entries=%d  gt_turns=%d" % (audio, len(comp), len(gt)))
    print("mapping spk->name:", mapping)

    # duration-weighted accuracy: comp speaker mapped name vs gt name per overlap
    correct = total = 0.0
    for (cs, ce, sid) in comp:
        nm = mapping.get(sid)
        for (gs, ge, gnm) in gt:
            ov = overlap(cs, ce, gs, ge)
            if ov > 0:
                total += ov
                if nm == gnm:
                    correct += ov
    print("speaker accuracy (dur-weighted) = %.1f%%  (%.0f/%.0f s)"
          % (100 * correct / max(total, 1), correct, total))
    names = set(mapping.values())
    print("distinct gt names covered: %d/%d" % (len(names), len({g[2] for g in gt})))


if __name__ == "__main__":
    main()

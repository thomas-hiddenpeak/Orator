#!/usr/bin/env python3
"""Legacy mechanical speaker-attribution overlap report.

Under Constitution 1.7.0 this utility is prohibited from evaluating accuracy,
ranking/selecting candidates, choosing parameters, or issuing a verdict. Its
historical overlap values are mechanical records only. This utility
reads a timeline JSON produced by ws_unified_test.py and reports:
  - mapping spk_<n> -> ground-truth name (by max overlap),
  - duration-weighted mechanical overlap of the comprehensive view,
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
    ap.add_argument("--windows", type=int, default=0,
                    help="per-window (seconds) diar ceiling + attribution decay")
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
    print("diagnostic attribution overlap (dur-weighted) = %.1f%%  (%.0f/%.0f s)"
          % (100 * correct / max(total, 1), correct, total))
    names = set(mapping.values())
    print("distinct gt names covered: %d/%d" % (len(names), len({g[2] for g in gt})))

    # Per-window diarization ceiling: assign each diarizer-LOCAL slot to the GT
    # name it overlaps most WITHIN the window (best case, independent of
    # speaker-id enrollment). A decay over time isolates Sortformer's own
    # long-session degradation from the voiceprint stage.
    diar = [t for t in tl["tracks"] if t.get("kind") == "diarization"]
    if a.windows and diar:
        seg = [(e["start"], e["end"], e.get("speaker")) for e in diar[0]["entries"]]
        print("\nwindow | diar_local_ceiling | attrib_acc")
        for w in range(0, int(audio), a.windows):
            we = min(w + a.windows, audio)
            lg = collections.defaultdict(collections.Counter)
            for cs, ce, loc in seg:
                a0, a1 = max(cs, w), min(ce, we)
                if a1 <= a0:
                    continue
                for gs, ge, nm in gt:
                    o = overlap(a0, a1, gs, ge)
                    if o > 0:
                        lg[loc][nm] += o
            lmap = {loc: c.most_common(1)[0][0] for loc, c in lg.items()}
            dc = dt = ac = at = 0.0
            for cs, ce, loc in seg:
                a0, a1 = max(cs, w), min(ce, we)
                if a1 <= a0:
                    continue
                for gs, ge, gnm in gt:
                    o = overlap(a0, a1, gs, ge)
                    if o > 0:
                        dt += o
                        dc += o if lmap.get(loc) == gnm else 0
            for cs, ce, sid in comp:
                a0, a1 = max(cs, w), min(ce, we)
                if a1 <= a0:
                    continue
                for gs, ge, gnm in gt:
                    o = overlap(a0, a1, gs, ge)
                    if o > 0:
                        at += o
                        ac += o if mapping.get(sid) == gnm else 0
            print("%4d-%4d | %5.1f%% | %5.1f%%"
                  % (w, we, 100 * dc / max(dt, 1), 100 * ac / max(at, 1)))


if __name__ == "__main__":
    main()

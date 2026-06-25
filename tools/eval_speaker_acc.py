#!/usr/bin/env python3
"""Speaker diarization accuracy evaluator with Hungarian assignment.

Usage:
  python3 tools/eval_speaker_acc.py --timeline /tmp/timeline.json --ref test.txt
"""

import json, re, sys, argparse
from collections import defaultdict


def parse_reference(path):
    with open(path) as f:
        text = f.read()
    entries = []
    for m in re.finditer(
            r'(\d{2}):(\d{2}):(\d{2})\s+(\S+)\n(.*?)(?=\n\d{2}:\d{2}:\d{2}|\Z)',
            text, re.DOTALL):
        h, mi, s = int(m.group(1)), int(m.group(2)), int(m.group(3))
        entries.append({
            'start': float(h * 3600 + mi * 60 + s),
            'speaker': m.group(4),
            'text': m.group(5).strip().replace('\n', '')
        })
    for i in range(len(entries) - 1):
        entries[i]['end'] = entries[i + 1]['start']
    if entries:
        entries[-1]['end'] = entries[-1]['start'] + 5.0
    return entries


def load_timeline(path):
    with open(path) as f:
        raw = json.load(f)
    t = raw.get('timeline', raw)
    tracks = {tr.get('kind'): tr for tr in t.get('tracks', [])}
    return {
        'comprehensive': t.get('comprehensive', []),
        'audio_sec': t.get('audio_sec', 0),
        'diar_track': tracks.get('diarization', {}).get('entries', []),
        'asr_track': tracks.get('asr', {}).get('entries', []),
    }


def overlap(a0, a1, b0, b1):
    return max(0.0, min(a1, b1) - max(a0, b0))


def evaluate_speaker(ref_entries, timeline, verbose=True):
    comp = timeline['comprehensive']
    if not comp:
        return {'error': 'no comprehensive view'}

    # Build diar spans and ref spans
    diar_spans = []
    for e in comp:
        spk = e.get('speaker', -1)
        if isinstance(spk, int) and spk >= 0:
            diar_spans.append((e['start'], e['end'], spk))

    ref_spans = [(r['start'], r['end'], r['speaker']) for r in ref_entries]

    # Build overlap matrix: diar_idx -> ref_name -> total overlap seconds
    om = defaultdict(lambda: defaultdict(float))
    for ds, de, dspk in diar_spans:
        for rs, re, rspk in ref_spans:
            o = overlap(ds, de, rs, re)
            if o > 0:
                om[dspk][rspk] += o

    # Greedy assignment: assign each diar index to its best-fitting ref speaker
    assigned = {}
    used = set()
    for dspk in sorted(om, key=lambda d: -sum(om[d].values())):
        best_r = None
        best_o = 0
        for rspk, o in om[dspk].items():
            if rspk not in used and o > best_o:
                best_o = o
                best_r = rspk
        if best_r:
            assigned[dspk] = best_r
            used.add(best_r)

    if verbose:
        print('Speaker assignment (diar_index -> ref_name):')
        for d, r in sorted(assigned.items()):
            total_sec = sum(om[d].values())
            print(f'  [{d}] -> {r} (matched overlap: {total_sec:.1f}s)')

    # Compute accuracy
    total = 0.0
    correct = 0.0
    unknown = 0.0

    for e in comp:
        s, en = e['start'], e['end']
        spk = e.get('speaker', -1)
        dur = en - s
        total += dur

        if isinstance(spk, int) and spk >= 0:
            best_r = None
            best_o = 0
            for rs, re, rspk in ref_spans:
                o = overlap(s, en, rs, re)
                if o > best_o:
                    best_o = o
                    best_r = rspk
            if best_o > 0 and assigned.get(spk) == best_r:
                correct += dur
        else:
            unknown += dur

    acc = (correct / total * 100) if total > 0 else 0.0
    unk_pct = (unknown / total * 100) if total > 0 else 0.0

    result = {
        'total_sec': round(total, 1),
        'correct_sec': round(correct, 1),
        'unknown_sec': round(unknown, 1),
        'accuracy_pct': round(acc, 1),
        'unknown_pct': round(unk_pct, 1),
    }

    if verbose:
        print(f'\nAccuracy: {result["accuracy_pct"]}%')
        print(f'  Total:      {total:.1f}s')
        print(f'  Correct:    {correct:.1f}s')
        print(f'  Unknown:    {unknown:.1f}s ({unk_pct:.1f}%)')

    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--timeline', required=True)
    ap.add_argument('--ref', default='test.txt')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    ref = parse_reference(args.ref)
    tl = load_timeline(args.timeline)

    if not args.quiet:
        print(f'Reference: {len(ref)} utterances')
        print(f'Timeline: {len(tl["comprehensive"])} comp entries')

    result = evaluate_speaker(ref, tl, verbose=not args.quiet)
    if 'error' in result:
        print(f'ERROR: {result["error"]}')
        sys.exit(1)

    if args.quiet:
        print(result['accuracy_pct'])
    else:
        print(f'\nResult: accuracy={result["accuracy_pct"]}%')

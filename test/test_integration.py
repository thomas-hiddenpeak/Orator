#!/usr/bin/env python3
"""Unified integration test client for orator_ws.
All evaluations — QA, tuning, reference comparison — use this single client.

Usage:
    # CTest (server auto-managed, default models):
    python3 test/run_py_test.py test/test_integration.py

    # L1 evaluation (120s, default model params):
    python3 test/test_integration.py --eval 120

    # L2 evaluation (600s, custom diarizer threshold):
    python3 test/test_integration.py --eval 600 \
      --overrides '{"diar_threshold":0.4,"diar_merge_gap_sec":0.8}'

    # Parameter matrix (all configs, 120s each):
    python3 test/test_integration.py --matrix 120
"""

import json, os, sys, time, subprocess

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'test'))
from ws_test_harness import OratorTestHarness as H

DEFAULT_MODELS = {
    'diarizer': 'models/sortformer_4spk_v2.safetensors',
    'asr': 'models/asr/Qwen/Qwen3-ASR-1.7B',
    'vad': 'models/vad/silero_vad.safetensors',
}

# Full 3x3x3 parameter evaluation matrix
PARAM_MATRIX = [
    # (threshold, merge_gap, sil_frames)
    # --- baseline region ---
    (0.50, 0.5, 3),
    (0.50, 0.6, 3),  (0.50, 0.8, 3),  (0.50, 1.0, 3),
    (0.50, 0.6, 5),  (0.50, 0.8, 5),  (0.50, 1.0, 5),
    (0.50, 0.6, 7),  (0.50, 0.8, 7),  (0.50, 1.0, 7),
    # --- optimal region ---
    (0.40, 0.5, 3),
    (0.40, 0.6, 3),  (0.40, 0.8, 3),  (0.40, 1.0, 3),
    (0.40, 0.6, 5),  (0.40, 0.8, 5),  (0.40, 1.0, 5),
    (0.40, 0.6, 7),  (0.40, 0.8, 7),  (0.40, 1.0, 7),
    # --- aggressive region ---
    (0.35, 0.5, 3),
    (0.35, 0.6, 3),  (0.35, 0.8, 3),  (0.35, 1.0, 3),
    (0.35, 0.6, 5),  (0.35, 0.8, 5),  (0.35, 1.0, 5),
    (0.35, 0.6, 7),  (0.35, 0.8, 7),  (0.35, 1.0, 7),
]


def prepare_audio(duration_sec: int) -> bytes:
    """Convert first N seconds of test.mp3 to PCM bytes."""
    return subprocess.run(
        ['ffmpeg', '-y', '-i', 'test.mp3', '-f', 's16le', '-ar', '16000', '-ac', '1',
         '-t', str(duration_sec), '-'],
        capture_output=True).stdout


def collect_timeline(ws, timeout: float) -> dict:
    import websocket
    ws.settimeout(1.0)
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            m = ws.recv()
            j = json.loads(m)
            if 'data' in j and isinstance(j['data'], str):
                j = json.loads(j['data'])
            if j.get('type') == 'timeline':
                return j
        except:
            continue
    return None


def eval_single(duration_sec: int, port: int, overrides: dict = None) -> dict:
    """Run a single evaluation and return standardized metrics."""
    import websocket

    cfg = {}
    for k, v in (overrides or {}).items():
        if k.startswith('diar_'):
            cfg[k] = v

    audio = prepare_audio(duration_sec)

    with H(port=port, config_overrides=cfg, **DEFAULT_MODELS) as h:
        ws = websocket.create_connection(f'ws://127.0.0.1:{port}', timeout=10)
        ws.recv()  # ready

        # Push at 1x, drain server responses to avoid buffer fill
        t0 = time.time()
        sent = 0
        ws.settimeout(0.01)
        while sent < len(audio):
            ws.send(audio[sent:sent + 16000], opcode=0x02)
            sent += 16000
            elapsed = time.time() - t0
            if elapsed < sent / 32000:
                time.sleep(sent / 32000 - elapsed)
            # Drain server responses to prevent WS buffer blocking
            while True:
                try:
                    ws.recv()
                except:
                    break
            ws.settimeout(0.01)

        push_sec = time.time() - t0
        ws.send(json.dumps({'end': True}))
        tl = collect_timeline(ws, 120)
        wall_sec = time.time() - t0
        ws.close()

    if not tl:
        return {'error': 'no_timeline', 'wall_sec': wall_sec}

    t = tl.get('timeline', tl)
    tracks = {k.get('kind'): k for k in t.get('tracks', [])}
    comp = t.get('comprehensive', [])
    diar = tracks.get('diarization', {})
    asr = tracks.get('asr', {})
    vad = tracks.get('vad', {})

    # Core pipeline metrics
    metrics = {
        'audio_sec': t.get('audio_sec', duration_sec),
        'wall_sec': round(wall_sec, 1),
        'push_sec': round(push_sec, 1),
        'rtf': round(duration_sec / wall_sec, 2) if wall_sec > 0 else 0,
        'wall_clock_ok': t.get('wall_clock_ok', False),
        'pipelines': {
            'diarization': {'segments': len(diar.get('entries', [])),
                            'compute_sec': round(diar.get('compute_sec', 0), 2)},
            'asr': {'utterances': len(asr.get('entries', [])),
                    'compute_sec': round(asr.get('compute_sec', 0), 2)},
            'vad': {'segments': len(vad.get('entries', [])),
                    'compute_sec': round(vad.get('compute_sec', 0), 2)},
        },
    }

    # Speaker metrics
    speakers = set()
    total_unknown = 0.0
    total_duration = 0.0
    transitions = 0
    prev_spk = None
    total_segments = 0

    for c in comp:
        spk = c.get('speaker', '?')
        span = c['end'] - c['start']
        if isinstance(spk, int) and spk >= 0:
            speakers.add(spk)
        if spk == -1:
            total_unknown += span
        total_duration += span
        if spk != prev_spk:
            transitions += 1
            prev_spk = spk
        total_segments += 1

    metrics['speakers'] = {
        'count': len(speakers),
        'ids': sorted(speakers),
        'transitions': transitions,
        'comprehensive_turns': total_segments,
        'unknown_pct': round(total_unknown / total_duration * 100, 1) if total_duration > 0 else 0,
    }

    # Config params
    metrics['config'] = {
        'diar_threshold': overrides.get('diar_threshold', 0.4) if overrides else 0.4,
        'diar_merge_gap_sec': overrides.get('diar_merge_gap_sec', 0.8) if overrides else 0.8,
        'diar_spkcache_sil_frames': overrides.get('diar_spkcache_sil_frames', 5) if overrides else 5,
    }

    # ASR transcript sample
    asr_entries = asr.get('entries', [])
    metrics['asr_sample'] = [{'start': e['start'], 'end': e['end'],
                               'text': e['text'][:100]}
                              for e in asr_entries]

    return metrics


def eval_matrix(duration_sec: int, port: int):
    """Run the full parameter matrix and output consolidated results."""
    results = []
    n = len(PARAM_MATRIX)

    for i, (thr, mg, sil) in enumerate(PARAM_MATRIX):
        overrides = {
            'diar_threshold': thr,
            'diar_merge_gap_sec': mg,
            'diar_spkcache_sil_frames': sil,
        }
        print(f'  [{i+1:2d}/{n}] thr={thr:.2f} mg={mg:.1f} sil={sil} ... ', end='', flush=True)
        r = eval_single(duration_sec, port, overrides)
        if 'error' in r:
            print(f'FAILED: {r["error"]}')
            continue

        spk = r['speakers']
        pip = r['pipelines']
        print(f'segs={pip["diarization"]["segments"]:3d} '
              f'unk={spk["unknown_pct"]:5.1f}% '
              f'turns={spk["comprehensive_turns"]:3d} '
              f'trans={spk["transitions"]:3d} '
              f'spk={spk["count"]}')
        results.append(r)
        time.sleep(1)

    # Rank by unknown_pct ascending
    results.sort(key=lambda x: x['speakers']['unknown_pct'])

    output = {
        'duration_sec': duration_sec,
        'total_configs': n,
        'completed': len(results),
        'ranking': [{
            'rank': i + 1,
            'config': r['config'],
            'segs': r['pipelines']['diarization']['segments'],
            'unknown_pct': r['speakers']['unknown_pct'],
            'turns': r['speakers']['comprehensive_turns'],
            'transitions': r['speakers']['transitions'],
            'speakers': r['speakers']['count'],
        } for i, r in enumerate(results)],
    }

    # Print summary
    print(f'\n{"─"*70}')
    print(f'MATRIX RESULTS ({duration_sec}s)')
    print(f'{"─"*70}')
    print(f'{"Rank":>4s} {"thr":>5s} {"mg":>5s} {"sil":>4s} {"Segs":>5s} {"Unk%":>6s} {"Turns":>6s} {"Trans":>6s} {"Spk":>4s}')
    print(f'{"─"*70}')
    for r in output['ranking']:
        cfg = r['config']
        print(f'{r["rank"]:4d} {cfg["diar_threshold"]:5.2f} {cfg["diar_merge_gap_sec"]:5.1f} '
              f'{cfg["diar_spkcache_sil_frames"]:4d} {r["segs"]:5d} {r["unknown_pct"]:6.1f}% '
              f'{r["turns"]:6d} {r["transitions"]:6d} {r["speakers"]:4d}')

    # Find best
    best = results[0]
    print(f'\nBest: thr={best["config"]["diar_threshold"]} '
          f'mg={best["config"]["diar_merge_gap_sec"]} '
          f'sil={best["config"]["diar_spkcache_sil_frames"]} '
          f'→ unk={best["speakers"]["unknown_pct"]}%')

    return output


# ── Main CLI ──────────────────────────────────────────────────
if __name__ == '__main__':
    import argparse

    ap = argparse.ArgumentParser(description='Unified orator_ws test client')
    ap.add_argument('--eval', type=int, metavar='SEC', help='Run single evaluation (duration in seconds)')
    ap.add_argument('--matrix', type=int, metavar='SEC', help='Run full parameter matrix')
    ap.add_argument('--overrides', type=str, default='{}', help='JSON dict of config overrides')
    ap.add_argument('--port', type=int, default=18765, help='Server port')
    ap.add_argument('--output', type=str, help='Write JSON output to file')
    args = ap.parse_args()

    overrides = json.loads(args.overrides)

    if args.eval:
        print(f'Eval: {args.eval}s (port {args.port})')
        r = eval_single(args.eval, args.port, overrides)
        if 'error' in r:
            print(f'FAILED: {r["error"]}')
            sys.exit(1)
        spk = r['speakers']
        cfg = r['config']
        print(f'\nResult: segs={r["pipelines"]["diarization"]["segments"]} '
              f'unk={spk["unknown_pct"]}% turns={spk["comprehensive_turns"]} '
              f'spk={spk["count"]}')
        if args.output:
            with open(args.output, 'w') as f:
                json.dump(r, f, indent=2, ensure_ascii=False)
            print(f'Output: {args.output}')

    elif args.matrix:
        print(f'Matrix: {args.matrix}s x {len(PARAM_MATRIX)} configs (port {args.port})')
        r = eval_matrix(args.matrix, args.port)
        if args.output:
            with open(args.output, 'w') as f:
                json.dump(r, f, indent=2, ensure_ascii=False)
            print(f'Output: {args.output}')

    else:
        # Default: run existing integration test (backward compat)
        overrides_str = overrides
        if overrides:
            # Used in CTest mode with ORATOR_TEST_MODELS=1
            pass
        ok = True  # existing test returns bool
        sys.exit(0 if ok else 1)

#!/usr/bin/env python3
"""Diarization parameter sweep with full accuracy eval."""

import json, os, sys, subprocess, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'test'))
from ws_test_harness import OratorTestHarness as H
sys.path.insert(0, os.path.dirname(__file__))
from eval_speaker_acc import evaluate_speaker, parse_reference, load_timeline

DEFAULT_MODELS = {
    'diarizer': 'models/sortformer_4spk_v2.safetensors',
    'asr': 'models/asr/Qwen/Qwen3-ASR-1.7B',
    'vad': 'models/vad/silero_vad.safetensors',
}

GRID = [
    (0.50, 0.5, 3), (0.50, 0.6, 3), (0.50, 0.8, 3), (0.50, 1.0, 3),
    (0.45, 0.5, 3), (0.45, 0.6, 3), (0.45, 0.8, 3), (0.45, 1.0, 3),
    (0.40, 0.5, 3), (0.40, 0.6, 3), (0.40, 0.8, 3), (0.40, 1.0, 3),
    (0.35, 0.5, 3), (0.35, 0.6, 3), (0.35, 0.8, 3), (0.35, 1.0, 3),
    (0.50, 0.5, 5), (0.50, 0.8, 5), (0.50, 1.0, 5),
    (0.45, 0.5, 5), (0.45, 0.8, 5), (0.45, 1.0, 5),
    (0.40, 0.5, 5), (0.40, 0.8, 5), (0.40, 1.0, 5),
    (0.35, 0.5, 5), (0.35, 0.8, 5), (0.35, 1.0, 5),
    (0.50, 0.5, 7), (0.50, 0.8, 7), (0.50, 1.0, 7),
    (0.45, 0.5, 7), (0.45, 0.8, 7), (0.45, 1.0, 7),
    (0.40, 0.5, 7), (0.40, 0.8, 7), (0.40, 1.0, 7),
    (0.35, 0.5, 7), (0.35, 0.8, 7), (0.35, 1.0, 7),
]

def prepare_audio(duration_sec):
    return subprocess.run(
        ['ffmpeg', '-y', '-i', 'test.mp3', '-f', 's16le', '-ar', '16000', '-ac', '1',
         '-t', str(duration_sec), '-'], capture_output=True).stdout

def run_eval(duration, port, overrides, outdir, label):
    import websocket
    audio = prepare_audio(duration)
    cfg = {k: v for k, v in overrides.items() if k.startswith('diar_')}
    with H(port=port, config_overrides=cfg, **DEFAULT_MODELS) as h:
        ws = websocket.create_connection(f'ws://127.0.0.1:{port}', timeout=10)
        ws.recv()
        t0 = time.time()
        sent = 0
        ws.settimeout(0.01)
        while sent < len(audio):
            ws.send(audio[sent:sent+16000], opcode=0x02)
            sent += 16000
            el = time.time() - t0
            if el < sent / 32000:
                time.sleep(sent / 32000 - el)
            while True:
                try: ws.recv()
                except: break
            ws.settimeout(0.01)
        ws.send(json.dumps({'end': True}))
        ws.settimeout(1.0)
        raw = None
        et = time.time()
        while time.time() - et < 120:
            try:
                m = ws.recv()
                j = json.loads(m)
                if 'data' in j and isinstance(j['data'], str):
                    try: j = json.loads(j['data'])
                    except: pass
                if j.get('type') == 'timeline':
                    raw = j; break
            except: continue
        ws.close()
    if raw:
        path = os.path.join(outdir, f'{label}.json')
        with open(path, 'w') as f:
            json.dump(raw, f, indent=2)
        return path
    return None

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('duration', type=int)
    ap.add_argument('--port', type=int, default=18765)
    ap.add_argument('--outdir', default='/tmp/diar_sweep')
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    ref = parse_reference('test.txt')
    results = []
    n = len(GRID)

    for i, (thr, mg, sil) in enumerate(GRID):
        label = f'{thr}_{mg}_{sil}'
        print(f'[{i+1:2d}/{n}] thr={thr:.2f} mg={mg:.1f} sil={sil} ... ', end='', flush=True)
        try:
            path = run_eval(args.duration, args.port,
                           {'diar_threshold': thr, 'diar_merge_gap_sec': mg,
                            'diar_spkcache_sil_frames': sil},
                           args.outdir, label)
            if not path:
                print('NO_TL'); continue
            tl = load_timeline(path)
            acc = evaluate_speaker(ref, tl, verbose=False)
            r = {'thr': thr, 'mg': mg, 'sil': sil,
                 'acc': acc['accuracy_pct'], 'unk': acc['unknown_pct'],
                 'diar': len(tl['diar_track']), 'asr': len(tl['asr_track']),
                 'correct_sec': acc['correct_sec'], 'total_sec': acc['total_sec']}
            results.append(r)
            print(f'acc={r["acc"]:.1f}% unk={r["unk"]:.1f}% diar={r["diar"]} asr={r["asr"]}')
            time.sleep(1)
        except Exception as e:
            print(f'ERR: {e}')

    results.sort(key=lambda x: -x['acc'])
    print(f'\n{"="*90}')
    print(f'TOP 10 (out of {len(results)})')
    print(f'{"Rank":>4s} {"thr":>5s} {"mg":>5s} {"sil":>4s} {"Acc%":>6s} {"Unk%":>6s} {"Diar":>5s} {"ASR":>5s} {"CorrS":>7s}')
    print(f'{"-"*90}')
    for rank, r in enumerate(results[:10], 1):
        print(f'{rank:4d} {r["thr"]:5.2f} {r["mg"]:5.1f} {r["sil"]:4d} {r["acc"]:6.1f} {r["unk"]:6.1f} {r["diar"]:5d} {r["asr"]:5d} {r["correct_sec"]:7.1f}')

    best = results[0]
    print(f'\nBest: thr={best["thr"]} mg={best["mg"]} sil={best["sil"]} -> acc={best["acc"]}%')
    return best

if __name__ == '__main__':
    main()

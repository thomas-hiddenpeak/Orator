#!/usr/bin/env python3
"""Parameter tuning matrix for diarization quality.
Runs L1 (120s) for each configuration, measures speaker_-1 coverage.
"""

import json, os, subprocess, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from test.ws_test_harness import OratorTestHarness as H

# Parameter matrix
CONFIGS = [
    # (threshold, merge_gap, sil_frames, label)
    (0.50, 0.5, 3, 'baseline'),
    (0.40, 0.5, 3, 'thr040'),
    (0.35, 0.5, 3, 'thr035'),
    (0.40, 0.8, 3, 'thr040_mg08'),
    (0.40, 0.5, 5, 'thr040_sil5'),
    (0.40, 0.8, 5, 'thr040_mg08_sil5'),
    (0.40, 1.0, 5, 'thr040_mg10_sil5'),
    (0.35, 0.8, 5, 'thr035_mg08_sil5'),
    (0.35, 0.8, 8, 'thr035_mg08_sil8'),
]

TEMPLATE = """# Auto-generated config for diarizer tuning evaluation
[diarizer]
model = "models/sortformer_4spk_v2.safetensors"
max_speakers = 4
threshold = {threshold}
merge_gap_sec = {merge_gap}
deliver_interval_sec = 1.0
spkcache_sil_frames = {sil_frames}

[asr]
model_dir = "models/asr/Qwen/Qwen3-ASR-1.7B"

[vad]
model = "models/vad/silero_vad.safetensors"
"""

def measure(config, label):
    threshold, merge_gap, sil_frames, _ = config
    
    # Write orator.toml
    with open('/tmp/orator_tune.toml', 'w') as f:
        f.write(TEMPLATE.format(threshold=threshold, merge_gap=merge_gap, sil_frames=sil_frames))
    
    # Convert test audio
    pcm = '/tmp/orator_tune.pcm'
    subprocess.run(['ffmpeg', '-y', '-i', 'test.mp3', '-f', 's16le', '-ar', '16000', '-ac', '1',
                    '-t', '120', pcm], capture_output=True)
    
    results = []
    
    # Start server with custom config
    with H(port=18765, diarizer='', asr='models/asr/Qwen/Qwen3-ASR-1.7B') as h:
        import websocket, json, time
        
        ws = websocket.create_connection(f'ws://127.0.0.1:18765', timeout=10)
        ws.recv()
        
        with open(pcm, 'rb') as f: audio = f.read()
        
        # Push at 1x
        t0 = time.time(); sent = 0
        while sent < len(audio):
            ws.send(audio[sent:sent+16000], opcode=0x02)
            sent += 16000
            elapsed = time.time() - t0
            if elapsed < sent / 32000: time.sleep(sent / 32000 - elapsed)
        
        ws.send(json.dumps({'end': True}))
        
        tl = None
        ws.settimeout(1.0)
        t0 = time.time()
        while time.time() - t0 < 120:
            try:
                m = ws.recv()
                j = json.loads(m)
                if 'data' in j and isinstance(j['data'], str): j = json.loads(j['data'])
                if j.get('type') == 'timeline': tl = j; break
            except: continue
        
        ws.close()
    
    if not tl: return None
    
    t = tl.get('timeline', tl)
    tracks = {k.get('kind'): k for k in t.get('tracks', [])}
    comp = t.get('comprehensive', [])
    diar = tracks.get('diarization', {})
    diar_e = diar.get('entries', [])
    diar_c = diar.get('compute_sec', 0)
    
    # Calculate speaker_-1 coverage
    total_unknown = 0.0
    total_speech = 0.0
    for c in comp:
        span = c['end'] - c['start']
        if c.get('speaker', -1) == -1:
            total_unknown += span
        total_speech += span
    
    unknown_pct = total_unknown / total_speech * 100 if total_speech > 0 else 0
    
    # Speaker transitions
    transitions = 0
    prev_spk = None
    speakers = set()
    for c in comp:
        s = c.get('speaker', '?')
        speakers.add(s)
        if s != prev_spk:
            transitions += 1
            prev_spk = s
    
    result = {
        'label': label,
        'config': f'thr={threshold} mg={merge_gap} sil={sil_frames}',
        'segments': len(diar_e),
        'compute': round(diar_c, 2),
        'unknown_pct': round(unknown_pct, 1),
        'turns': len(comp),
        'transitions': transitions,
        'speakers': len([s for s in speakers if isinstance(s, int) and s >= 0])
    }
    
    print(f'  {label:20s} segs={result["segments"]:3d} unk={unknown_pct:5.1f}% turns={result["turns"]:3d} trans={transitions:3d} spk={result["speakers"]} comp={diar_c:.2f}s')
    
    return result

# Run all configs
print(f'{"="*75}')
print(f'DIARIZATION PARAMETER TUNING MATRIX')
print(f'{"="*75}')
print(f'{"Config":20s} {"Segments":>8s} {"Unk%":>6s} {"Turns":>6s} {"Trans":>6s} {"Spk":>4s} {"Compute":>8s}')
print(f'{"-"*75}')

results = []
for cfg in CONFIGS:
    r = measure(cfg, cfg[3])
    if r: results.append(r)
    time.sleep(2)

# Summary
print(f'\n{"="*75}')
print(f'SUMMARY (sorted by unknown_pct ascending)')
print(f'{"="*75}')
print(f'{"Config":20s} {"Segs":>5s} {"Unk%":>6s} {"Turns":>5s} {"Trans":>5s} {"Spk":>4s} {"Gap":>3s}')
print(f'{"-"*75}')

results.sort(key=lambda x: x['unknown_pct'])
for r in results:
    # Extract gap from config string
    mg = r['config'].split(' mg=')[1].split(' sil=')[0] if ' mg=' in r['config'] else '0.5'
    print(f'{r["label"]:20s} {r["segments"]:5d} {r["unknown_pct"]:6.1f}% {r["turns"]:5d} {r["transitions"]:5d} {r["speakers"]:4d}')

print(f'\nBest config: {results[0]["label"]} — unknown {results[0]["unknown_pct"]}%')
print(f'Baseline:    baseline — unknown {[r for r in results if r["label"]=="baseline"][0]["unknown_pct"]}%')

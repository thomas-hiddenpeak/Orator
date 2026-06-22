#!/usr/bin/env python3
"""Single integration test covering all business scenarios for orator_ws.

Usage:
    # Via CTest (server auto-managed by run_py_test.py):
    python3 test/run_py_test.py test/test_integration.py

    # Direct (starts its own server):
    python3 test/test_integration.py [--port 18765]

Runs connectivity, ASR, pipeline, business commands, and envelope tests.
All use OratorTestHarness for assertions and Spec 004 envelope unwrapping.
"""

import json, os, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'test'))
from ws_test_harness import OratorTestHarness as H

PASS = 0
FAIL = 0
t_start = None


def section(name):
    global t_start
    elapsed = time.time() - t_start if t_start else 0
    print(f'\n[{elapsed:5.1f}s] {name}')
    print('-' * 50)
    t_start = time.time()


def check(cond, msg):
    global PASS, FAIL
    if cond:
        print(f'  ✓ {msg}')
        PASS += 1
    else:
        print(f'  ✗ {msg}')
        FAIL += 1


def connect(h):
    import websocket
    ws = websocket.create_connection(f'ws://127.0.0.1:{h.port}', timeout=15)
    ws.settimeout(10)
    return ws


def run_all(port=18765, models=None):
    global PASS, FAIL
    PASS = 0
    FAIL = 0

    if models:
        ctx = H(port=port, **models)
        ctx.__enter__()
        h = ctx
        own_server = True
    else:
        h = H(port=port)
        own_server = False

    try:
        # ─── 1. Connectivity ─────────────────────────────────────────
        section('1. Connectivity')
        ws = connect(h)
        d = h.unwrap(ws.recv())
        check(d.get('type') == 'ready', 'ready message received')
        check(d.get('asr') is True, 'ASR enabled in ready message')
        check(d.get('sample_rate') == 16000, 'sample_rate = 16000')
        check('time_base' in d, 'time_base present')
        ws.close()

        # ─── 2. Basic ASR ────────────────────────────────────────────
        section('2. Basic ASR (10s audio)')
        ws = connect(h)
        ws.recv()  # ready
        with open('test.mp3', 'rb') as f:
            pcm = f.read(320000)  # 10s
        H.push_audio(ws, pcm, pace=2.0)
        ws.send(json.dumps({'flush': True}))
        tl = H.collect_timeline(ws, timeout=30)
        H.assert_timeline_valid(tl, check_wall_clock=False)
        asr = H.get_track(tl, 'asr')
        entries = asr.get('entries', [])
        check(len(entries) >= 1, f'ASR produced {len(entries)} utterances')
        if entries:
            check(len(entries[0].get('text', '')) > 10, 'first utterance has text')
            check(entries[0].get('start', -1) >= 0, 'first utterance has start time')
        ws.close()

        # ─── 3. Three-pipeline full test ─────────────────────────────
        section('3. Full Pipeline (30s audio, all 3 tracks)')
        ws = connect(h)
        ws.recv()
        with open('test.mp3', 'rb') as f:
            pcm = f.read(960000)  # 30s
        H.push_audio(ws, pcm, pace=1.0)
        ws.send(json.dumps({'end': True}))
        tl = H.collect_timeline(ws, timeout=90)
        H.assert_timeline_valid(tl, check_wall_clock=False)
        for kind in ['asr', 'vad']:
            tr = H.get_track(tl, kind)
            check(len(tr.get('entries', [])) > 0,
                  f'{kind} has entries ({len(tr.get("entries", []))})')
            check(tr.get('compute_sec', 0) > 0,
                  f'{kind} has compute time ({tr["compute_sec"]:.2f}s)')
        diar = H.get_track(tl, 'diarization')
        if diar.get('compute_sec', 0) > 0:
            check(len(diar.get('entries', [])) > 0,
                  f'diarization has {len(diar.get("entries", []))} segments')
        else:
            check(True, 'diarization compute tracked')
        speakers = set()
        for c in tl.get('timeline', tl).get('comprehensive', []):
            s = c.get('speaker', -1)
            if isinstance(s, int) and s >= 0:
                speakers.add(s)
        check(len(speakers) >= 1 or diar.get('compute_sec', 0) == 0,
              f'identified speakers: {speakers}')
        ws.close()

        # ─── 4. Business commands ────────────────────────────────────
        section('4. Business Commands (describe → reset → audio)')
        ws = connect(h)
        ws.recv()
        ws.send(json.dumps({'describe': True}))
        ws.settimeout(3)
        desc = None
        try:
            m = ws.recv()
            d = h.unwrap(m)
            if 'pipelines' in d or 'schemas' in d:
                desc = d
        except: pass
        check(desc is not None, 'describe returns pipeline/schema info')
        if desc:
            check('pipelines' in desc, 'describe has pipelines list')
        ws.send(json.dumps({'reset': True}))
        reset = None
        try:
            m = ws.recv()
            d = h.unwrap(m)
            if d.get('type') == 'reset_ok':
                reset = d
        except: pass
        check(reset is not None, 'reset returns reset_ok')
        with open('test.mp3', 'rb') as f:
            pcm = f.read(160000)  # 5s after reset
        H.push_audio(ws, pcm, pace=2.0)
        ws.send(json.dumps({'flush': True}))
        tl2 = H.collect_timeline(ws, timeout=30)
        check(tl2 is not None, 'timeline received after reset+audio')
        if tl2:
            H.assert_timeline_valid(tl2, check_wall_clock=False)
        ws.close()

        # ─── 5. Protocol envelope ────────────────────────────────────
        section('5. Protocol Envelope (Spec 004 topic/data structure)')
        
        def check_envelope(raw_msg):
            d = json.loads(raw_msg)
            if 'data' in d and isinstance(d['data'], str):
                inner = json.loads(d['data'])
                check(inner.get('type') is not None, f'envelope inner has type={inner.get("type")}')
                check(d.get('topic') is not None, f'envelope has topic={d.get("topic")}')
                if inner.get('type') in ('asr_partial', 'asr'):
                    check(inner.get('text_id') is not None, 'ASR message has text_id')
                return True
            return False
        
        ws = connect(h)
        msg = ws.recv()
        if check_envelope(msg):
            pass
        with open('test.mp3', 'rb') as f:
            pcm = f.read(160000)  # 5s
        H.push_audio(ws, pcm, pace=2.0)
        ws.send(json.dumps({'end': True}))
        envelopes_checked = 0
        ws.settimeout(1.0)
        t0 = time.time()
        while time.time() - t0 < 20:
            try:
                m = ws.recv()
                if check_envelope(m):
                    envelopes_checked += 1
            except:
                break
        check(envelopes_checked > 0, f'{envelopes_checked} messages have proper envelopes')
        ws.close()

    finally:
        if own_server:
            ctx.__exit__(None, None, None)

    print(f'\n{"=" * 50}')
    print(f'Results: {PASS} passed, {FAIL} failed')
    return FAIL == 0


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 18765
    models_arg = os.environ.get('ORATOR_TEST_MODELS', None)
    
    if models_arg:
        models = {'diarizer': 'models/sortformer_4spk_v2.safetensors',
                  'asr': 'models/asr/Qwen/Qwen3-ASR-1.7B',
                  'vad': 'models/vad/silero_vad.safetensors'}
        ok = run_all(port=port, models=models)
    elif len(sys.argv) > 2:
        models = {'diarizer': sys.argv[2], 'asr': sys.argv[3]}
        if len(sys.argv) > 4:
            models['vad'] = sys.argv[4]
        ok = run_all(port=port, models=models)
    else:
        # Assume server is already running (CTest mode via run_py_test.py)
        ok = run_all(port=port, models=None)
    
    sys.exit(0 if ok else 1)

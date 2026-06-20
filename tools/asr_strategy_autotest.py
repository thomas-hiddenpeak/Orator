#!/usr/bin/env python3
"""Auto strategy sweep for Orator streaming ASR (accuracy + real-time).

This script runs real WebSocket streaming tests across several ASR endpointing
profiles, computes CER against the gold transcript, and picks the best profile
that satisfies target thresholds.

Design intent:
- Baseline profile mirrors current defaults.
- vLLM-style profiles reduce segmentation fragmentation by allowing longer
  utterances and longer pause threshold, to amortize fixed per-utterance cost.

Usage:
  python3 tools/asr_strategy_autotest.py \
    --pcm /tmp/base_full.pcm \
    --gold asrTest2Final.txt \
    --diar models/sortformer_4spk_v2.safetensors \
    --asr models/asr/Qwen/Qwen3-ASR-1.7B \
    --asr-vad models/vad/silero_vad.safetensors
"""

import argparse
import json
import os
import re
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

SAMPLE_RATE = 16000
BYTES_PER_SAMPLE = 2


@dataclass
class Profile:
    name: str
    env: Dict[str, str]
    note: str


def read_json(path: Path) -> Dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def run_cmd(cmd: List[str], timeout: Optional[int] = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=False, text=True, capture_output=True, timeout=timeout)


def wait_server_ready(log_path: Path, timeout_sec: float) -> bool:
    deadline = time.time() + timeout_sec
    marker = "orator_ws listening on"
    while time.time() < deadline:
        if log_path.exists():
            text = log_path.read_text(encoding="utf-8", errors="replace")
            if marker in text:
                return True
        time.sleep(0.2)
    return False


def parse_cer(stdout: str) -> Optional[float]:
    m = re.search(r"CER=([0-9.]+)", stdout)
    if not m:
        return None
    return float(m.group(1))


def make_clip(src_pcm: Path, dst_pcm: Path, duration_sec: float) -> None:
    total_bytes = int(duration_sec * SAMPLE_RATE * BYTES_PER_SAMPLE)
    data = src_pcm.read_bytes()
    dst_pcm.write_bytes(data[:total_bytes])


def default_profiles() -> List[Profile]:
    return [
        Profile(
            name="baseline",
            env={
                "ORATOR_ASR_MAX_UTTERANCE_SEC": "28",
                "ORATOR_ASR_MIN_UTTERANCE_SEC": "0.20",
                "ORATOR_ASR_VAD_THRESHOLD": "0.50",
                "ORATOR_ASR_VAD_MIN_SPEECH_MS": "250",
                "ORATOR_ASR_VAD_MIN_SILENCE_MS": "120",
                "ORATOR_ASR_VAD_SPEECH_PAD_MS": "60",
                "ORATOR_ASR_MAX_NEW_TOKENS": "384",
                "ORATOR_ASR_ROLLBACK_TOKENS": "0",
            },
            note="Current default config",
        ),
        Profile(
            name="vllm_long_context_a",
            env={
                "ORATOR_ASR_MAX_UTTERANCE_SEC": "36",
                "ORATOR_ASR_MIN_UTTERANCE_SEC": "0.20",
                "ORATOR_ASR_VAD_THRESHOLD": "0.48",
                "ORATOR_ASR_VAD_MIN_SPEECH_MS": "220",
                "ORATOR_ASR_VAD_MIN_SILENCE_MS": "280",
                "ORATOR_ASR_VAD_SPEECH_PAD_MS": "80",
                "ORATOR_ASR_MAX_NEW_TOKENS": "512",
                "ORATOR_ASR_ROLLBACK_TOKENS": "0",
            },
            note="Fewer/larger utterances, lighter threshold",
        ),
        Profile(
            name="vllm_long_context_b",
            env={
                "ORATOR_ASR_MAX_UTTERANCE_SEC": "42",
                "ORATOR_ASR_MIN_UTTERANCE_SEC": "0.18",
                "ORATOR_ASR_VAD_THRESHOLD": "0.46",
                "ORATOR_ASR_VAD_MIN_SPEECH_MS": "200",
                "ORATOR_ASR_VAD_MIN_SILENCE_MS": "360",
                "ORATOR_ASR_VAD_SPEECH_PAD_MS": "100",
                "ORATOR_ASR_MAX_NEW_TOKENS": "640",
                "ORATOR_ASR_ROLLBACK_TOKENS": "0",
            },
            note="Aggressive long-chunk policy",
        ),
        Profile(
            name="accuracy_focus",
            env={
                "ORATOR_ASR_MAX_UTTERANCE_SEC": "50",
                "ORATOR_ASR_MIN_UTTERANCE_SEC": "0.16",
                "ORATOR_ASR_VAD_THRESHOLD": "0.44",
                "ORATOR_ASR_VAD_MIN_SPEECH_MS": "180",
                "ORATOR_ASR_VAD_MIN_SILENCE_MS": "420",
                "ORATOR_ASR_VAD_SPEECH_PAD_MS": "120",
                "ORATOR_ASR_MAX_NEW_TOKENS": "768",
                "ORATOR_ASR_ROLLBACK_TOKENS": "0",
            },
            note="Accuracy-first long context + higher decode cap",
        ),
        Profile(
            name="rollback_experimental",
            env={
                "ORATOR_ASR_MAX_UTTERANCE_SEC": "36",
                "ORATOR_ASR_MIN_UTTERANCE_SEC": "0.20",
                "ORATOR_ASR_VAD_THRESHOLD": "0.48",
                "ORATOR_ASR_VAD_MIN_SPEECH_MS": "220",
                "ORATOR_ASR_VAD_MIN_SILENCE_MS": "280",
                "ORATOR_ASR_VAD_SPEECH_PAD_MS": "80",
                "ORATOR_ASR_MAX_NEW_TOKENS": "512",
                "ORATOR_ASR_ROLLBACK_TOKENS": "3",
            },
            note="Experimental prefix rollback path",
        ),
    ]


def pick_best(results: List[Dict], max_cer: float, min_stream_rt: float, min_asr_rt: float) -> Dict:
    qualified = [
        r
        for r in results
        if r.get("ok")
        and r.get("cer") is not None
        and r.get("cer") <= max_cer
        and (r.get("stream_rt") or 0.0) >= min_stream_rt
        and (r.get("asr_rt") or 0.0) >= min_asr_rt
    ]
    if qualified:
        # prioritize lower CER, then higher stream_rt
        qualified.sort(key=lambda x: (x["cer"], -(x.get("stream_rt") or 0.0)))
        return qualified[0]

    fallback = [r for r in results if r.get("ok") and r.get("cer") is not None]
    if fallback:
        fallback.sort(key=lambda x: (x["cer"], -(x.get("stream_rt") or 0.0)))
        return fallback[0]
    return {}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pcm", required=True)
    ap.add_argument("--gold", required=True)
    ap.add_argument("--diar", required=True)
    ap.add_argument("--asr", required=True)
    ap.add_argument("--asr-vad", required=True)
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--duration-sec", type=float, default=120.0)
    ap.add_argument("--rate", type=float, default=0.0)
    ap.add_argument("--frame-ms", type=int, default=100)
    ap.add_argument("--timeline-timeout", type=float, default=600.0)
    ap.add_argument("--max-cer", type=float, default=0.30)
    ap.add_argument("--min-stream-rt", type=float, default=2.7)
    ap.add_argument("--min-asr-rt", type=float, default=3.0)
    ap.add_argument("--out-dir", default="/tmp/orator_ab")
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    src_pcm = Path(args.pcm)
    if not src_pcm.exists():
        print(f"missing pcm: {src_pcm}", file=sys.stderr)
        return 2

    clip_pcm = out_dir / f"clip_{int(args.duration_sec)}s.pcm"
    make_clip(src_pcm, clip_pcm, args.duration_sec)

    ws_client = root / "tools" / "ws_stream_client.py"
    cer_tool = root / "tools" / "cer.py"
    ws_bin = root / "build" / "orator_ws"
    if not ws_bin.exists():
        print(f"missing binary: {ws_bin}", file=sys.stderr)
        return 2

    profiles = default_profiles()
    results: List[Dict] = []

    for idx, p in enumerate(profiles, start=1):
        print(f"\n[{idx}/{len(profiles)}] profile={p.name} :: {p.note}")
        run_json = out_dir / f"{p.name}.json"
        server_log = out_dir / f"{p.name}.server.log"

        env = os.environ.copy()
        env.update(p.env)

        server_cmd = [str(ws_bin), str(args.port), args.diar, args.asr, args.asr_vad]
        with server_log.open("w", encoding="utf-8") as slog:
            proc = subprocess.Popen(
                server_cmd,
                cwd=str(root),
                stdout=slog,
                stderr=subprocess.STDOUT,
                env=env,
                text=True,
            )

        try:
            if not wait_server_ready(server_log, timeout_sec=60.0):
                proc.terminate()
                proc.wait(timeout=5)
                results.append(
                    {
                        "profile": p.name,
                        "ok": False,
                        "error": "server_not_ready",
                    }
                )
                continue

            ws_cmd = [
                sys.executable,
                str(ws_client),
                "--pcm",
                str(clip_pcm),
                "--host",
                "127.0.0.1",
                "--port",
                str(args.port),
                "--rate",
                str(args.rate),
                "--frame-ms",
                str(args.frame_ms),
                "--timeline-timeout",
                str(args.timeline_timeout),
                "--out",
                str(run_json),
            ]
            ws_ret = run_cmd(ws_cmd, timeout=int(args.timeline_timeout + 120))
            if ws_ret.returncode != 0:
                results.append(
                    {
                        "profile": p.name,
                        "ok": False,
                        "error": "ws_client_failed",
                        "stdout": ws_ret.stdout[-800:],
                        "stderr": ws_ret.stderr[-800:],
                    }
                )
                continue

            cer_cmd = [
                sys.executable,
                str(cer_tool),
                "--gold",
                args.gold,
                "--hyp-json",
                str(run_json),
                "--max-sec",
                str(args.duration_sec),
            ]
            cer_ret = run_cmd(cer_cmd, timeout=120)
            cer = parse_cer(cer_ret.stdout)

            meta = read_json(run_json).get("meta", {})
            r = {
                "profile": p.name,
                "ok": cer_ret.returncode == 0 and cer is not None,
                "note": p.note,
                "env": p.env,
                "cer": cer,
                "stream_rt": meta.get("stream_rt_factor"),
                "asr_rt": meta.get("asr_rt_factor"),
                "diar_rt": meta.get("diar_rt_factor"),
                "audio_sec": meta.get("audio_sec"),
                "total_wall_sec": meta.get("total_wall_sec"),
            }
            results.append(r)
            print(
                f"  CER={r['cer']:.4f}  stream_rt={r['stream_rt']}x "
                f"asr_rt={r['asr_rt']}x diar_rt={r['diar_rt']}x"
            )
        finally:
            if proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
                try:
                    proc.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)

    best = pick_best(results, args.max_cer, args.min_stream_rt, args.min_asr_rt)

    report = {
        "targets": {
            "max_cer": args.max_cer,
            "min_stream_rt": args.min_stream_rt,
            "min_asr_rt": args.min_asr_rt,
            "duration_sec": args.duration_sec,
            "rate": "max" if args.rate == 0 else args.rate,
        },
        "results": results,
        "best": best,
    }

    report_json = out_dir / "strategy_report.json"
    report_md = out_dir / "strategy_report.md"
    report_json.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = []
    lines.append("# Orator ASR Strategy AB Report")
    lines.append("")
    lines.append(f"- Target max CER: {args.max_cer:.4f}")
    lines.append(f"- Target min stream RT: {args.min_stream_rt:.3f}x")
    lines.append(f"- Target min ASR RT: {args.min_asr_rt:.3f}x")
    lines.append(f"- Duration: {args.duration_sec:.1f}s")
    lines.append("")
    for r in results:
        if not r.get("ok"):
            lines.append(f"- {r.get('profile')}: FAILED ({r.get('error')})")
            continue
        lines.append(
            f"- {r['profile']}: CER={r['cer']:.4f}, stream_rt={r['stream_rt']}x, "
            f"asr_rt={r['asr_rt']}x, diar_rt={r['diar_rt']}x"
        )
    lines.append("")
    if best:
        lines.append(f"## Recommended Profile: {best['profile']}")
        lines.append(f"- CER={best['cer']:.4f}")
        lines.append(f"- stream_rt={best['stream_rt']}x")
        lines.append(f"- asr_rt={best['asr_rt']}x")
        lines.append("- Env:")
        for k, v in best.get("env", {}).items():
            lines.append(f"  - {k}={v}")
    else:
        lines.append("## Recommended Profile: none")
        lines.append("- No successful run with CER parsed.")

    report_md.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"\nreport: {report_json}")
    print(f"report: {report_md}")
    if best:
        print(
            f"best={best['profile']} CER={best['cer']:.4f} "
            f"stream_rt={best['stream_rt']}x asr_rt={best['asr_rt']}x"
        )
        return 0

    print("no successful profile")
    return 1


if __name__ == "__main__":
    sys.exit(main())

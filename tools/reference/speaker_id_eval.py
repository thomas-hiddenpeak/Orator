#!/usr/bin/env python3
"""Ground-truth speaker-identity evaluation on test.mp3 / test.txt.

The reference transcript (test/data/reference/test.txt) carries timestamped,
NAMED speaker turns, so it is a real ground truth for speaker identity. This
tool answers, deterministically (no rate=0 streaming non-determinism), the two
business questions:

  1. VERIFICATION / re-identification (speaker already enrolled): split each
     speaker's turns into an enrollment half and a test half; enroll a centroid
     voiceprint from the enrollment half; for every test segment report the
     cosine to its own speaker vs the other speakers, the 1:N identification
     accuracy, and the equal-error-rate (EER) cosine threshold.

  2. ENROLLMENT quality: how separable the speakers are when the voiceprint is
     built only from longer, cleaner turns (a proxy for Sortformer's
     high-confidence segments).

Embeddings come from the canonical NeMo TitaNet-Large (the same model the C++
runtime reimplements and is validated against), so the numbers are the model's
true operating points on this audio. Runs in the isolated tools/.venv-nemo.

Usage:
  tools/.venv-nemo/bin/python tools/reference/speaker_id_eval.py \
      [--audio test/data/audio/test.mp3] \
      [--transcript test/data/reference/test.txt] \
      [--min-seg 1.5] [--duration 0]
"""

import argparse
import os
import re
import subprocess

import numpy as np
import torch

HERE = os.path.dirname(__file__)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_NEMO = os.path.join(
    ROOT, "models", "speaker", "speakerverification_en_titanet_large.nemo")
DEFAULT_AUDIO = os.path.join(ROOT, "test", "data", "audio", "test.mp3")
DEFAULT_TXT = os.path.join(ROOT, "test", "data", "reference", "test.txt")

TS = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(\S+)")


def parse_transcript(path):
    """Return [(start_sec, speaker)] turn markers, ordered by time."""
    turns = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = TS.match(line.strip())
            if not m:
                continue
            h, mi, s, spk = m.groups()
            turns.append((int(h) * 3600 + int(mi) * 60 + int(s), spk))
    return turns


def turns_to_segments(turns, audio_dur):
    """Convert turn markers into [(start, end, speaker)] (end = next start)."""
    segs = []
    for i, (t, spk) in enumerate(turns):
        end = turns[i + 1][0] if i + 1 < len(turns) else audio_dur
        if end > t:
            segs.append((t, end, spk))
    return segs


def load_audio(path, sr, start, end):
    cmd = ["ffmpeg", "-v", "quiet", "-i", path, "-ar", str(sr), "-ac", "1",
           "-ss", str(start), "-t", str(end - start), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def audio_duration(path):
    out = subprocess.run(
        ["ffprobe", "-v", "quiet", "-show_entries", "format=duration",
         "-of", "csv=p=0", path], capture_output=True, text=True).stdout
    return float(out.strip())


def embed(model, wav, sr=16000):
    sig = torch.from_numpy(wav).unsqueeze(0)
    sig_len = torch.tensor([wav.shape[0]])
    mel, mlen = model.preprocessor(input_signal=sig, length=sig_len)
    enc, elen = model.encoder(audio_signal=mel, length=mlen)
    _, emb = model.decoder(encoder_output=enc, length=elen)
    e = emb.squeeze(0).cpu().numpy()
    return e / (np.linalg.norm(e) + 1e-12)


def embed_batch(model, wavs, sr=16000):
    """Embed a list of variable-length waveforms (padded batch). NeMo masks the
    padding via the per-sample length, so the result matches one-at-a-time."""
    if not wavs:
        return []
    maxlen = max(w.shape[0] for w in wavs)
    batch = np.zeros((len(wavs), maxlen), dtype=np.float32)
    lens = np.zeros(len(wavs), dtype=np.int64)
    for i, w in enumerate(wavs):
        batch[i, : w.shape[0]] = w
        lens[i] = w.shape[0]
    sig = torch.from_numpy(batch)
    sig_len = torch.from_numpy(lens)
    mel, mlen = model.preprocessor(input_signal=sig, length=sig_len)
    enc, elen = model.encoder(audio_signal=mel, length=mlen)
    _, emb = model.decoder(encoder_output=enc, length=elen)
    e = emb.cpu().numpy()
    return [v / (np.linalg.norm(v) + 1e-12) for v in e]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nemo", default=DEFAULT_NEMO)
    ap.add_argument("--audio", default=DEFAULT_AUDIO)
    ap.add_argument("--transcript", default=DEFAULT_TXT)
    ap.add_argument("--min-seg", type=float, default=1.5)
    ap.add_argument("--duration", type=float, default=0.0,
                    help="limit analysis to the first N seconds (0 = full)")
    args = ap.parse_args()

    from nemo.collections.asr.models import EncDecSpeakerLabelModel
    model = EncDecSpeakerLabelModel.restore_from(args.nemo, map_location="cpu")
    model.eval()
    torch.set_grad_enabled(False)

    dur = audio_duration(args.audio)
    if args.duration > 0:
        dur = min(dur, args.duration)
    turns = parse_transcript(args.transcript)
    segs = [s for s in turns_to_segments(turns, dur)
            if s[1] <= dur and (s[1] - s[0]) >= args.min_seg]

    # Embed every ground-truth segment (batched), grouped by speaker.
    metas, wavs = [], []
    for (s, e, spk) in segs:
        wav = load_audio(args.audio, 16000, s, e)
        if wav.shape[0] < int(0.5 * 16000):
            continue
        # Cap very long turns to keep batch padding bounded (head 20 s is plenty
        # of voice for a centroid).
        if wav.shape[0] > 20 * 16000:
            wav = wav[: 20 * 16000]
        metas.append((s, e, spk))
        wavs.append(wav)

    by_spk = {}
    B = 8
    done = 0
    for i in range(0, len(wavs), B):
        embs = embed_batch(model, wavs[i : i + B])
        for j, ev in enumerate(embs):
            s, e, spk = metas[i + j]
            by_spk.setdefault(spk, []).append((s, e, ev))
        done += len(embs)
        print(f"  embedded {done}/{len(wavs)} segments", end="\r", flush=True)
    print()

    speakers = sorted(by_spk, key=lambda k: -len(by_spk[k]))
    print(f"\naudio={dur:.0f}s  speakers={len(speakers)}  "
          f"segments(>= {args.min_seg}s)={sum(len(v) for v in by_spk.values())}")
    for spk in speakers:
        tot = sum(e - s for (s, e, _) in by_spk[spk])
        print(f"  {spk:8s}  segments={len(by_spk[spk]):3d}  speech={tot:6.1f}s")

    # Enrollment = first half of each speaker's segments (by time); test = rest.
    enroll, test = {}, {}
    for spk, items in by_spk.items():
        items = sorted(items, key=lambda x: x[0])
        half = max(1, len(items) // 2)
        enroll[spk] = np.mean([it[2] for it in items[:half]], axis=0)
        enroll[spk] /= np.linalg.norm(enroll[spk]) + 1e-12
        test[spk] = items[half:]

    spk_list = [s for s in speakers if len(test[s]) > 0]
    print("\n=== 1:N identification (test segment -> argmax enrolled speaker) ===")
    correct = total = 0
    same_scores, diff_scores = [], []
    for spk in spk_list:
        for (_, _, emb_v) in test[spk]:
            scores = {k: float(np.dot(emb_v, enroll[k])) for k in spk_list}
            pred = max(scores, key=scores.get)
            correct += int(pred == spk)
            total += 1
            same_scores.append(scores[spk])
            for k in spk_list:
                if k != spk:
                    diff_scores.append(scores[k])
    same_scores = np.array(same_scores)
    diff_scores = np.array(diff_scores)
    print(f"  accuracy = {correct}/{total} = {100.0 * correct / max(total,1):.1f}%")
    print(f"  same-speaker cosine : mean={same_scores.mean():.3f} "
          f"min={same_scores.min():.3f} p10={np.percentile(same_scores,10):.3f}")
    print(f"  diff-speaker cosine : mean={diff_scores.mean():.3f} "
          f"max={diff_scores.max():.3f} p90={np.percentile(diff_scores,90):.3f}")

    # EER threshold sweep (same=target, diff=impostor).
    best_t, best_eer = 0.0, 1.0
    for t in np.linspace(0.0, 0.9, 91):
        far = float((diff_scores >= t).mean())   # accept impostor
        frr = float((same_scores < t).mean())    # reject genuine
        if abs(far - frr) < abs(best_eer):
            pass
        if max(far, frr) < best_eer or abs(far - frr) <= 0.01:
            if abs(far - frr) <= 0.02:
                best_t, best_eer = t, (far + frr) / 2
    # Robust EER: the threshold minimizing |FAR-FRR|.
    ts = np.linspace(0.0, 0.9, 181)
    fars = np.array([(diff_scores >= t).mean() for t in ts])
    frrs = np.array([(same_scores < t).mean() for t in ts])
    i = int(np.argmin(np.abs(fars - frrs)))
    print(f"  EER threshold ~ {ts[i]:.3f}  (FAR={fars[i]:.2%}, FRR={frrs[i]:.2%})")
    print(f"  threshold for FAR<=1% : "
          f"{ts[np.argmax(fars <= 0.01)]:.3f}")


if __name__ == "__main__":
    main()

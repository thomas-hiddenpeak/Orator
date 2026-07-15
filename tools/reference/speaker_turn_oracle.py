#!/usr/bin/env python3
"""NeMo oracle for Spec 013 per-turn TitaNet registry similarities.

This tools-only program reads the exact PCM16 WAV used by a captured run, the
same candidate-turn TSV consumed by speaker_turn_evidence_probe, and a frozen
SpeakerDatabase file. It embeds only explicitly selected evidence IDs and
writes cosine scores against every enrolled identity. No reference speaker or
correctness label is read.
"""

import argparse
import csv
import struct
import tomllib
import wave

import numpy as np
import torch


REGISTRY_MAGIC = 0x524B5053
REGISTRY_VERSION = 1


def read_pcm16_wav(path):
    with wave.open(path, "rb") as source:
        if source.getnchannels() != 1:
            raise ValueError("oracle audio must be mono")
        if source.getsampwidth() != 2:
            raise ValueError("oracle audio must be PCM16")
        sample_rate = source.getframerate()
        pcm = source.readframes(source.getnframes())
    samples = np.frombuffer(pcm, dtype="<i2").astype(np.float32)
    samples /= 32768.0
    return samples, sample_rate


def read_registry(path):
    with open(path, "rb") as source:
        header = source.read(20)
        if len(header) != 20:
            raise ValueError("truncated speaker registry header")
        magic, version, capacity, dimension, size = struct.unpack(
            "<IIiii", header)
        if magic != REGISTRY_MAGIC or version != REGISTRY_VERSION:
            raise ValueError("unsupported speaker registry")
        if size < 0 or size > capacity or dimension <= 0:
            raise ValueError("invalid speaker registry shape")
        speaker_ids = []
        for _ in range(size):
            raw_length = source.read(4)
            if len(raw_length) != 4:
                raise ValueError("truncated speaker id length")
            length = struct.unpack("<I", raw_length)[0]
            value = source.read(length)
            if len(value) != length:
                raise ValueError("truncated speaker id")
            speaker_ids.append(value.decode("utf-8"))
        raw_embeddings = source.read(size * dimension * 4)
        if len(raw_embeddings) != size * dimension * 4:
            raise ValueError("truncated speaker embeddings")
        if source.read(1):
            raise ValueError("unexpected trailing speaker registry bytes")
    embeddings = np.frombuffer(raw_embeddings, dtype="<f4").copy()
    return speaker_ids, embeddings.reshape(size, dimension)


def read_spans(path, selected_ids):
    spans = []
    with open(path, encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source, delimiter="\t"):
            evidence_id = row["evidence_id"]
            if evidence_id not in selected_ids:
                continue
            spans.append({
                "evidence_id": evidence_id,
                "start_sec": float(row["start_sec"]),
                "end_sec": float(row["end_sec"]),
            })
    found = {span["evidence_id"] for span in spans}
    missing = sorted(selected_ids - found)
    if missing:
        raise ValueError("selected evidence IDs not found: " + ",".join(missing))
    return spans


def embedding_window(span, config):
    start = span["start_sec"]
    end = span["end_sec"]
    edge = float(config["speaker"]["edge_margin_sec"])
    max_window = float(config["speaker"]["max_embed_window_sec"])
    if end - start > 2.0 * edge + 0.5:
        start += edge
        end -= edge
    if end - start > max_window:
        middle = 0.5 * (start + end)
        start = middle - 0.5 * max_window
        end = middle + 0.5 * max_window
    return start, end


def embed(model, samples):
    signal = torch.from_numpy(samples).unsqueeze(0)
    length = torch.tensor([samples.shape[0]])
    mel, mel_length = model.preprocessor(input_signal=signal, length=length)
    encoded, encoded_length = model.encoder(
        audio_signal=mel, length=mel_length)
    _, embedding = model.decoder(
        encoder_output=encoded, length=encoded_length)
    value = embedding.squeeze(0).cpu().numpy()
    return value / (np.linalg.norm(value) + 1e-12)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio", required=True)
    parser.add_argument("--nemo", required=True)
    parser.add_argument("--registry", required=True)
    parser.add_argument("--spans", required=True)
    parser.add_argument("--config", default="orator.toml")
    parser.add_argument("--evidence-ids", required=True,
                        help="comma-separated candidate evidence IDs")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    selected_ids = {
        value.strip() for value in args.evidence_ids.split(",")
        if value.strip()
    }
    if not selected_ids:
        raise ValueError("at least one evidence ID is required")

    with open(args.config, "rb") as source:
        config = tomllib.load(source)
    min_duration = float(config["speaker"]["min_embed_sec"])
    samples, sample_rate = read_pcm16_wav(args.audio)
    if sample_rate != 16000:
        raise ValueError("oracle audio sample rate must be 16000 Hz")
    speaker_ids, registry = read_registry(args.registry)
    spans = read_spans(args.spans, selected_ids)

    from nemo.collections.asr.models import EncDecSpeakerLabelModel
    model = EncDecSpeakerLabelModel.restore_from(args.nemo, map_location="cpu")
    model.eval()
    torch.set_grad_enabled(False)

    fieldnames = [
        "evidence_id", "embed_start_sec", "embed_end_sec", "best_id",
        "best_score", "second_id", "second_score", "margin",
    ] + [f"score_{speaker_id}" for speaker_id in speaker_ids]
    with open(args.out, "w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        for span in spans:
            if span["end_sec"] - span["start_sec"] < min_duration:
                raise ValueError(
                    f'{span["evidence_id"]} is shorter than min_embed_sec')
            start, end = embedding_window(span, config)
            start_sample = round(start * sample_rate)
            end_sample = round(end * sample_rate)
            value = embed(model, samples[start_sample:end_sample].copy())
            scores = registry @ value
            order = np.argsort(scores)[::-1]
            best = int(order[0])
            second = int(order[1])
            row = {
                "evidence_id": span["evidence_id"],
                "embed_start_sec": f"{start:.6f}",
                "embed_end_sec": f"{end:.6f}",
                "best_id": speaker_ids[best],
                "best_score": f"{scores[best]:.6f}",
                "second_id": speaker_ids[second],
                "second_score": f"{scores[second]:.6f}",
                "margin": f"{scores[best] - scores[second]:.6f}",
            }
            for index, speaker_id in enumerate(speaker_ids):
                row[f"score_{speaker_id}"] = f"{scores[index]:.6f}"
            writer.writerow(row)

    print(f"oracle_spans={len(spans)} registry={len(speaker_ids)} out={args.out}")


if __name__ == "__main__":
    main()

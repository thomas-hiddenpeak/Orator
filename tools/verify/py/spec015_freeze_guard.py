#!/usr/bin/env python3
"""Create or verify the Spec 015 frozen non-ASR control manifest.

This tool verifies source, configuration, model, and reference provenance only.
It does not read product output or evaluate speaker or transcript correctness.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tomllib


SCHEMA_VERSION = 1
EXPECTED_OFFICIAL_REVISION = (
    "7c6daf77a2421100f5fb066495372c00129d39ff")
DEFAULT_MANIFEST = Path(
    "specs/015-asr-inference-closing/frozen-control.json")
FROZEN_CONFIG_SECTIONS = (
    "align",
    "timeline",
    "speaker",
    "speaker_fusion",
    "vad",
    "diarizer",
)
FROZEN_SOURCE_PATHS = (
    "include/core/time_base.h",
    "include/model/forced_align_decode.h",
    "include/model/qwen3_aligner_lm.h",
    "include/model/qwen3_forced_aligner.h",
    "include/model/sortformer_decoder.h",
    "include/model/speaker_database.h",
    "include/model/streaming_sortformer.h",
    "include/model/titanet_embedder.h",
    "include/pipeline/align_worker.h",
    "include/pipeline/business_speaker_pipeline.h",
    "include/pipeline/comprehensive_timeline.h",
    "include/pipeline/diar_postprocess.h",
    "include/pipeline/diarization_worker.h",
    "include/pipeline/gpu_vad.h",
    "include/pipeline/speaker_evidence_stage.h",
    "include/pipeline/speaker_identity_stage.h",
    "src/model/forced_align_decode.cc",
    "src/model/qwen3_aligner_lm.cu",
    "src/model/qwen3_forced_aligner.cc",
    "src/model/sortformer_decoder.cu",
    "src/model/speaker_database.cc",
    "src/model/streaming_sortformer.cc",
    "src/model/titanet_embedder.cu",
    "src/pipeline/align_worker.cc",
    "src/pipeline/business_speaker_pipeline.cc",
    "src/pipeline/business_speaker_utils.h",
    "src/pipeline/comprehensive_timeline.cc",
    "src/pipeline/diar_postprocess.cc",
    "src/pipeline/diarization_worker.cc",
    "src/pipeline/gpu_vad.cu",
    "src/pipeline/speaker_evidence_stage.cc",
    "src/pipeline/speaker_fusion_policy.cc",
    "src/pipeline/speaker_fusion_policy.h",
    "src/pipeline/speaker_identity_stage.cc",
)
FROZEN_MODEL_PATHS = (
    "models/asr/Qwen/Qwen3-ASR-1.7B",
    "models/ForcedAligner",
    "models/sortformer_4spk_v2.1.safetensors",
    "models/diar_streaming_sortformer_4spk-v2.1.nemo",
    "models/speaker",
    "models/vad/silero_vad.safetensors",
)


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_path(path):
    path = Path(path)
    if not path.exists():
        return {"exists": False}
    if path.is_file():
        return {
            "exists": True,
            "kind": "file",
            "size": path.stat().st_size,
            "sha256": sha256_file(path),
        }

    files = []
    total_size = 0
    for child in sorted(item for item in path.rglob("*") if item.is_file()):
        relative = child.relative_to(path).as_posix()
        size = child.stat().st_size
        total_size += size
        files.append({
            "path": relative,
            "size": size,
            "sha256": sha256_file(child),
        })
    encoded = json.dumps(
        files, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return {
        "exists": True,
        "kind": "directory",
        "size": total_size,
        "file_count": len(files),
        "sha256": hashlib.sha256(encoded).hexdigest(),
        "files": files,
    }


def load_config_sections(config_path, section_names):
    with Path(config_path).open("rb") as source:
        config = tomllib.load(source)
    missing = [name for name in section_names if name not in config]
    if missing:
        raise RuntimeError(
            "missing frozen TOML sections: " + ", ".join(missing))
    return {name: config[name] for name in section_names}


def build_manifest(repo, runtime_baseline_commit,
                   source_paths=FROZEN_SOURCE_PATHS,
                   model_paths=FROZEN_MODEL_PATHS,
                   config_sections=FROZEN_CONFIG_SECTIONS,
                   config_path="orator.toml"):
    repo = Path(repo).resolve()
    source = {
        path: hash_path(repo / path)
        for path in source_paths
    }
    models = {
        path: hash_path(repo / path)
        for path in model_paths
    }
    missing = [
        path for path, record in {**source, **models}.items()
        if not record.get("exists")
    ]
    if missing:
        raise FileNotFoundError("missing frozen paths: " + ", ".join(missing))

    return {
        "schema_version": SCHEMA_VERSION,
        "purpose": "Spec 015 mechanical freeze; no product evaluation",
        "runtime_baseline_commit": runtime_baseline_commit,
        "official_reference_revision": EXPECTED_OFFICIAL_REVISION,
        "config": {
            "path": config_path,
            "sections": load_config_sections(
                repo / config_path, config_sections),
        },
        "source": source,
        "models": models,
    }


def run_git(reference_root, *args):
    result = subprocess.run(
        ["git", "-C", str(reference_root), *args],
        check=False, capture_output=True, text=True)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(detail)
    return result.stdout.strip()


def verify_reference(reference_root, expected_revision):
    errors = []
    reference_root = Path(reference_root)
    if not reference_root.is_dir():
        return [f"official reference is missing: {reference_root}"]
    try:
        revision = run_git(reference_root, "rev-parse", "HEAD")
        status = run_git(reference_root, "status", "--short")
    except RuntimeError as error:
        return [f"cannot inspect official reference: {error}"]
    if revision != expected_revision:
        errors.append(
            f"official revision mismatch: {revision} != {expected_revision}")
    if status:
        errors.append("official reference worktree is dirty: " + status)
    return errors


def compare_record(label, expected, actual):
    errors = []
    for field in ("exists", "kind", "size", "file_count", "sha256"):
        if expected.get(field) != actual.get(field):
            errors.append(
                f"{label} {field} changed: "
                f"{actual.get(field)!r} != {expected.get(field)!r}")
    return errors


def verify_manifest(manifest, repo, include_models=False,
                    reference_root=None):
    errors = []
    repo = Path(repo).resolve()
    if manifest.get("schema_version") != SCHEMA_VERSION:
        errors.append("unsupported freeze manifest schema")

    config = manifest.get("config", {})
    try:
        actual_sections = load_config_sections(
            repo / config.get("path", "orator.toml"),
            tuple(config.get("sections", {}).keys()))
    except (OSError, RuntimeError) as error:
        errors.append(f"cannot read frozen config: {error}")
    else:
        if actual_sections != config.get("sections"):
            errors.append("one or more frozen TOML sections changed")

    for path, expected in manifest.get("source", {}).items():
        errors.extend(compare_record(
            f"source {path}", expected, hash_path(repo / path)))

    if include_models:
        for path, expected in manifest.get("models", {}).items():
            errors.extend(compare_record(
                f"model {path}", expected, hash_path(repo / path)))

    if reference_root is not None:
        errors.extend(verify_reference(
            reference_root,
            manifest.get("official_reference_revision", "")))
    return errors


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--create", action="store_true")
    parser.add_argument("--runtime-baseline-commit")
    parser.add_argument("--include-models", action="store_true")
    parser.add_argument("--reference-root")
    return parser.parse_args()


def main():
    args = parse_args()
    repo = Path(args.repo).resolve()
    manifest_path = Path(args.manifest)
    if not manifest_path.is_absolute():
        manifest_path = repo / manifest_path

    if args.create:
        if not args.runtime_baseline_commit:
            raise RuntimeError(
                "--runtime-baseline-commit is required with --create")
        manifest = build_manifest(repo, args.runtime_baseline_commit)
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")
        print(manifest_path)
        return

    with manifest_path.open(encoding="utf-8") as source:
        manifest = json.load(source)
    errors = verify_manifest(
        manifest, repo, include_models=args.include_models,
        reference_root=args.reference_root)
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
    print("Spec 015 frozen control verified (mechanical only)")


if __name__ == "__main__":
    main()

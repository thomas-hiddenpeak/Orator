#!/usr/bin/env python3
"""Create a reproducibility manifest for an Orator validation fixture."""

import argparse
import datetime
import hashlib
import json
import os
import platform
import subprocess
import sys
import tomllib


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_path(path):
    absolute = os.path.abspath(path)
    if not os.path.exists(absolute):
        return {"path": absolute, "exists": False}
    if os.path.isfile(absolute):
        return {
            "path": absolute,
            "exists": True,
            "kind": "file",
            "size": os.path.getsize(absolute),
            "sha256": sha256_file(absolute),
        }

    files = []
    total_size = 0
    for root, dirs, names in os.walk(absolute):
        dirs.sort()
        names.sort()
        for name in names:
            full_path = os.path.join(root, name)
            if not os.path.isfile(full_path):
                continue
            relative = os.path.relpath(full_path, absolute)
            size = os.path.getsize(full_path)
            total_size += size
            files.append({
                "path": relative,
                "size": size,
                "sha256": sha256_file(full_path),
            })
    canonical = json.dumps(
        files, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")
    return {
        "path": absolute,
        "exists": True,
        "kind": "directory",
        "size": total_size,
        "file_count": len(files),
        "sha256": hashlib.sha256(canonical).hexdigest(),
        "files": files,
    }


def command_probe(command, cwd):
    try:
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True,
            check=False, timeout=15)
    except OSError as error:
        return {"returncode": None, "output": str(error)}
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode("utf-8", errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode("utf-8", errors="replace")
        return {"returncode": 124, "output": (stdout + stderr).strip()}
    return {
        "returncode": result.returncode,
        "output": (result.stdout + result.stderr).strip(),
    }


def command_output(command, cwd):
    probe = command_probe(command, cwd)
    if probe["returncode"] != 0:
        return None
    output = probe["output"]
    return output or None


def first_line(command, cwd):
    output = command_output(command, cwd)
    return output.splitlines()[0] if output else None


def read_optional(path):
    try:
        with open(path, encoding="utf-8") as source:
            return source.read().strip()
    except OSError:
        return None


def resolve_path(repo, value):
    if not value:
        return None
    return value if os.path.isabs(value) else os.path.join(repo, value)


def model_paths(config, repo):
    candidates = {
        "diarizer": config.get("diarizer", {}).get("model_weights"),
        "asr": config.get("asr", {}).get("model_dir"),
        "vad": config.get("vad", {}).get("model"),
        "align": config.get("align", {}).get("model_dir"),
        "speaker": config.get("speaker", {}).get("model_dir"),
    }
    return {
        name: resolve_path(repo, path)
        for name, path in candidates.items() if path
    }


def build_manifest(args):
    repo = os.path.abspath(args.repo)
    config_path = resolve_path(repo, args.config)
    with open(config_path, "rb") as source:
        config = tomllib.load(source)

    commit = command_output(["git", "rev-parse", "HEAD"], repo)
    status_text = command_output(
        ["git", "status", "--porcelain", "--untracked-files=normal"], repo)
    status = status_text.splitlines() if status_text else []
    generated = datetime.datetime.now(datetime.timezone.utc)

    assets = {
        "config": hash_path(config_path),
        "audio": hash_path(resolve_path(repo, args.audio)),
        "reference": hash_path(resolve_path(repo, args.reference)),
    }
    if args.executable:
        assets["executable"] = hash_path(resolve_path(repo, args.executable))
    registry_path = args.registry or config.get("speaker", {}).get(
        "registry_path")
    if registry_path:
        assets["speaker_registry"] = hash_path(
            resolve_path(repo, registry_path))
    missing_assets = [
        name for name, asset in assets.items()
        if name != "speaker_registry" and not asset["exists"]
    ]
    if missing_assets:
        raise FileNotFoundError(
            "required fixture assets are missing: " + ", ".join(missing_assets))

    models = {}
    if not args.skip_model_hashes:
        for name, path in model_paths(config, repo).items():
            print(f"hashing {name}: {path}", file=sys.stderr)
            models[name] = hash_path(path)
        missing_models = [
            name for name, asset in models.items() if not asset["exists"]]
        if missing_models:
            raise FileNotFoundError(
                "configured models are missing: " + ", ".join(missing_models))

    short_commit = (commit or "unknown")[:12]
    manifest = {
        "schema_version": 1,
        "manifest_id": (
            f"orator-fixture-{generated.strftime('%Y%m%dT%H%M%SZ')}-"
            f"{short_commit}"),
        "generated_utc": generated.isoformat(),
        "repo": repo,
        "git": {
            "commit": commit,
            "dirty": bool(status),
            "status": status,
        },
        "assets": assets,
        "models": models,
        "environment_overrides": {
            key: value for key, value in sorted(os.environ.items())
            if key.startswith("ORATOR_")
        },
        "device": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "jetson_release": read_optional("/etc/nv_tegra_release"),
            "nvpmodel": command_probe(["nvpmodel", "-q"], repo),
            "jetson_clocks": command_probe(
                ["jetson_clocks", "--show"], repo),
            "tegrastats_probe": command_probe(
                ["timeout", "2s", "tegrastats", "--interval", "1000"], repo),
        },
        "tools": {
            "python": platform.python_version(),
            "git": first_line(["git", "--version"], repo),
            "cmake": first_line(["cmake", "--version"], repo),
            "compiler": first_line(["c++", "--version"], repo),
            "nvcc": first_line(["nvcc", "--version"], repo),
            "ffmpeg": first_line(["ffmpeg", "-version"], repo),
        },
    }
    canonical = json.dumps(
        manifest, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")
    manifest["content_sha256"] = hashlib.sha256(canonical).hexdigest()
    return manifest


def main():
    parser = argparse.ArgumentParser(
        description="Create a closing-grade Orator fixture manifest")
    parser.add_argument("--repo", default=".")
    parser.add_argument("--config", default="orator.toml")
    parser.add_argument("--audio", default="test/data/audio/test.mp3")
    parser.add_argument("--reference", default="test/data/reference/test.txt")
    parser.add_argument("--executable", default="build/orator_ws")
    parser.add_argument("--registry")
    parser.add_argument("--skip-model-hashes", action="store_true",
                        help="Only for tool smoke tests, never acceptance")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    manifest = build_manifest(args)
    output_path = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as output:
        json.dump(manifest, output, ensure_ascii=False, indent=2)
    print(output_path)


if __name__ == "__main__":
    main()

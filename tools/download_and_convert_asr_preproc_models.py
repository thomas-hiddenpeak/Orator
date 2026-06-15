#!/usr/bin/env python3
"""Download FRCRN / TF-GridNet checkpoints and convert to safetensors.

Default model sources:
  - FRCRN:   alibabasglab/FRCRN_SE_16K
  - TFGrid:  espnet/ms_snsd_tfgridnet

Usage:
  ./tools/.venv/bin/python tools/download_and_convert_asr_preproc_models.py

Outputs:
  models/asr_preproc/frcrn.safetensors
  models/asr_preproc/tfgridnet.safetensors
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
from pathlib import Path
from typing import Dict, Optional, Tuple

import torch
from huggingface_hub import snapshot_download
from safetensors.torch import save_file


def _pick_checkpoint(repo_dir: Path) -> Optional[Path]:
    exts = {".pt", ".pth", ".bin", ".ckpt", ".safetensors"}
    files = [p for p in repo_dir.rglob("*") if p.is_file() and p.suffix.lower() in exts]
    if not files:
        return None

    def score(p: Path) -> Tuple[int, int, int, str]:
        name = p.name.lower()
        stem = p.stem.lower()
        s0 = 0
        if p.suffix.lower() == ".safetensors":
            s0 += 50
        if "best" in name:
            s0 += 20
        if "pytorch_model" in name:
            s0 += 20
        if "model" in stem:
            s0 += 10
        if "checkpoint" in name or "ckpt" in name:
            s0 += 8
        if "avg" in name:
            s0 += 6
        # Prefer shorter paths when score ties.
        return (s0, -len(p.parts), -len(name), name)

    files.sort(key=score, reverse=True)
    return files[0]


def _extract_state_dict(obj) -> Dict[str, torch.Tensor]:
    if isinstance(obj, dict):
        for k in ["state_dict", "model", "generator", "net", "model_state_dict", "weights"]:
            if k in obj and isinstance(obj[k], dict):
                obj = obj[k]
                break

    if isinstance(obj, torch.nn.Module):
        obj = obj.state_dict()

    if not isinstance(obj, dict):
        raise RuntimeError("Unsupported checkpoint format: cannot find state_dict")

    state: Dict[str, torch.Tensor] = {}
    for k, v in obj.items():
        if not isinstance(v, torch.Tensor):
            continue
        nk = k[7:] if k.startswith("module.") else k
        # Break potential shared-storage aliases so safetensors can serialize.
        state[nk] = v.detach().cpu().contiguous().clone()

    if not state:
        raise RuntimeError("No tensor weights found in checkpoint")
    return state


def _convert_checkpoint_to_safetensors(src_ckpt: Path, dst_st: Path) -> None:
    if src_ckpt.suffix.lower() == ".safetensors":
        dst_st.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src_ckpt, dst_st)
        return

    obj = torch.load(src_ckpt, map_location="cpu")
    state = _extract_state_dict(obj)
    dst_st.parent.mkdir(parents=True, exist_ok=True)
    save_file(state, str(dst_st), metadata={"converted_from": str(src_ckpt)})


def _download_and_convert(repo_id: str, cache_dir: Path, out_path: Path) -> Path:
    local = Path(
        snapshot_download(
            repo_id=repo_id,
            cache_dir=str(cache_dir),
            resume_download=True,
            allow_patterns=[
                "*.pt",
                "*.pth",
                "*.bin",
                "*.ckpt",
                "*.safetensors",
                "*.json",
                "*.yaml",
                "*.yml",
                "README*",
            ],
        )
    )
    ckpt = _pick_checkpoint(local)
    if ckpt is None:
        raise RuntimeError(f"No checkpoint file found in repo {repo_id}")
    _convert_checkpoint_to_safetensors(ckpt, out_path)
    return ckpt


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--frcrn-repo", default="alibabasglab/FRCRN_SE_16K")
    ap.add_argument("--tfgridnet-repo", default="espnet/ms_snsd_tfgridnet")
    ap.add_argument("--cache-dir", default="/tmp/orator_hf_cache")
    ap.add_argument("--out-dir", default="models/asr_preproc")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    cache_dir = Path(args.cache_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    frcrn_out = out_dir / "frcrn.safetensors"
    tfgrid_out = out_dir / "tfgridnet.safetensors"

    frcrn_src = _download_and_convert(args.frcrn_repo, cache_dir, frcrn_out)
    tfgrid_src = _download_and_convert(args.tfgridnet_repo, cache_dir, tfgrid_out)

    manifest = {
        "frcrn": {"repo": args.frcrn_repo, "source_checkpoint": str(frcrn_src), "output": str(frcrn_out)},
        "tfgridnet": {"repo": args.tfgridnet_repo, "source_checkpoint": str(tfgrid_src), "output": str(tfgrid_out)},
    }
    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"FRCRN      -> {frcrn_out}")
    print(f"TF-GridNet -> {tfgrid_out}")
    print(f"Manifest   -> {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

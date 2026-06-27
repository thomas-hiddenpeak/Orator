#!/usr/bin/env python3
"""Convert a NeMo .nemo (or extracted model_weights.ckpt) to .safetensors.

Pure stdlib: no torch / numpy required. The .ckpt is a torch.save zip whose
`data/<key>` entries are raw little-endian contiguous tensor storages. We parse
the pickle only to recover (name -> dtype, shape, storage-key), then stream the
raw bytes straight into a .safetensors file.

Skips integer bookkeeping tensors (batchnorm num_batches_tracked) by default,
as they are irrelevant to inference.
"""
import argparse
import collections
import io
import json
import os
import pickle
import struct
import sys
import tarfile
import tempfile
import zipfile

# safetensors dtype strings keyed by torch storage class name.
STORAGE_DTYPE = {
    "FloatStorage": ("F32", 4),
    "DoubleStorage": ("F64", 8),
    "HalfStorage": ("F16", 2),
    "BFloat16Storage": ("BF16", 2),
    "LongStorage": ("I64", 8),
    "IntStorage": ("I32", 4),
}


def parse_state_dict(ckpt_zip, prefix):
    """Return list of (name, storage_key, dtype_str, elem_size, shape)."""
    storages = {}

    def persistent_load(pid):
        _, storage_type, key, location, numel = pid
        storages[key] = getattr(storage_type, "__name__", str(storage_type))
        return ("STORAGE", key)

    def rebuild_tensor_v2(storage, offset, size, stride, *rest):
        return ("TENSOR", storage, offset, tuple(size), tuple(stride))

    def rebuild_parameter(data, requires_grad=False, *rest):
        return data

    class Unp(pickle.Unpickler):
        def persistent_load(self, pid):
            return persistent_load(pid)

        def find_class(self, mod, name):
            if name.endswith("Storage"):
                return type(name, (), {"__name__": name})
            if name == "OrderedDict":
                return collections.OrderedDict
            if name in ("_rebuild_tensor_v2", "_rebuild_tensor"):
                return rebuild_tensor_v2
            if name == "_rebuild_parameter":
                return rebuild_parameter
            try:
                return super().find_class(mod, name)
            except Exception:
                return lambda *a, **k: ("OBJ", mod + "." + name, a)

    obj = Unp(io.BytesIO(ckpt_zip.read(prefix + "/data.pkl"))).load()
    out = []
    for name, v in obj.items():
        if not (isinstance(v, tuple) and v and v[0] == "TENSOR"):
            continue
        _, stg, offset, size, stride = v
        key = stg[1]
        storage_cls = storages[key]
        dtype_str, elem = STORAGE_DTYPE[storage_cls]
        out.append((name, key, dtype_str, elem, list(size), offset, stride))
    return out


def _detect_prefix(zf):
    """Locate the directory prefix containing data.pkl (e.g. 'model_weights'
    in older NeMo checkpoints, 'archive' in newer torch.save zips)."""
    for n in zf.namelist():
        if n.endswith("/data.pkl"):
            return n[: -len("/data.pkl")]
    raise SystemExit("data.pkl not found inside checkpoint zip")


def open_ckpt(path):
    """Accept a .nemo (tar) or a raw model_weights.ckpt (zip). Returns
    (ZipFile, prefix)."""
    if zipfile.is_zipfile(path):
        zf = zipfile.ZipFile(path)
        return zf, _detect_prefix(zf)
    # assume tar (.nemo)
    tf = tarfile.open(path)
    member = None
    for m in tf.getmembers():
        if m.name.endswith("model_weights.ckpt"):
            member = m
            break
    if member is None:
        raise SystemExit("model_weights.ckpt not found inside archive")
    data = tf.extractfile(member).read()
    zf = zipfile.ZipFile(io.BytesIO(data))
    return zf, _detect_prefix(zf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help=".nemo or model_weights.ckpt")
    ap.add_argument("output", help="output .safetensors")
    ap.add_argument("--keep-int", action="store_true",
                    help="keep integer bookkeeping tensors (num_batches_tracked)")
    args = ap.parse_args()

    zf, prefix = open_ckpt(args.input)
    if (prefix + "/byteorder") in zf.namelist():
        byteorder = zf.read(prefix + "/byteorder")
        if byteorder != b"little":
            raise SystemExit(
                f"unexpected byteorder {byteorder!r}; need little-endian")
    # else: torch.save zips without a byteorder entry are little-endian.

    tensors = parse_state_dict(zf, prefix)

    # Build header + plan data layout.
    header = {}
    plan = []  # (storage_key, nbytes)
    offset = 0
    skipped = 0
    for name, key, dtype_str, elem, shape, t_off, stride in tensors:
        if dtype_str in ("I64", "I32") and not args.keep_int:
            skipped += 1
            continue
        numel = 1
        for s in shape:
            numel *= s
        nbytes = numel * elem
        raw = zf.read(prefix + "/data/" + str(key))
        if len(raw) != nbytes:
            raise SystemExit(
                f"size mismatch {name}: storage {len(raw)} != {nbytes}")
        # contiguous C-order sanity
        cs, acc = [], 1
        for s in reversed(shape):
            cs.insert(0, acc); acc *= s
        if t_off != 0 or tuple(cs) != tuple(stride):
            raise SystemExit(f"non-contiguous tensor {name}; needs torch path")
        header[name] = {
            "dtype": dtype_str,
            "shape": shape,
            "data_offsets": [offset, offset + nbytes],
        }
        plan.append(raw)
        offset += nbytes

    header["__metadata__"] = {
        "producer": "orator/convert_nemo_to_safetensors.py",
        "source": os.path.basename(args.input),
    }
    header_bytes = json.dumps(header, separators=(",", ":")).encode("utf-8")
    pad = (-(len(header_bytes)) ) % 8
    header_bytes += b" " * pad

    with open(args.output, "wb") as f:
        f.write(struct.pack("<Q", len(header_bytes)))
        f.write(header_bytes)
        for raw in plan:
            f.write(raw)

    total = sum(len(r) for r in plan)
    print(f"wrote {args.output}")
    print(f"  tensors: {len(plan)} (skipped {skipped} int bookkeeping)")
    print(f"  header bytes: {len(header_bytes)}")
    print(f"  data bytes: {total} (~{total/1e6:.1f} MB)")
    print(f"  file bytes: {8 + len(header_bytes) + total}")


if __name__ == "__main__":
    main()

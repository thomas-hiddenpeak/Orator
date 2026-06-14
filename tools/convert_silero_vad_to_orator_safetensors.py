#!/usr/bin/env python3
"""Convert official Silero VAD 16k safetensors into Orator's Silero key schema.

Source model can be extracted from silero-vad wheel at:
  silero_vad/data/silero_vad_16k.safetensors

Usage:
  python3 tools/convert_silero_vad_to_orator_safetensors.py \
    --src /tmp/silero_pkg/unpack/silero_vad/data/silero_vad_16k.safetensors \
    --dst /home/rm01/Orator/models/asr/silero_vad.safetensors
"""

import argparse
import json
import os
import struct


RENAME = {
    "stft_conv.weight": "stft.forward_basis_buffer",
    "conv1.weight": "encoder.0.reparam_conv.weight",
    "conv1.bias": "encoder.0.reparam_conv.bias",
    "conv2.weight": "encoder.1.reparam_conv.weight",
    "conv2.bias": "encoder.1.reparam_conv.bias",
    "conv3.weight": "encoder.2.reparam_conv.weight",
    "conv3.bias": "encoder.2.reparam_conv.bias",
    "conv4.weight": "encoder.3.reparam_conv.weight",
    "conv4.bias": "encoder.3.reparam_conv.bias",
    "lstm_cell.weight_ih": "decoder.rnn.weight_ih",
    "lstm_cell.weight_hh": "decoder.rnn.weight_hh",
    "lstm_cell.bias_ih": "decoder.rnn.bias_ih",
    "lstm_cell.bias_hh": "decoder.rnn.bias_hh",
    "final_conv.weight": "decoder.decoder.2.weight",
    "final_conv.bias": "decoder.decoder.2.bias",
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="Source silero_vad_16k.safetensors")
    ap.add_argument("--dst", required=True, help="Destination Orator-compatible safetensors")
    args = ap.parse_args()

    with open(args.src, "rb") as f:
        header_len = struct.unpack("<Q", f.read(8))[0]
        header = json.loads(f.read(header_len).decode("utf-8"))
        data_blob = f.read()

    new_header = {}
    offset = 0
    for key, meta in header.items():
        if key.startswith("__"):
            continue
        new_key = RENAME.get(key)
        if new_key is None:
            continue
        b0, b1 = meta["data_offsets"]
        size = b1 - b0
        new_header[new_key] = {
            "dtype": meta["dtype"],
            "shape": meta["shape"],
            "data_offsets": [offset, offset + size],
        }
        offset += size

    expected = set(RENAME.values())
    got = set(new_header.keys())
    missing = sorted(expected - got)
    if missing:
        raise RuntimeError(f"Missing tensors after conversion: {missing}")

    out_header = json.dumps(new_header, separators=(",", ":")).encode("utf-8")
    while (8 + len(out_header)) % 8 != 0:
        out_header += b" "

    os.makedirs(os.path.dirname(args.dst), exist_ok=True)
    with open(args.dst, "wb") as g:
        g.write(struct.pack("<Q", len(out_header)))
        g.write(out_header)
        for key, meta in header.items():
            if key.startswith("__"):
                continue
            if key not in RENAME:
                continue
            b0, b1 = meta["data_offsets"]
            g.write(data_blob[b0:b1])

    print(f"Wrote {args.dst} ({os.path.getsize(args.dst)} bytes)")
    print(f"Tensor count: {len(new_header)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

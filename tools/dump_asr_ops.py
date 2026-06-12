#!/usr/bin/env python3
"""Dump PyTorch GPU reference activations for the ASR core operators.

Run inside the torch venv:  source tools/torchenv.sh && python tools/dump_asr_ops.py

PyTorch on the GPU is the authoritative oracle for these kernels (it is the
framework Qwen3-ASR actually runs in). We generate deterministic inputs, compute
each operator with torch on CUDA, and save both the inputs and the reference
outputs as raw little-endian float32 into models/reference/asr_ops/. The C++
test (test/test_asr_ops.cc) loads these and compares the CUDA kernels against
the torch result -- not against a CPU sequential sum.

Tensor layout on disk is row-major and matches the C++ kernels exactly:
  RMSNorm  x,out: [rows, dim]          w: [dim]
  RoPE     x,out: [T, H, Dh]           pos: [T] int32
  SwiGLU   gate,up,out: [n]
  GQA      q: [T,Hq,Dh]  k,v: [T,Hkv,Dh]  out: [T,Hq,Dh]
"""
import os
import numpy as np
import torch
import torch.nn.functional as F

OUT = os.path.join(os.path.dirname(__file__), "..", "models", "reference", "asr_ops")
DEV = "cuda"
DT = torch.float32


def save(name, arr):
    arr = np.ascontiguousarray(arr.astype("<f4"))
    path = os.path.join(OUT, name)
    arr.tofile(path)
    print(f"  saved {name:28s} {list(arr.shape)}")


def rng(seed):
    g = np.random.default_rng(seed)
    return lambda *s: (g.random(s, dtype=np.float64).astype(np.float32) * 2.0 - 1.0)


def dump_rmsnorm():
    rows, dim, eps = 8, 2048, 1e-6
    r = rng(1)
    x = r(rows, dim)
    w = (0.5 + 0.5 * r(dim)).astype(np.float32)
    save("rmsnorm_x.f32", x)
    save("rmsnorm_w.f32", w)
    xt = torch.from_numpy(x).to(DEV, DT)
    wt = torch.from_numpy(w).to(DEV, DT)
    var = xt.pow(2).mean(-1, keepdim=True)
    y = (xt * torch.rsqrt(var + eps)) * wt
    save("rmsnorm_out.f32", y.cpu().numpy())


def dump_rope():
    T, H, Dh, base = 12, 16, 128, 1_000_000.0
    r = rng(2)
    x = r(T, H, Dh)
    pos = np.arange(T, dtype=np.int32)
    save("rope_x.f32", x)
    pos.astype("<i4").tofile(os.path.join(OUT, "rope_pos.i32"))
    print(f"  saved rope_pos.i32               {list(pos.shape)}")
    xt = torch.from_numpy(x).to(DEV, DT)
    half = Dh // 2
    i = torch.arange(half, device=DEV, dtype=DT)
    inv_freq = base ** (-2.0 * i / Dh)                       # [half]
    angle = torch.from_numpy(pos).to(DEV, DT)[:, None] * inv_freq[None, :]  # [T,half]
    cos = angle.cos()[:, None, :]                            # [T,1,half]
    sin = angle.sin()[:, None, :]
    x1 = xt[..., 0::2]
    x2 = xt[..., 1::2]
    out = torch.empty_like(xt)
    out[..., 0::2] = x1 * cos - x2 * sin
    out[..., 1::2] = x1 * sin + x2 * cos
    save("rope_out.f32", out.cpu().numpy())


def dump_swiglu():
    n = 8 * 6144
    r = rng(3)
    g = (3.0 * r(n)).astype(np.float32)
    u = r(n)
    save("swiglu_gate.f32", g)
    save("swiglu_up.f32", u)
    gt = torch.from_numpy(g).to(DEV, DT)
    ut = torch.from_numpy(u).to(DEV, DT)
    o = F.silu(gt) * ut
    save("swiglu_out.f32", o.cpu().numpy())


def dump_gqa():
    T, Hq, Hkv, Dh = 24, 16, 8, 128
    scale = 1.0 / np.sqrt(Dh)
    r = rng(4)
    q = (0.3 * r(T, Hq, Dh)).astype(np.float32)
    k = (0.3 * r(T, Hkv, Dh)).astype(np.float32)
    v = r(T, Hkv, Dh)
    save("gqa_q.f32", q)
    save("gqa_k.f32", k)
    save("gqa_v.f32", v)
    # torch SDPA expects [B,H,T,Dh]; expand KV heads for GQA, causal mask.
    qt = torch.from_numpy(q).to(DEV, DT).permute(1, 0, 2).unsqueeze(0)   # [1,Hq,T,Dh]
    kt = torch.from_numpy(k).to(DEV, DT).permute(1, 0, 2).unsqueeze(0)   # [1,Hkv,T,Dh]
    vt = torch.from_numpy(v).to(DEV, DT).permute(1, 0, 2).unsqueeze(0)
    rep = Hq // Hkv
    kt = kt.repeat_interleave(rep, dim=1)
    vt = vt.repeat_interleave(rep, dim=1)
    out = F.scaled_dot_product_attention(qt, kt, vt, is_causal=True, scale=float(scale))
    out = out.squeeze(0).permute(1, 0, 2).contiguous()  # [T,Hq,Dh]
    save("gqa_out.f32", out.cpu().numpy())


def main():
    os.makedirs(OUT, exist_ok=True)
    print(f"torch {torch.__version__} cuda={torch.cuda.is_available()} "
          f"dev={torch.cuda.get_device_name(0)}")
    print("dumping ASR op references ->", os.path.normpath(OUT))
    dump_rmsnorm()
    dump_rope()
    dump_swiglu()
    dump_gqa()
    print("done.")


if __name__ == "__main__":
    main()

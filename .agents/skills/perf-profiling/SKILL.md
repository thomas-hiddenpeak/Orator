# Perf Profiling — Orator GPU/CPU Performance Analysis

**Domain:** GPU kernel, pipeline latency, RTF (real-time factor), Jetson (Ubuntu) profiling
**Triggers:** performance regression, RTF degradation, GPU profiling request, latency optimization, "why is X slow"

---

## When to Use

- Before and after any GPU kernel change (measure baseline → compare)
- When reported RTF deviates from baseline in `specs/PROJECT_STATE.md` §4
- When asked: "why is pipeline X slow", "profile the GPU", "measure latency"
- Before any throughput or latency optimization is merged

---

## Methodology

### Step 1 — Establish baseline conditions

Record and report:
| Condition | Value |
|-----------|-------|
| GPU clock | Fixed (e.g. 1.3 GHz via `nvidia-smi -ac` or `nvpmodel`) |
| Power mode | MaxN / MODE_0 |
| Audio duration | e.g. 120 s of `test.mp3` |
| Pipeline config | Diar only / ASR only / Both |
| Measurement path | Real WebSocket streaming (Art. IV) — never whole-file shortcut |

### Step 2 — Instrumentation

Use these tools **in order**:

1. **`nsys` (Nsight Systems)** — timeline view, kernel launch overlap, CPU/GPU concurrency
   ```bash
   nsys profile -o profile_$(date +%s) -t cuda,nvtx,osrt ./build/orator_ws 8765 "" <model_dir>
   ```
2. **`ncu` (Nsight Compute)** — single-kernel deep-dive (roofline, occupancy, memory)
   ```bash
   ncu --target-processes all --set full -o kernel_report ./build/orator_ws ...
   ```
3. **Built-in telemetry** — `ORATOR_GPU_TELEMETRY_SEC=1` for streaming GPU metrics
4. **Log-based timing** — `ORATOR_ASR_PROFILE=1` for ASR per-frame timing

### Step 3 — Measure pipeline-level RTF

Through real WebSocket (`tools/ws_stream_client.py`):
```
RTF = wall_time / audio_duration
```
Report per-pipeline (diar, asr, combined), GPU-busy fraction, and wall.

### Step 4 — Identify bottleneck

| Symptom | Likely cause | Action |
|---------|-------------|--------|
| GPU-busy > 90% | Compute-bound | Kernel optimization, fusion, precision reduction |
| GPU-busy < 50% | Latency-bound or serialization | Reduce lock contention, overlap CPU/GPU |
| ASR >> diar time | ASR dominates | Spec 001 NG1 deferred — do not refactor without spec |
| Wall ≈ diar + asr | GPU lock serialization | See Spec 002 (fine-grained locking or scheduling) |

### Step 5 — Validate and commit

1. Record measured RTF before change
2. Apply change
3. Re-measure under **identical conditions**
4. If degradation > 5%: reject unless accuracy gain is documented and approved (Art. II)
5. Update `specs/PROJECT_STATE.md` §4 with new numbers
6. Commit results alongside code change

---

## Anti-Patterns

- **Whole-file measurement for streaming claims** — must go through real WebSocket (Art. IV)
- **Uncontrolled GPU clock** — always fix clock, report clock speed in results
- **Single-run conclusions** — run 3×, report mean ± range
- **Cherry-picking metrics** — report both RTF AND accuracy (trade-offs must be explicit)
- **Optimizing without baseline** — baseline measurement is mandatory before any change

---

## Relationship with SDD

- Performance claims belong in `plan.md` (HOW section) and validated in `tasks.md` completion criteria
- Any perf optimization that touches model numerics requires a `spec.md` amendment and accuracy comparison
- Post-optimization: update `PROJECT_STATE.md` §4 in the same commit

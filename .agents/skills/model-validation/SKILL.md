# Model Validation — Numerical Oracles and Contextual Product Review

**Domain:** ASR (Qwen3-ASR) vs PyTorch, diarization (Sortformer) vs NeMo, numerical equivalence
**Triggers:** new model implementation, precision change, kernel fusion,
"verify ASR accuracy", speaker/ASR result review

---

## When to Use

- Implementing a new model stage (encoder layer, decoder, feature extractor)
- Any numerical change: precision reduction (bf16→int8), kernel fusion, approximation
- Before merging any change that touches model output values
- When asked: "is the ASR output correct?" or "verify against oracle"
- Constitution Art. II mandates validation for EVERY model stage before "done"

---

## Methodology

### Step 1 — Identify the oracle reference

| Model | Oracle Source | Reference Data Location | Validated Tolerance |
|-------|--------------|------------------------|--------------------|
| ASR Mel | PyTorch `whisper_mel` | `models/reference/asr/mel_output.f32` | 3.9e-3 (RMSE) |
| ASR Encoder | PyTorch Qwen3-ASR encoder | `models/reference/asr/encoder_output.f32` | 1.3e-3 (RMSE) |
| ASR Decoder | PyTorch argmax decode | `models/reference/asr/decoder_output.f32` | argmax-match |
| Diarization | NeMo Sortformer | `models/reference/asr_ops/` | <5e-3 (forward), <1e-2 (streaming), <1e-4 (incremental) |

### Step 2 — Generate oracle outputs

Use Python scripts in `tools/`:
```bash
# Generate ASR mel reference
python3 tools/gen_asr_reference.py --output-dir models/reference/asr/

# Compare against C++ implementation
./build/asr_testmp3 --compare-mel models/reference/asr/mel_output.f32
```

### Step 3 — Run numerical comparison

C++ test binaries compare against `.f32` reference files:
```bash
./build/test/test_reference     # ASR stage comparisons
./build/test/test_kernels       # GPU kernel CPU-reference validation
cd build && ctest -R reference  # All reference-based tests
```

Validation criteria:
- **Encoder/decoder**: RMSE must be within recorded tolerance (see PROJECT_STATE.md §3)
- **Kernels**: element-wise relative error < 1e-5 for float32, < 1e-2 for bf16-equivalent
- **Transcript output**: token-level argmax match (exact string match for greedy decode)

### Step 4 — End-to-end contextual semantic review

Capture the exact real-WebSocket terminal artifact and arrange its unjudged
evidence beside `test.txt`. Then read every in-scope item with its surrounding
conversation, assign the semantic/speaker judgment directly, complete the
required reverse-order second pass, reconcile disagreements, and manually
derive/check every reported result.

No code, script, test, notebook, formula, query, metric, or algorithm may assign
correctness, calculate accuracy, rank/select a candidate, or issue a product
verdict. CER, DER, string distance, timestamp overlap, duration mapping, and
embedding scores are not product-evaluation methods.

### Step 5 — Document and gate

- Record validated tolerances in the relevant `spec.md` acceptance criteria
- If tolerance drifts: investigate root cause before proceeding. Do NOT adjust tolerance to pass.
- If the contextual semantic review shows degradation, the change is rejected
  by default (Art. II). Exception requires explicit approval.

---

## Anti-Patterns

- **Skipping oracle comparison** — "looks reasonable" is not validation (Art. II §4)
- **Adjusting tolerances post-hoc** — tolerances are set by oracle measurement, not by what passes
- **Whole-file shortcut for streaming accuracy** — must validate through streaming path (Art. IV)
- **Using GPU results as self-reference** — CPU oracle or PyTorch reference required
- **Code-based product evaluation** — numerical tests validate implementation
  parity only; they never label, total, rank, select, or accept business results

---

## Relationship with SDD

- Accuracy requirements and tolerances are defined in `spec.md` acceptance criteria
- Validation scripts and reference data are documented in `plan.md` (validation strategy)
- Each model-stage task in `tasks.md` has a validation step that compares against oracle
- Verified tolerances are recorded in `/memories/repo/` and `PROJECT_STATE.md`

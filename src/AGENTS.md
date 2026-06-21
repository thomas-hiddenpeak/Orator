# Orator — src/ Implementation Layer

Implementation directory mirroring `include/` structure. One `.cc`/`.cu` per public header.

**Do NOT repeat root AGENTS.md — read it first.**

---

## STRUCTURE

```
src/
├── core/          # Data types, base contracts
├── feature/       # Mel spectrogram, WhisperMel (GPU)
├── gpu/           # Memory mgmt, buffer, kernels, lock, scheduler
├── io/            # Safetensor loader, tokenizer, JSON sink, audio I/O
├── model/         # Concrete models (Sortformer, Qwen3-ASR) + registration
├── net/           # WebSocket server, HTTP static server
├── pipeline/      # Orchestration workers + timeline
└── protocol/      # Topic registry, storage backends, envelope
```

---

## RULES

### Layering (strict)
Dependencies point inward toward `core/`. Never import from:
- `pipeline/` → NO
- `model/` → NO
- Anything outside `src/` → NO

### One file = one type
- `foo.h` in `include/<layer>/` → `foo.cc` in `src/<layer>/`
- No monolithic files. If a file exceeds ~250 lines of logic, split.

### CUDA kernels
- `.cu` files in `src/gpu/` for device kernels
- `.cu` files in `src/model/` for model-specific GPU compute
- Every kernel validated against CPU reference in `test/`
- Every CUDA call wrapped in error check macro

### Registration pattern
New models go through `BuiltinRegistration` in `src/model/builtin_registration.cc`. Never hardcode model selection in consumers.

### Build
All sources compiled into `orator_core` static library (see `CMakeLists.txt`). The library is linked by every executable target. New `.cc`/`.cu` files must be added to `ORATOR_CORE_SOURCES` in the root CMakeLists.txt.

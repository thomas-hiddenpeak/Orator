# Security Review — Orator Audit Protocols

**Domain:** Network attack surface, WebSocket security, model file integrity, dependency audit, AGPL compliance
**Triggers:** pre-release security check, vulnerability audit, network-facing change, dependency update

---

## When to Use

- Before any release or deployment
- When modifying `src/net/` (WebSocket server, HTTP server)
- When adding new network-facing functionality
- When updating vendored code (`third_party/minimp3`) or `libwebsockets` version
- When asked: "security review", "audit the service", "check for vulnerabilities"

---

## Audit Checklist

### 1. WebSocket Transport (`src/net/websocket_server.cc`)

- [ ] Input validation: message size limits (reject oversized frames before buffering)
- [ ] Message parsing: JSON parse error handling (malformed payload → close with 1003)
- [ ] Resource exhaustion: max concurrent connections, per-connection buffer limits
- [ ] Close handling: orderly shutdown (1000), protocol error (1003), abnormal close (1006)
- [ ] No eval/exec: the WS handler never evaluates dynamic content from the wire
- [ ] TLS readiness: `LWS_WITH_SSL` is off by default, but the code should handle both paths

### 2. HTTP Static Server (`src/net/http_static_server.cc`)

- [ ] Path traversal: `ORATOR_UI_ROOT` must resolve within `web/` — reject `..` in URL paths
- [ ] MIME types: served files have correct Content-Type (`.html`, `.js`, `.css` only)
- [ ] Directory listing: must be disabled — only serve explicit files

### 3. Build & Supply Chain

- [ ] `libwebsockets` fetched via `FetchContent` with pinned `GIT_TAG v4.3.3` (not `main`)
- [ ] `third_party/minimp3` is vendored, static, offline-only (never on runtime path)
- [ ] CUDA runtime (`cudart`, `cuBLAS`): from system package, no network fetch at build
- [ ] Git LFS: model files tracked with `.safetensors`, `.npz`, etc. — verify no secrets in LFS

### 4. Runtime Safety

- [ ] RAII wrappers for all GPU allocations (no leaked `cudaMalloc`)
- [ ] Thread safety: `gpu::DeviceLock` serializes GPU access, `SharedAudioBuffer` mutex-guarded
- [ ] Signal handling: `SIGINT`/`SIGTERM` for graceful shutdown (in-flight audio flushed before exit)
- [ ] No hardcoded secrets, API keys, or credentials in the codebase

---

## Methodology

```
Phase 1 — Static analysis
├── grep for dangerous patterns (sprintf without bounds, unchecked memcpy, format strings)
├── grep for debug paths left enabled (fprintf(stderr) in production)
└── grep for "TODO", "FIXME", "HACK" in security-critical files (src/net/, src/protocol/)

Phase 2 — Dependency scan
├── Check CVEs for libwebsockets v4.3.3
└── Verify no new runtime dependencies (Constitution Art. I)

Phase 3 — Penetration mindset
├── Send malformed WebSocket frames (oversized, binary instead of text, partial frames)
├── Path traversal attempts on HTTP server (../, %2e%2e/, encoded slashes)
└── Connection flood (many concurrent connections, slow loris pattern)
```

---

## AGPL Compliance

- The `COPYRIGHT` and `LICENSE` files must be distributed with the binary
- If modified and provided as a network service: source code must be offered to users
- No proprietary dependencies linked in — this is a hard requirement (Art. I)

---

## Anti-Patterns

- **Security through obscurity** — no secret endpoints, no hidden flags
- **"We'll fix it in production"** — every finding must be resolved or explicitly waived before deployment
- **Disabling security features for convenience** — e.g. disabling message size limits for testing
- **Unpinned dependencies** — every external dependency must be pinned to a specific version

---

## Relationship with SDD

- Security findings that affect the architecture require a `plan.md` amendment
- Critical findings block release and must be tracked as a spec revision or new spec
- Approved waivers are recorded in `PROJECT_STATE.md` with rationale

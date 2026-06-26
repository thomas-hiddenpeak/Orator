# Cloud Deploy — Orator Deployment and Containerization

**Domain:** Deployment, containerization, CI/CD, release preparation
**Triggers:** deployment changes, containerization, CI/CD pipeline modifications, release preparation

---

## When to Use

- Modifying or creating deployment configurations (Dockerfiles, Kubernetes manifests)
- Updating CI/CD pipelines (GitHub Actions, GitLab CI, etc.)
- Preparing for release or deployment to production environments
- When asked: "deploy Orator", "containerize the project", "update CI/CD"

---

## Architecture Overview

### Current deployment targets
- **Edge devices**: Jetson Orin/Thor (Ubuntu-based)
- **Runtime environment**: Pure C++/CUDA with zero third-party runtime dependencies
- **Build system**: CMake with CUDA support

### Deployment constraints (Constitution Art. I)
- **Zero runtime dependencies**: Production runtime is pure C++20/CUDA only
- **No third-party runtime libraries**: Only C++ standard library, libc, and CUDA toolkit runtime libraries are permitted
- **Vendored sources**: `third_party/minimp3` is allowed only in offline tooling and tests, never on a runtime path

---

## Key Patterns

### Containerization
- Use NVIDIA CUDA base images for GPU-enabled deployments
- Ensure minimal image size while including required CUDA runtime libraries
- Do not include Python, PyTorch, or other development tools in production images

### CI/CD Pipeline
- GitHub Actions workflow in `.github/workflows/ci.yml` handles:
  - CUDA 12.5.1 toolchain setup
  - CMake configuration and build
  - CTest execution with `--output-on-failure`
  - Compiler warning checks
  - Python test syntax verification

### Release Preparation
- Verify all models are tracked via Git LFS (`*.safetensors`, `.npz`, `.npy`, `.f32`, `.i32`, `.pcm`)
- Ensure `COPYRIGHT` and `LICENSE` files are distributed with the binary
- Verify AGPL compliance for network service deployments

---

## Change Guidelines

| Change Type | Files | Notes |
|-------------|-------|-------|
| Docker configuration | `Dockerfile*` | Use NVIDIA CUDA base images, minimize image size |
| CI/CD pipelines | `.github/workflows/*.yml` | Follow existing patterns, ensure build and test steps |
| Release scripts | `tools/release_*` | Ensure LFS files are properly handled |
| Deployment manifests | `deploy/*.yaml` | Ensure GPU access and CUDA runtime availability |

### Constraints
- **Zero runtime dependencies** — production images must not include development tools or third-party libraries
- **CUDA compatibility** — ensure base images match required CUDA version (12.5.1)
- **AGPL compliance** — source code must be offered to users if modified and provided as a network service
- **Git LFS verification** — model files must be properly tracked and accessible

---

## Anti-Patterns

- **Including development tools in production images** — no Python, PyTorch, or development headers in production images
- **Unpinned dependencies** — all external dependencies must be pinned to specific versions
- **Ignoring AGPL requirements** — if modified and provided as a network service, source code must be offered to users
- **Breaking zero-dependency policy** — no third-party runtime libraries in production images

---

## Relationship with SDD

- Deployment changes should be documented in `plan.md` (HOW section)
- CI/CD pipeline changes should be validated through the GitHub Actions workflow
- Release preparation should update `PROJECT_STATE.md` with release information
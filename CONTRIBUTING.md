# Contributing to Orator

Thank you for your interest in contributing to the Orator project! This document provides guidelines and information for contributors.

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/version/2/0/code_of_conduct/). By participating, you are expected to uphold this code.

## Development Setup

### Prerequisites
- CMake 3.20 or higher
- NVIDIA CUDA Toolkit
- GCC 10+ or Clang 12+
- Python 3.8+
- Git

### Build Instructions
```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j

# Test
cd build && ctest --output-on-failure
```

## Coding Standards

### C++ Style Guide
We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with the following specific conventions:

- **Indentation**: 2 spaces (no tabs)
- **Type names**: `PascalCase`
- **Method names**: `PascalCase`
- **Variable names**: `lower_snake_case`
- **Member variables**: `lower_snake_case_` (trailing underscore)
- **Interface names**: `I` prefix (e.g., `IAsr`, `IDiarizer`)
- **Header guards**: `#pragma once`

### Example
```cpp
class IAsrPipeline {
 public:
  virtual ~IAsrPipeline() = default;
  virtual bool Initialize(const std::string& model_path) = 0;
  virtual std::string Process(const AudioBuffer& audio) = 0;

 private:
  std::string model_path_;
  bool is_initialized_ = false;
};
```

### Python Style Guide
We follow [PEP 8](https://peps.python.org/pep-0008/) for Python code:
- 4 spaces for indentation
- Maximum line length: 100 characters
- Type hints required for all functions

## Project Structure

```
Orator/
├── include/          # Public headers — one per type, layered by domain
│   ├── core/         #   Interfaces, data types, contracts
│   ├── model/        #   Model interfaces (IAsr, IDiarizer)
│   ├── pipeline/     #   Pipeline orchestration interfaces
│   ├── protocol/     #   Topic-based protocol layer
│   ├── gpu/          #   GPU memory & synchronization
│   ├── io/           #   File I/O, tokenizers
│   ├── net/          #   WebSocket transport
│   └── feature/      #   Feature extraction (mel)
├── src/              # Implementations — mirrors include/ structure
├── test/             # Test scripts and data
│   └── data/         # Test audio files
├── test_legacy/      # Legacy test scripts (deprecated)
├── tools/            # Development tools and probes
├── web/              # Web UI
├── models/           # Model weights (Git LFS)
├── specs/            # SDD artifacts
└── .specify/         # Constitution and governance
```

## Layering Rules

Dependencies point toward the bottom layers, with `core/` as the base:
```
protocol/  net/
    ↑       ↑
    pipeline/
    ↑
  model/
    ↑
  gpu/ io/ feature/
    ↑     ↑
      core/
```

**Important**: `protocol/` is consumed by both `pipeline/` and `net/`. It functions as a shared dependency layer, not as an outermost layer.

## Testing

### Running Tests
```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test
ctest -R test_name --output-on-failure

# Run real business test
python3 test/real_business_test.py --test-file test/data/test.mp3 --duration 120
```

### Test Organization
- **test/**: Active test scripts for current development
- **test_legacy/**: Legacy test scripts (deprecated, kept for reference)
- **test/data/**: Test audio files and fixtures

## Commit Message Format

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
type(scope): description

[optional body]

[optional footer(s)]
```

### Types
- `feat`: A new feature
- `fix`: A bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

### Examples
```
feat(asr): add sliding window support for long audio
fix(diar): correct speaker segmentation boundary calculation
docs: update API documentation for pipeline interface
```

## Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests for new functionality
5. Update documentation as needed
6. Commit your changes (`git commit -m 'feat(scope): add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### PR Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated and passing
- [ ] No new warnings in build
- [ ] CI checks passing

## Code Review Guidelines

### Reviewers should check for:
- Correctness and completeness
- Code style adherence
- Test coverage
- Documentation quality
- Performance implications
- Security considerations

### Authors should:
- Respond to all review comments
- Make requested changes promptly
- Explain design decisions when asked
- Keep PRs focused and manageable

## Architecture Decision Records

For significant architectural decisions, create an ADR in the `docs/adr/` directory following the [MADR 3.0.0](https://github.com/adr/madr) format.

## Getting Help

- Check existing issues and documentation
- Ask questions in GitHub Discussions
- Reach out to maintainers

## License

By contributing, you agree that your contributions will be licensed under the project's license. See [LICENSE](LICENSE) for details.
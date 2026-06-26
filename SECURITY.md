# Security Policy

## Supported Versions

The following versions of Orator are currently supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1   | :x:                |

## Reporting a Vulnerability

We take the security of Orator seriously. If you believe you have found a security vulnerability, please report it to us as described below.

**Please do NOT report security vulnerabilities through public GitHub issues.**

Instead, please report them via email to [security@orator-project.org](mailto:security@orator-project.org) (if available) or through the GitHub Security Advisories feature.

You should receive a response within 48 hours. If for some reason you do not, please follow up via email to ensure we received your original message.

Please include the following information in your report:
- Type of issue (e.g., buffer overflow, injection, etc.)
- Full paths of source file(s) related to the manifestation of the issue
- The location of the affected source code (tag/branch/commit or direct URL)
- Any special configuration required to reproduce the issue
- Step-by-step instructions to reproduce the issue
- Proof-of-concept or exploit code (if possible)
- Impact of the issue, including how an attacker might exploit it

## Preferred Languages

We prefer all communications to be in English.

## Vulnerability Disclosure Process

1. **Report**: Submit your vulnerability report via the preferred channel
2. **Acknowledge**: We will acknowledge receipt within 48 hours
3. **Assess**: We will assess the report and determine the impact
4. **Develop**: We will develop a fix and test it thoroughly
5. **Release**: We will release a patch and update this document
6. **Credit**: We will credit the reporter (unless they prefer to remain anonymous)

## Security Best Practices

### For Users
- Keep your Orator installation up to date
- Use strong passwords for any authentication mechanisms
- Run Orator in a secure network environment
- Regularly review logs for suspicious activity
- Use TLS/WSS for WebSocket connections in production

### For Developers
- Follow secure coding practices
- Validate all input data
- Use RAII for resource management
- Avoid raw pointers and manual memory management
- Keep dependencies up to date
- Review code for security vulnerabilities before merging

### Dependency Security
- We use minimal third-party dependencies
- Dependencies are fetched from trusted sources (GitHub)
- Regular dependency audits are performed
- Consider using Dependabot for automated dependency updates

## Security Considerations

### Memory Safety
- RAII-based memory management throughout the codebase
- No raw `new`/`delete` or `cudaMalloc`/`cudaFree` on performance paths
- Smart pointers for shared ownership
- Bounds checking for array access

### Input Validation
- All client input is validated before processing
- Audio data format verification
- Message size limits to prevent DoS
- Timeout handling for long-running operations

### Network Security
- WebSocket connection validation
- Optional TLS/WSS support
- Client authentication (if enabled)
- Rate limiting for connection attempts

### GPU Security
- CUDA error checking on all GPU operations
- Memory access bounds validation
- Kernel execution timeout handling
- GPU memory leak prevention

## Bug Bounty Program

Currently, we do not have a formal bug bounty program. However, we appreciate responsible disclosure of security vulnerabilities and will acknowledge contributors who help improve our security.

## Acknowledgments

We would like to thank the following individuals/organizations for their responsible disclosure of security vulnerabilities:

- [List of security researchers who have reported vulnerabilities]

## Updates to This Policy

We will review and update this security policy annually or as needed to reflect changes in our project or security landscape.

**Last Updated:** 2026-06-26
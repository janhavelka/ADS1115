# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 2.x     | :white_check_mark: |
| 1.x and earlier | :x: |

## Reporting a Vulnerability

If you discover a security vulnerability within this library, please follow
responsible disclosure:

1. **Do NOT** open a public GitHub issue.
2. Email the maintainer at: `info@thymos.cz`.
3. Include:
   - A description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Any suggested fixes (optional)

We will acknowledge receipt within 48 hours and aim to provide a fix or
mitigation within 14 days for critical issues.

## Scope

This library is a framework-neutral device driver. Networking, authentication,
persistent storage, I2C bus recovery, locking, and system retry policy are out
of scope. The core performs no steady-state heap allocation and does not own the
bus or transport context.

## Security Best Practices for Users

- Keep transport callbacks and their user context valid for the full binding
  lifetime.
- Externally serialize every driver call and never call the public API from an
  ISR.
- Enforce callback timeouts and whole-operation deadlines in the system's
  monotonic time domain.
- Treat I2C as untrusted input: preserve transport failures, require verified
  profile replay after dirty/ambiguous writes, and validate samples before
  using them in safety-relevant decisions.
- Apply system watchdog, isolation, retry, and recovery policy at the
  application level.

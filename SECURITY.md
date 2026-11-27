# Security Policy — FeatherTech FARS

## Supported Versions

This project is in early research stage. Security fixes will be applied to the latest commit on the default branch.

## Reporting a Vulnerability

If you discover a security issue:

1. **Do not** open a public GitHub issue for sensitive reports.
2. Contact the maintainer privately (repository owner: syedfurqan01).
3. Provide a clear description, steps to reproduce, and potential impact.

We will acknowledge receipt and work with you on a coordinated disclosure timeline.

## Scope Notes

- The current Memory Profiler collects only local system metrics (RAM, process memory, storage bandwidth). It does not handle user content or network data.
- Future components that process user prompts, knowledge bases, or optional cloud routes will require additional threat modeling; this document will be updated accordingly.

## Preferred Practices

- Prefer memory-safe patterns and careful handling of untrusted model files.
- Avoid introducing unnecessary network capabilities in core measurement tools.
- Keep dependencies minimal and auditable.
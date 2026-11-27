# Contributing to FeatherTech FARS

Thank you for your interest.

## Current Stage

The project is in early research and foundation-building. The highest-value contributions right now are:

1. Careful review of the Memory Profiler design and measurements
2. Reproducible baseline experiments on real low-resource devices
3. High-quality documentation improvements
4. Identification of relevant prior art we may have missed

Large feature PRs that implement adaptive algorithms before solid baselines exist are likely to be deferred.

## Development Environment

Primary development target: **Android + Termux**.

Please prefer changes that remain buildable and testable under Termux. Desktop-only improvements are welcome when they do not break the mobile workflow.

## Workflow

1. Open an issue describing the proposed change or experiment.
2. Keep PRs focused and small.
3. Include:
   - Clear description of what was measured
   - Exact settings and hardware used
   - How to reproduce
4. Do not auto-download large model files in scripts or CI.
5. Preserve the MIT license and existing attribution.

## Code Style

- Prefer clear, modular C++20
- Document non-obvious design decisions
- Include basic tests for new measurement code
- Avoid unnecessary dependencies

## Communication

Be precise. Distinguish:
- Proven fact
- Measurement
- Hypothesis
- Opinion

Negative results are valuable and should be reported.
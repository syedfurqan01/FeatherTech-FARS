# Roadmap — FeatherTech FARS

This is an indicative plan. Scientific validation takes priority over calendar deadlines.

## Month 1 (Current)
- Repository foundation and documentation
- Literature / prior-art review
- **FARS Memory Profiler 0.1** (first measurable artifact)
- Baseline memory and performance experiments on Linux and Termux

## Month 2
- Harden Memory Profiler
- Initial AMAS-style experiments (adaptive budgeting under measured pressure)
- Systematic comparison of full-load vs memory-mapped loading

## Month 3
- Hardware profiler improvements (thermal, battery where measurable)
- Early QAM-oriented experiments (quality-aware cache management)
- PALS exploratory measurements (with strong negative-result expectation)

## Month 4
- Dynamic compute / routing research
- Initial small-model fine-tuning or distillation experiments (if compute allows)
- Intelligence orchestration sketches

## Month 5
- Integration of proven techniques into a thin runtime layer
- Android prototype improvements
- Optional hybrid (local + cloud) experiments under strict privacy controls

## Month 6
- Full benchmark suite evaluation
- Documentation and reproducibility packages
- Decision point: FARS Alpha release **only if** evidence justifies it

## Guiding Principles

- One solid milestone at a time.
- Prefer working, measurable code over large unfinished architectures.
- Publish negative results.
- Keep Termux / Android development workflow viable throughout.
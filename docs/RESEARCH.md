# Research & Prior Art — FeatherTech FARS

## Purpose of This Document

Before claiming novelty we must understand what already exists. This file records the prior-art landscape relevant to FARS and the six proposed research algorithms.

## Key Existing Systems & Techniques

### Inference Runtimes
- **llama.cpp / ggml** — Dominant efficient local inference engine. Strong quantization support (including KV-cache types), memory mapping, multi-backend (CPU, CUDA, Metal, Vulkan, etc.), excellent Android/Termux compatibility.
- **bitnet.cpp** — Specialized runtime for 1.58-bit BitNet models.
- **MLC LLM, ONNX Runtime, Android NNAPI / Qualcomm AI Engine** — Important for mobile and edge deployment.

### Memory & KV-Cache Optimization
- **KIVI** (2024) — Tuning-free asymmetric 2-bit KV-cache quantization (keys per-channel, values per-token). Demonstrated substantial peak-memory reduction with limited quality loss.
- **H2O / Heavy-Hitter Oracle** (NeurIPS 2023) — Dynamic KV eviction that retains high-attention “heavy hitter” tokens plus recent tokens. Can operate at ~20 % cache budget on many tasks.
- llama.cpp native KV quantization and context management.
- PagedAttention (vLLM) and related paging techniques (server-oriented).

### Hybrid / Sparse / Predictive Execution
- **PowerInfer** (2023–2024) — Exploits power-law neuron activation locality. Keeps “hot” neurons on GPU and computes “cold” neurons on CPU. Strong results on consumer GPUs and demonstrated mobile applicability (PowerInfer-2). Closest existing system to predictive component loading and hybrid compute.
- Speculative decoding, early-exit models, mixture-of-experts routing, and various dynamic compute allocation methods.

### Distillation & Adaptive Reasoning
- Extensive literature on knowledge distillation, process supervision, tool-use training, and model routing. No single prior system matches the exact “Reasoning-Guided Model Distillation” framing proposed for FARS, but the building blocks are mature.

## Preliminary Novelty Assessment

| Proposed Algorithm | Closest Prior Art              | Opportunity Level | Notes |
|--------------------|--------------------------------|-------------------|-------|
| AMAS               | llama.cpp + OS allocators     | Moderate         | Adaptive budgeting under hard mobile constraints + thermal/battery awareness still under-served |
| PALS               | PowerInfer, mmap, caching     | Limited / High risk | Pure layer streaming is often slower than resident weights; must be measured rigorously |
| QAM                | KIVI + H2O                    | Good             | Combining asymmetric quant + importance-based eviction + explicit quality measurement under strict budgets |
| DCR                | Speculative decoding, early exit, MoE, PowerInfer | Moderate | Especially valuable when tightly coupled to real device profiles |
| HCR                | Existing hybrid systems, PowerInfer | Moderate     | Privacy-first, energy/cost-aware offline bias is valuable |
| RGM                | Standard distillation literature | Incremental   | Focus on measurable transfer of useful capabilities into very small models |

## Research Stance

- Existing techniques will be integrated or adapted where they are strong.
- New names (AMAS, PALS, etc.) are research hypotheses, not proven inventions.
- Every proposed improvement must be validated against strong baselines (primarily llama.cpp) on the same model, quantization, context length, and hardware.
- Negative results will be published.

## Open Questions (Selected)

1. Under what real-world storage latency and memory-pressure conditions does predictive component loading beat well-tuned memory mapping?
2. How much quality can be preserved when aggressive KV compression + eviction is applied on 2–4 GB devices?
3. Can a lightweight, measurement-driven scheduler meaningfully improve the quality-per-memory and quality-per-energy ratios on Android?

This document will be updated as experiments produce data.
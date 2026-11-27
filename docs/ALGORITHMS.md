# Algorithms — Research Hypotheses

These are **research hypotheses**, not proven inventions. Each must be validated experimentally against strong baselines before any performance claims are made.

## 1. AMAS — Adaptive Memory Allocation Scheduler

**Objective**  
Dynamically allocate memory according to available hardware resources and current inference requirements.

**Potential responsibilities**  
- Model weight allocation  
- KV-cache management  
- Activation memory  
- Runtime overhead reservation  
- Memory-pressure monitoring  
- Adaptive budget adjustment  
- OOM prevention  

**Research question**  
Can an adaptive scheduler improve memory efficiency while preserving acceptable inference quality and latency under hard device limits?

**Status**  
Not implemented. Baselines must be established first via the Memory Profiler.

## 2. PALS — Predictive Adaptive Layer Streaming

**Objective**  
Investigate predictive loading and intelligent caching of model components.

**Critical warning**  
Streaming model weights from storage repeatedly is frequently slower than keeping them in RAM. Any implementation must be measured against memory mapping and standard caching.

**Research question**  
Can predictive loading reduce peak RAM or improve performance under constrained memory *without* unacceptable latency regression?

**Status**  
Not implemented. High risk of negative results.

## 3. QAM — Quality-Aware Adaptive Memory

**Objective**  
Manage context and KV cache according to memory pressure and information importance.

**Potential techniques**  
- KV-cache compression (building on KIVI-style ideas)  
- Token importance estimation  
- Adaptive eviction (building on H2O-style heavy-hitter ideas)  
- Explicit quality measurement after eviction  

**Research question**  
Can we reduce memory usage while preserving the information most important to the model’s response?

**Status**  
Not implemented. Strong prior art exists; the opportunity lies in integration under strict mobile budgets with rigorous quality measurement.

## 4. DCR — Dynamic Compute Routing

**Objective**  
Allocate compute according to task difficulty, uncertainty, risk, and available hardware.

**Research question**  
Can adaptive compute allocation improve the quality-per-unit-compute ratio on low-resource devices?

**Status**  
Not implemented.

## 5. HCR — Hybrid Compute Redistribution

**Objective**  
Coordinate local and optional cloud computation under privacy, latency, energy, and cost constraints.

**Research question**  
Can hybrid execution provide a better balance between intelligence, speed, privacy, and cost while remaining offline-first by default?

**Status**  
Not implemented.

## 6. RGM — Reasoning-Guided Model Distillation

**Objective**  
Investigate transfer of useful problem-solving and tool-use capabilities into smaller models.

**Research question**  
Can smaller models learn useful problem-solving behavior without requiring the memory footprint of larger models?

**Status**  
Not implemented. Distillation literature is mature; focus will be on measurable capability transfer under FARS constraints.

## Evaluation Rule

For every algorithm:

1. State the exact baseline (model, quant, context, hardware, settings).  
2. Run multiple trials.  
3. Report both positive and negative outcomes.  
4. Never claim improvement without the numbers.
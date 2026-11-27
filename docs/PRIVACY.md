# Privacy — FeatherTech FARS

## Principles

1. **Offline-first by default**  
   Inference and knowledge storage should function without network access.

2. **Local by default for sensitive data**  
   User content, conversation history, and personal knowledge remain on-device unless the user explicitly opts in to cloud processing.

3. **Explicit consent**  
   Any cloud fallback or hybrid execution path must be under clear, revocable user control.

4. **Minimal data collection**  
   The project itself does not require telemetry. Any future optional analytics will be documented and opt-in.

5. **Transparency**  
   Users and developers should be able to understand what data leaves the device (if anything) and under what conditions.

## Implications for Architecture

- FARS-Knowledge and context stores are designed for local persistence (SQLite, local vector indices, files).
- Hybrid Compute Redistribution (HCR) is optional and gated.
- The Memory Profiler and early prototypes collect only local system metrics; no user content is transmitted.

## Future Considerations

As cloud-assisted features are explored, this document will be expanded with concrete data-flow diagrams, retention policies, and threat-model notes.
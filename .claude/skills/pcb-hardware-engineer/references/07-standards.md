## § 7 Standards & Reference

**Frameworks:**
- **IPC-2221** — Generic PCB standard (design requirements)
- **IPC-6012** — Qualification and performance (solder joint reliability)
- **IPC-A-610** — Acceptability of electronic assemblies (Class 3)
- **FCC Part 15
- **IEC 61000-4-2** — ESD immunity

| Metric | Formula | Target Range |
|--------|---------|--------------|
| Microstrip Impedance | Z₀ = (87/√(εr+1.41)) × ln(5.98H/(0.8W+T)) | 50Ω ± 10% |
| Differential Impedance | Z_diff = 2 × Z₀ × (1 - C_coupling/C_total) | 90Ω ± 10% |
| Trace Current Capacity | I = 0.048 × ΔT^0.44 × A^0.725 | < 30°C rise |
| Via Current Capacity | I = 0.076 × d^0.54 × ΔT^0.27 | < 30°C rise |
| Decap Self-Resonance | f_SR = 1/(2π√(L×C)) | Target decap resonant freq at noise freq |
| Crosstalk (3W rule) | NEXT ≈ (1/π) × (W/(W+S)) | 3W spacing → < 5% coupling |
| Via Stub Resonance | f_stub = v/(4 × L_stub × √εr) | Remove stubs for >1Gbps |


## § 9 Scenario Examples

### Scenario 1 — DDR4 Routing with Length Matching

**User:** I need to route DDR4 signals on an 8-layer stackup. The address/command/control signals need length matching. What are the specific guidelines?

**Expert:** DDR4 at 2133-3200 MT/s requires precise length matching. Here are the specifications:

**Stackup for DDR4 (8-layer example):**
```
Layer 1 (Top):    Signal (DDR signals) — 0.5oz
Layer 2 (GND):    Plane (continuous ground)
Layer 3 (Plane):  Signal/Power split
Layer 4 (Plane):  DDR VDD plane
Layer 5 (Plane):  DDR VDD plane
Layer 6 (Signal): Signal/Power split
Layer 7 (Plane):  Ground plane
Layer 8 (Bottom): Signal (DDR signals)
```

**Length Matching Requirements:**
| Signal Group | Matching Tolerance | Max Length |
|--------------|-------------------|------------|
| DQ to DQS | ± 2.5mm | 50mm |
| Address/Command to CK | ± 12.7mm | 80mm |
| CK differential pair | ± 0.254mm (skew) | — |
| Control signals | ± 25mm | 100mm |

**Routing Rules:**
```text
- Trace width: 0.1mm (50Ω single-ended)
- Pair spacing: 0.15mm (differential 90Ω)
- DQS: must reference solid ground plane, not split
- Via count: max 2 vias per byte lane
- Keepout from edge: > 3mm (impedance change)
```

[RISK] Do not route DDR across a split in the reference plane — this causes reflections and EMI problems.


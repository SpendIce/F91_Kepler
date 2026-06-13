# F91 Kepler v2 — Schematic Verification Report

**Generated:** 2026-06-13  
**Source of truth:** `/Hardware/v2/tools/netlist.py` (TI SWRS253B, SLUSB42F, DocID029682, DS10925, SLOS854D)  
**Datasheet reference:** `docs/phase2/v2_reference_circuits.md`  
**BOM source:** `Hardware/v2/output/f91_kepler_v2_bom.csv`

---

## FLAGS SUMMARY

Two issues require resolution before PCB tapeout. No flags are power-rail or interface errors on the five primary ICs — both flags are on secondary/protection circuits.

### FLAG-1 — FS8205A gate pin conflict (Q1) ⚠️ VERIFY BEFORE TAPEOUT

The `v2_reference_circuits.md` lists the FS8205A pin map as **[UNVERIFIED — aggregator sites only]**, with G1 at pin 3 and the shared drain (D) at pin 4. The netlist (which claims to be verified against the Fortune FS8205A-DS-12_EN PDF) assigns PROT_OD to **pin 4** and PROT_OC to pin 5, placing a source (BATN) at pin 3.

| Pin | Aggregator data (ref doc) | Netlist (Fortune PDF claimed) |
|-----|--------------------------|-------------------------------|
| 3 | G1 — discharge gate (OD) | BATN (source of FET1) |
| 4 | D\_shared — common drain | PROT\_OD (DW01A discharge gate) |

If the aggregator data is correct, the gate and source assignments for FET1 are swapped in the netlist — this would be a dead short when the protection circuit fires. **Download and verify the Fortune FS8205A-DS-12_EN primary PDF against pin 3 and pin 4 before generating Gerbers.**

### FLAG-2 — ST25DV04K phantom pin 9 (U7) ⚠️ VERIFY IN KICAD

The netlist assigns `"9": "NC"` to U7 (ST25DV04K, SOIC-8 package). SOIC-8 has no exposed pad; pin 9 does not exist. This suggests the KiCad symbol for the -IER8C3 variant includes a phantom EP pin (correct only for the UFDFPN8 package). Confirm that ERC reports no unmatched pin and that the KiCad symbol pin 9 is flagged as a passive/no-connect so no net stub is generated on the SOIC-8 footprint.

---

## Part 1 — Pin-by-Pin Verification Tables

### U1 — CC2652R7RGZ (VQFN-48, TI SWRS253B)

| Pin | Datasheet Name | Net (netlist.py) | Status | Notes |
|-----|---------------|------------------|--------|-------|
| 1 | RF\_P | RF\_P | OK | To balun FL1 pad 4 |
| 2 | RF\_N | RF\_N | OK | To balun FL1 pad 3 |
| 3 | X32K\_Q1 | X32K\_1 | OK | 12 pF load cap C12 to GND |
| 4 | X32K\_Q2 | X32K\_2 | OK | 12 pF load cap C13 to GND |
| 5 | DIO\_0 | NC | OK | Unused |
| 6 | DIO\_1 | NC | OK | Unused |
| 7 | DIO\_2 | UART\_RX | **CONFIRMED** | BSL UART0 RX — fixed ROM pin (SWRA640 Table 3-3) |
| 8 | DIO\_3 | UART\_TX | **CONFIRMED** | BSL UART0 TX — fixed ROM pin; pogo TP6/TP7 correct |
| 9 | DIO\_4 | NC | OK | Unused |
| 10 | DIO\_5 | SDA | OK | I2C bus (high-drive capable) |
| 11 | DIO\_6 | SCL | OK | I2C bus (high-drive capable) |
| 12 | DIO\_7 | ACC\_INT1 | OK | LIS2DW12 INT1 (high-drive capable) |
| 13 | VDDS2 | VDDS | OK | 100 nF via C4 on VDDS rail |
| 14 | DIO\_8 | ACC\_INT2 | OK | LIS2DW12 INT2 |
| 15 | DIO\_9 | LCD\_MOSI | OK | Sharp LCD SPI data |
| 16 | DIO\_10 | LCD\_SCLK | OK | Sharp LCD SPI clock |
| 17 | DIO\_11 | LCD\_CS | OK | Sharp LCD CS (active HIGH) |
| 18 | DIO\_12 | LCD\_DISP | OK | Sharp LCD DISP |
| 19 | DIO\_13 | LCD\_EXTCOMIN | OK | Sharp LCD EXTCOMIN (HW VCOM) |
| 20 | DIO\_14 | NFC\_GPO | OK | ST25DV04K GPO interrupt |
| 21 | DIO\_15 | BTN\_1 | OK | Also BSL backdoor per DIO plan |
| 22 | VDDS3 | VDDS | OK | 100 nF via C3 on VDDS rail |
| 23 | DCOUPL | DCOUPL | OK | C10 1 µF to GND (required) |
| 24 | JTAG\_TMSC | SWD\_TMS | OK | SWD debug, pogo TP4 |
| 25 | JTAG\_TCKC | SWD\_TCK | OK | SWD debug, pogo TP5 |
| 26 | DIO\_16 | NC | OK | Unused |
| 27 | DIO\_17 | NC | OK | Unused |
| 28 | DIO\_18 | NC | OK | Unused |
| 29 | DIO\_19 | NC | OK | Unused |
| 30 | DIO\_20 | NC | OK | Unused |
| 31 | DIO\_21 | CHG\_DET | OK | BQ51050B /CHG signal (active-LOW); R9 100 k pullup to VDDS |
| 32 | DIO\_22 | BUZZER | OK | Piezo drive via R3 100 Ω + R4 1 k bleed |
| 33 | DCDC\_SW | DCDC\_SW | OK | L2 6.8 µH → VDDR |
| 34 | VDDS\_DCDC | VDDS | OK* | *On VDDS rail. PCB layout must place C5 (22 µF) and at least one 100 nF cap physically adjacent to pin 34 — dedicated per TI LAUNCHXL reference design |
| 35 | RESET\_N | nRESET | OK | R1 100 k pullup to VDDS + C11 100 nF to GND |
| 36 | DIO\_23 | NC | OK | Unused (analog-capable) |
| 37 | DIO\_24 | HAPTIC\_EN | OK | DRV2605L EN |
| 38 | DIO\_25 | BTN\_2 | OK | |
| 39 | DIO\_26 | BTN\_3 | OK | |
| 40 | DIO\_27 | NC | OK | Unused (analog-capable) |
| 41 | DIO\_28 | NC | OK | Unused (analog-capable) |
| 42 | DIO\_29 | NC | OK | Unused (analog-capable) |
| 43 | DIO\_30 | NC | OK | Unused (analog-capable) |
| 44 | VDDS | VDDS | OK | 100 nF via C2; ferrite FB1 from +3V0 |
| 45 | VDDR | VDDR | OK | C7 22 µF + C8 100 nF; supplied from DCDC via L2 |
| 46 | X48M\_N | X48M\_N | OK | FA-128 48 MHz, Y1 pin 1; internal load caps per Epson spec |
| 47 | X48M\_P | X48M\_P | OK | FA-128 48 MHz, Y1 pin 3 |
| 48 | VDDR\_RF | VDDR | OK | 100 nF via C9 on VDDR rail |
| EP (49) | GND | GND | OK | Thermal pad — sole GND connection for MCU |

**Power-domain checklist confirmed:**

| Requirement | Implementation | Result |
|-------------|---------------|--------|
| VDDS (pins 13, 22, 44) on VDDS net | All three on VDDS, ferrite from +3V0 | OK |
| VDDS\_DCDC (pin 34) on VDDS net | On VDDS, see placement note above | OK |
| VDDR (pin 45) + VDDR\_RF (pin 48) on VDDR net | Both VDDR, supplied by DCDC | OK |
| DCDC\_SW → inductor → VDDR | L2 6.8 µH MLZ2012N6R8 between DCDC\_SW and VDDR | OK |
| DCOUPL 1 µF to GND | C10 1 µF | OK |
| RESET\_N 100 k pullup + 100 nF | R1 + C11 | OK |
| 48 MHz crystal (internal load caps) | Y1 FA-128, no external load caps in netlist | OK |
| 32.768 kHz crystal + 2×12 pF load caps | Y2 FC-135 + C12/C13 | OK |
| RF\_P/RF\_N to balun | FL1 LFB182G45BG5D920 pads 4/3 | OK |
| EP → GND | Pin 49 on GND | OK |
| BSL UART: DIO\_2=RX (pin 7), DIO\_3=TX (pin 8) | Verified SWRA640 Table 3-3 | **CONFIRMED** |

---

### U2 — BQ51050B (VQFN-20 RHL, TI SLUSB42F)

| Pin | Datasheet Name | Net (netlist.py) | Status | Notes |
|-----|---------------|------------------|--------|-------|
| 1 | PGND | GND | OK | |
| 2 | AC1 | AC1 | OK | Qi coil side 1; resonant C19/C20 (BENCH-TUNE) |
| 3 | BOOT1 | BOOT1 | OK | C23 10 nF to AC1 |
| 4 | BAT | VBAT | OK | Battery+ output; C29 1 µF + C30 100 nF to GND |
| 5 | CLAMP1 | CLAMP1 | OK | C25 470 nF (25 V) to AC1 |
| 6 | COMM1 | COMM1 | OK | C27 22 nF (25 V) to AC1 |
| 7 | /CHG | CHG\_DET | OK | Open-drain, active LOW; R9 100 k pullup to VDDS; MCU DIO\_21 |
| 8 | /AD-EN | NC | OK | Float per datasheet |
| 9 | AD | GND | OK | → PGND per datasheet |
| 10 | TERM | TERM | OK | R7 2.4 kΩ to GND (10 % termination current) |
| 11 | EN2 | GND | OK | → GND per datasheet |
| 12 | ILIM | ILIM | OK | R5 3.92 kΩ to FOD\_TAP; sets I\_BULK ≈ 76 mA for 150 mAh cell |
| 13 | TS/CTRL | TS | OK | R8 10 kΩ to GND (NTC disabled, safe per design guide) |
| 14 | FOD | FOD\_TAP | OK | Taps ILIM divider junction (R5 + R6 200 Ω to GND) |
| 15 | COMM2 | COMM2 | OK | C28 22 nF (25 V) to AC2 |
| 16 | CLAMP2 | CLAMP2 | OK | C26 470 nF (25 V) to AC2 |
| 17 | BOOT2 | BOOT2 | OK | C24 10 nF to AC2 |
| 18 | RECT | RECT | OK | C16 10 µF + C17 10 µF + C18 100 nF to GND |
| 19 | AC2 | AC2 | OK | Qi coil side 2; W1 0 Ω jumper from COIL2 |
| 20 | PGND | GND | OK | |
| EP (21) | PGND | GND | OK | Exposed thermal pad |

**BQ51050B sub-checklist:**

| Requirement | Implementation | Result |
|-------------|---------------|--------|
| AC1/AC2 to coil + resonant caps | COIL1→C19/C20→AC1; COIL2→W1→AC2; C21/C22 across AC1-AC2 | OK (BENCH-TUNE) |
| BOOT cap pairs (10 nF × AC) | C23 BOOT1→AC1, C24 BOOT2→AC2 | OK |
| CLAMP cap pairs (470 nF × AC) | C25 CLAMP1→AC1, C26 CLAMP2→AC2 | OK |
| COMM cap pairs (22 nF × AC) | C27 COMM1→AC1, C28 COMM2→AC2 | OK |
| RECT decoupling (2×10 µF + 100 nF) | C16 + C17 + C18 | OK |
| BAT decoupling (1 µF + 100 nF) | C29 + C30 | OK |
| ILIM + FOD resistor divider | R5 3.92 kΩ + R6 200 Ω | OK |
| TERM resistor | R7 2.4 kΩ | OK |
| TS resistor (no NTC) | R8 10 kΩ | OK |
| /CHG to MCU with pullup | CHG\_DET → DIO\_21, R9 100 k → VDDS | OK |

---

### U5 — LIS2DW12 (LGA-12 2×2 mm, ST DocID029682)

| Pin | Datasheet Name | Net (netlist.py) | Status | Notes |
|-----|---------------|------------------|--------|-------|
| 1 | SCL/SPC | SCL | OK | |
| 2 | CS | +3V0 | **OK** | Tied HIGH → I2C mode (not GND/floating). Internal 20 kΩ pullup also present at 3.6 V; explicit tie confirmed correct |
| 3 | SDO/SA0 | GND | OK | I2C address 0x18 (default) |
| 4 | SDA/SDI/SDO | SDA | OK | |
| 5 | NC | NC | OK | May be tied to VDD, VDD\_IO, or GND per datasheet |
| 6 | GND | GND | OK | |
| 7 | RES | GND | OK | Reserved — must tie to GND per datasheet |
| 8 | GND | GND | OK | |
| 9 | VDD | +3V0 | OK | C34 100 nF to GND |
| 10 | VDD\_IO | +3V0 | OK | C35 100 nF to GND |
| 11 | INT2 | ACC\_INT2 | OK | MCU DIO\_8 (pin 14) |
| 12 | INT1 | ACC\_INT1 | OK | MCU DIO\_7 (pin 12) |

**LIS2DW12 I2C strapping confirmed:** CS (pin 2) tied HIGH (+3V0) ✓. SA0 (pin 3) tied LOW (GND) → address 0x18 ✓.

---

### U6 — DRV2605L (VSSOP-10 DGS, TI SLOS854D)

| Pin | Datasheet Name | Net (netlist.py) | Status | Notes |
|-----|---------------|------------------|--------|-------|
| 1 | REG | DRV\_REG | OK | C36 1 µF to GND (required per SLOS854D Rev D) |
| 2 | SCL | SCL | OK | Shared I2C bus |
| 3 | SDA | SDA | OK | |
| 4 | IN/TRIG | GND | OK | Tied GND per TI Rev D recommendation when unused |
| 5 | EN | HAPTIC\_EN | OK | MCU DIO\_24 (pin 37) |
| 6 | VDD/NC | +3V0 | OK | Optional supply pin; tied VDD per datasheet |
| 7 | OUT+ | MOT\_P | OK | Differential ERM/LRA output+; TP13 test point |
| 8 | GND | GND | OK | |
| 9 | OUT− | MOT\_N | OK | Differential ERM/LRA output−; TP14 test point |
| 10 | VDD | +3V0 | OK | C37 100 nF to GND. Note: datasheet requires 1 µF; +3V0 rail has additional 1 µF at C33/C40. Confirm 1 µF cap placed within 2 mm of pin 10 during layout |

**OUT+/OUT− to motor pads confirmed.** I2C address 0x5A (fixed, no pin strap). EN → MCU DIO\_24 ✓. REG cap 1 µF ✓.

---

### U7 — ST25DV04K-IER8C3 (SOIC-8, ST DS10925)

| Pin | Datasheet Name | Net (netlist.py) | Status | Notes |
|-----|---------------|------------------|--------|-------|
| 1 | V\_EH | NC | OK | Energy harvesting output — HiZ when EH disabled. Not used in this design; NC is valid |
| 2 | AC0 | NFC\_A | OK | NFC antenna coil connection; TP15 test point |
| 3 | AC1 | NFC\_B | OK | NFC antenna coil connection; TP16 test point |
| 4 | VSS | GND | OK | |
| 5 | SDA | SDA | OK | Open-drain; shared I2C bus pullup R13 3.3 kΩ to +3V0 |
| 6 | SCL | SCL | OK | Open-drain; shared I2C bus pullup R14 3.3 kΩ to +3V0 |
| 7 | GPO | NFC\_GPO | OK | Open-drain (-IE suffix); R12 100 kΩ to +3V0 (> 4.7 kΩ required) ✓; MCU DIO\_14 (pin 20) |
| 8 | VCC | +3V0 | OK | C38 100 nF to GND |
| 9* | (phantom EP) | NC | FLAG-2 | SOIC-8 has no exposed pad. Verify KiCad symbol does not drive a net here |

---

## Part 2 — LCSC / JLCPCB Assembly BOM

**Tier definitions used below:**
- **Basic** = JLCPCB Basic Parts library (lowest assembly surcharge)
- **Preferred** = JLCPCB Preferred Parts (reduced extended surcharge)
- **Extended** = JLCPCB Extended Parts (per-type setup fee applies, ~$3 each)
- **[CHECK STOCK]** = no confirmed LCSC C-number found; must verify before BOM upload

For standard 0402 resistors and capacitors in common values (100 nF, 1 µF, 10 nF, 22 nF, 100 kΩ, 3.3 kΩ, etc.) — use "JLCPCB Basic generic 0402" and assign during BOM upload from the Basic library. The 12 pF load caps (C12, C13) and 22 µF/10 µF 0603 values are also readily available as Basic or Preferred.

### Active ICs

| Reference | Value / MPN | Package | LCSC Part # | Tier | Notes |
|-----------|------------|---------|-------------|------|-------|
| U1 | CC2652R7RGZR | VQFN-48 7×7 mm | **[CHECK STOCK]** | Extended | CC2652R7RGZR not found directly on LCSC. **C3606870** lists CC2652R74T0RGZR (same die, rev 4T0 silicon); confirm interchangeability with TI before ordering. Fallback per fab doc: CC2652R1FRGZR — requires SDK 4.x instead of 7.x, different flash size (352 kB vs 704 kB); treat as last resort only |
| U2 | BQ51050BRHLR | VQFN-20 RHL | **C133307** | Extended | Texas Instruments, 2313 units in stock at time of search |
| U3 | DW01A | SOT-23-6 | **C351410** | Extended | PUOLOP brand; also on JLCPCB C351410. Fortune Semicon variant: C61503 |
| U4 | XC6206P302MR | SOT-23 | **C9972** | Extended | Torex original XC6206P302MR-G; clones (TWGMC C5250984, MSKSEMI C5252898) available cheaper but verify dropout and noise specs |
| U5 | LIS2DW12TR | LGA-12 2×2 mm | **C189624** | Extended | STMicroelectronics, 23 732 units in stock |
| U6 | DRV2605LDGSR | VSSOP-10 | **C527464** | Extended | Texas Instruments, 526 units in stock |
| U7 | ST25DV04K-IER8C3 | SOIC-8 | **C2654866** | Extended | -IER8C3 matches netlist symbol. Alternative -IER6S3 (C155601) is an older speed grade; same pinout, confirm revision compatibility with ST |

### Discrete Semiconductors / Protection

| Reference | Value / MPN | Package | LCSC Part # | Tier | Notes |
|-----------|------------|---------|-------------|------|-------|
| Q1 | FS8205A | TSSOP-8 | **C16052** or **C14212** | Basic (JLCPCB) | Fortune Semicon. C14212 appears in JLCPCB parts library directly — likely Basic or Preferred. C16052 is the LCSC catalog entry. **Resolve FLAG-1 before ordering** |

### Passives — Inductors and Ferrites

| Reference | Value / MPN | Package | LCSC Part # | Tier | Notes |
|-----------|------------|---------|-------------|------|-------|
| FB1 | BLM18HE152SN1D | 0603 | **C82155** | Extended | Murata 1.5 kΩ @ 100 MHz power ferrite; 254 900 units in stock |
| L2 | MLZ2012N6R8LT000 | 0805 | **C82157** | Extended | TDK 6.8 µH DC-DC inductor; priced from $0.0315 |
| FL1 | LFB182G45BG5D920 | LFB18 1608 | **C90492** | Extended | Murata integrated 2.4 GHz balun + filter; 2820 units in stock |

### Crystals

| Reference | Value / MPN | Package | LCSC Part # | Tier | Notes |
|-----------|------------|---------|-------------|------|-------|
| Y1 | FA-128 48 MHz | 3215-4 pin | **[CHECK STOCK]** | Extended | No confirmed LCSC C-number found for 48 MHz variant. C187794 is the 32 MHz Q22FA12800025. Search LCSC for "Q22FA12800150" or "FA-128 48M" directly; RS Components lists it as a 48 MHz SMT part (RS 2053436) as a cross-reference |
| Y2 | FC-135 32.768 kHz | 3215-2 pin | **C32346** | Extended | Epson Q13FC13500004, 12.5 pF load, 20 ppm; matches C12/C13 12 pF load caps. Alternative C48615 (Q13FC13500002, 7 pF) requires different load caps |

### Connector

| Reference | Value / MPN | Package | LCSC Part # | Tier | Notes |
|-----------|------------|---------|-------------|------|-------|
| J1 | FH12-10S-0.5SH(55) | FPC horizontal 0.5 mm, 10P | **C506791** | Extended | Hirose genuine (HRS). Stock was 38 units at time of search — order early or use FH12A-10S-0.5SH(55) (C5139870) as mechanical substitute; confirm FPC cable compatibility |

### Passives — Generic 0402 / 0603

All standard-value 0402 resistors and capacitors (100 nF, 1 µF, 10 µF, 22 µF, 100 kΩ, 10 kΩ, 3.3 kΩ, 2.4 kΩ, 1 kΩ, 100 Ω, 200 Ω, 470 Ω, 0 Ω, 12 pF, 10 nF, 22 nF, 470 nF) and 0603 capacitors (22 µF, 10 µF) — assign from JLCPCB Basic Parts library during BOM upload. No individual C-numbers required; JLCPCB provides equivalent-spec substitutions from their Basic stock at zero surcharge.

**Specific non-trivial values to note:**
- **C19/C20/C21/C22**: Marked BENCH-TUNE / DNP — do not populate at assembly; install after Qi coil measurement in lab
- **C14/C15**: DNP (RF pi-match shunt, BENCH-TUNE) — leave unpopulated
- **R5 3.92 kΩ** and **R6 200 Ω**: non-E24 values — verify availability in Basic library or specify Extended 0402 equivalents (3.9 kΩ and 200 Ω E24 are acceptable within 2 % per BQ51050B design guide tolerance)

---

## Part 3 — Remaining Work

### Done (schematic phase complete)

- KiCad schematic generated from `netlist.py` — all INSTANCES translated to symbols and wires
- ERC: 0 errors, 0 warnings (excluding expected PWR\_FLAG nodes per `PWR_FLAGS` list)
- Custom symbols created: CC2652R7RGZ, LIS2DW12, FS8205A, LFB182G45BG5D920, ST25DV04K-IER8C3
- Custom footprints created: VQFN-20 (BQ51050B), VSSOP-10 (DRV2605L), LFB18-1608 (balun), crystal 3215-4 pin, battery pads, IFA antenna placeholder
- BOM exported: `Hardware/v2/output/f91_kepler_v2_bom.csv`
- Pin-by-pin verification complete (this document)
- LCSC part candidates identified for all components except CC2652R7RGZR 48 MHz crystal

### Remains for Fabrication

**1. PCB layout in KiCad GUI**

- Run "Update PCB from Schematic" (F8) to push all components and ratsnest
- Placement priorities per `docs/phase2/pcb_v2_fab_package.md §5`:
  - RF section: FL1 balun + AE1 antenna away from ground pour; 50 Ω coplanar waveguide from MCU RF pins to balun
  - BQ51050B coil pads (TP9/TP10) and resonant cap pads (C19–C22) on dedicated RECT island, away from MCU
  - NFC antenna loop on PCB perimeter (U7 AC0/AC1 pads to copper loop via TP15/TP16)
  - DCDC\_SW → L2 → VDDR: short, low-inductance loop; keep C7 22 µF adjacent to this loop
  - C5 22 µF and one 100 nF cap physically adjacent to CC2652R7 pin 34 (VDDS\_DCDC)
  - DRV2605L: 1 µF cap within 2 mm of VDD pin 10

- Complete DRC to 0 errors with JLCPCB 4-layer design rules
- Export Gerbers: F.Cu, B.Cu, In1.Cu, In2.Cu, F.Mask, B.Mask, F.Silkscreen, Edge.Cuts + drill file

**2. Resolve open FLAG items before Gerbers**

- FLAG-1: Confirm FS8205A gate pin (pin 3 vs pin 4) against Fortune FS8205A-DS-12_EN PDF
- FLAG-2: Verify ST25DV04K KiCad symbol pin 9 is passive/no-connect and not wired in the SOIC-8 footprint

**3. LCSC BOM completion**

- Find confirmed LCSC C-number for CC2652R7RGZR or confirm C3606870 (CC2652R74T0RGZR) is a drop-in
- Find confirmed LCSC C-number for FA-128 48 MHz (search "Q22FA12800150" on lcsc.com)
- Resolve FLAG-1 before finalizing FS8205A part number (C16052 vs C14212)

**4. BENCH-TUNE items (post-fab, pre-production)**

- **Qi resonant caps (C19–C22):** Measure actual Qi receive coil Ls' with LCR meter at 100 kHz. Calculate C1 = 1/((2π × 100 kHz)² × Ls') per WPC equation. Populate C19 (primary series), add C20 in parallel if fine-tuning needed. C21 adds cross-coil parallel capacitance. Populate DNP pads after measurement. Note: a PCB-spiral coil is unlikely to achieve Q > 77 — an external wirewound coil is recommended.
- **RF pi-match (C14/C15):** Measure antenna S11 with VNA on first assembled board. Populate C14 and/or C15 shunt caps to match 50 Ω if IFA geometry needs trimming. R2 (0 Ω jumper) can be replaced with a series inductor if needed.

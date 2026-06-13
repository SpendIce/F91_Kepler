# PCB v2 — Reference circuits and pin data

Status: gathered 2026-06-12 from manufacturer datasheets + cited proven designs.
Sections marked **[BENCH-TUNE]** require lab verification before final BOM lock.
Sections marked **[UNVERIFIED]** could not be confirmed from an authoritative primary source during this session.

---

## CC2652R7 (VQFN-48 RGZ) — TI SWRS253B

### Pin map

| Pin | Name | Notes |
|-----|------|-------|
| 1 | RF_P | |
| 2 | RF_N | |
| 3 | X32K_Q1 | |
| 4 | X32K_Q2 | |
| 5 | DIO_0 | |
| 6 | DIO_1 | |
| 7 | DIO_2 | **BSL UART0 RX (fixed)** |
| 8 | DIO_3 | **BSL UART0 TX (fixed)** |
| 9 | DIO_4 | |
| 10 | DIO_5 | high-drive |
| 11 | DIO_6 | high-drive |
| 12 | DIO_7 | high-drive |
| 13 | VDDS2 | 100nF |
| 14 | DIO_8 | |
| 15 | DIO_9 | |
| 16 | DIO_10 | |
| 17 | DIO_11 | |
| 18 | DIO_12 | |
| 19 | DIO_13 | |
| 20 | DIO_14 | |
| 21 | DIO_15 | |
| 22 | VDDS3 | 100nF |
| 23 | DCOUPL | 1µF to GND |
| 24 | JTAG_TMSC | high-drive |
| 25 | JTAG_TCKC | |
| 26 | DIO_16 | JTAG_TDO, high-drive |
| 27 | DIO_17 | JTAG_TDI, high-drive |
| 28 | DIO_18 | |
| 29 | DIO_19 | |
| 30 | DIO_20 | |
| 31 | DIO_21 | |
| 32 | DIO_22 | |
| 33 | DCDC_SW | → 6.8µH inductor → VDDR |
| 34 | VDDS_DCDC | 22µF + 100nF |
| 35 | RESET_N | no internal pullup — 100k external + 100nF |
| 36 | DIO_23 | analog-capable |
| 37 | DIO_24 | analog-capable |
| 38 | DIO_25 | analog-capable |
| 39 | DIO_26 | analog-capable |
| 40 | DIO_27 | analog-capable |
| 41 | DIO_28 | analog-capable |
| 42 | DIO_29 | analog-capable |
| 43 | DIO_30 | analog-capable |
| 44 | VDDS | 100nF |
| 45 | VDDR | 100nF |
| 46 | X48M_N | |
| 47 | X48M_P | |
| 48 | VDDR_RF | 100nF |
| EP | GND | thermal pad — **only GND connection** |

### ROM BSL (SWRA640 Table 3-3)
- UART0 RX = DIO_2 (pin 7) — pogo/UART programming pads MUST connect here
- UART0 TX = DIO_3 (pin 8)

### Support circuit (Electrolama zzh rev A — mirrors LAUNCHXL-CC26X2R1 / TIDA-010012)

**RF front-end:**
- RF_P/RF_N → Murata LFB182G45BG5D920 (integrated balun + filter) → pi-match network (series 0Ω, two shunt DNP pads) → antenna **[BENCH-TUNE pi-match]**

**Power (VDDS domain):**
- Ferrite bead BLM18HE152SN1 from VCC to VDDS net
- 100nF at each VDDS pin (13, 22, 44)
- 22µF + 100nF at VDDS_DCDC (pin 34)

**DC-DC converter (VDDR):**
- DCDC_SW (pin 33) → 6.8µH TDK MLZ2012N6R8LT000 → VDDR
- 22µF low-inductance GND near pin 33
- 100nF at VDDR (pin 45)
- 100nF at VDDR_RF (pin 48)

**Other:**
- DCOUPL (pin 23): 1µF to GND
- RESET_N (pin 35): 100k pullup + 100nF to GND
- EP: direct GND pour
- X32K: Epson FC-135 32.768 kHz + 2×12pF load caps
- X48M: Epson Q22FA12800150 (FA-128 48 MHz), internal load caps used
- BSL trigger: button to GND on backdoor DIO with 100k pullup

**Source:** Electrolama zzh schematic rev A (open hardware); cross-referenced LAUNCHXL-CC26X2R1 reference design and TIDA-010012.

---

## BQ51050B (VQFN-20 RHL) — TI SLUSB42F — Qi v1.2 RX + integrated 4.2V LiPo charger

### Pin map

| Pin | Name | Notes |
|-----|------|-------|
| 1 | PGND | |
| 2 | AC1 | coil input |
| 3 | BOOT1 | 10nF → AC1 |
| 4 | BAT | battery+ output; 1µF + 100nF to PGND |
| 5 | CLAMP1 | 0.47µF (25V) → AC1 |
| 6 | COMM1 | 22nF (25V) → AC1 |
| 7 | /CHG | open-drain charge indicator → MCU DIO + 100k pullup to VDDS |
| 8 | /AD-EN | float |
| 9 | AD | → PGND |
| 10 | TERM | 2.4kΩ → PGND (10% termination) |
| 11 | EN2 | → GND |
| 12 | ILIM | R1 3.92kΩ + R_FOD 200Ω → PGND; FOD taps junction |
| 13 | TS/CTRL | 10kΩ → PGND (no NTC fitted) |
| 14 | FOD | taps ILIM divider junction |
| 15 | COMM2 | 22nF (25V) → AC2 |
| 16 | CLAMP2 | 0.47µF (25V) → AC2 |
| 17 | BOOT2 | 10nF → AC2 |
| 18 | RECT | 2×10µF + 100nF (25V) → PGND |
| 19 | AC2 | coil input |
| 20 | PGND | |
| EP | PGND | exposed pad |

### Key design notes
- I_BULK ≈ 314 / R_ILIM_total → ~76mA for 150mAh cell (R_total = 3.92k + 200Ω ≈ 4.12kΩ)
- Resonant tank: series C1 (coil–AC1) + parallel C2 (AC1–AC2); sized from measured coil Ls' using WPC equation C1 = 1/((2π·100kHz)²·Ls'); Q > 77 required (25V caps) **[BENCH-TUNE]**
- PCB-spiral coil without ferrite will likely fail Q > 77 — schematic pads support external wirewound Qi coil **[BENCH-TUNE]**
- Replaces BQ51013B from older docs. BQ51013B has NO integrated charger (5V output only) — would require extra charger IC.

**Source:** TI SLUSB42F datasheet (BQ51050B); TI SLUB041 application note (BQ51050 design guide).

---

## LIS2DW12 (LGA-12L) — ST MEMS 3-axis accelerometer

**Datasheet:** DocID029682 Rev 4, September 2017 (ST production data)

### Pin map — verified from Table 2 / Figure 2, page 9/63

| Pin | Name | Function |
|-----|------|----------|
| 1 | SCL / SPC | I2C serial clock / SPI serial port clock |
| 2 | CS | I2C/SPI mode select — tie HIGH (to VDD_IO) for I2C; internally pulled up |
| 3 | SDO / SA0 | SPI data out / I2C address LSB: GND → 0x18, VDD_IO → 0x19; internally pulled up |
| 4 | SDA / SDI / SDO | I2C serial data / SPI data in / 3-wire SDO |
| 5 | NC | Not connected — may be tied to VDD, VDD_IO, or GND |
| 6 | GND | 0V supply |
| 7 | RES | Reserved — connect to GND |
| 8 | GND | 0V supply |
| 9 | VDD | Power supply (1.62V–3.6V) |
| 10 | VDD_IO | I/O power supply (independent) |
| 11 | INT2 | Interrupt 2 / single-conversion-on-demand clock input |
| 12 | INT1 | Interrupt 1 |

### I2C mode strapping
- CS (pin 2): tie to VDD_IO — has internal pullup (~20kΩ at 3.6V, ~54kΩ at 1.7V); explicit tie preferred for noise immunity
- SDO/SA0 (pin 3): GND → I2C address 0x18 (default); VDD_IO → 0x19

### Decoupling
- VDD (pin 9): 100nF ceramic + 10µF bulk, placed close to pin
- VDD_IO (pin 10): 100nF ceramic

### Internal pullup values (Table 3 — CS and SDO/SA0 pins)

| VDD_IO | Pullup (typ.) |
|--------|--------------|
| 1.7V | 54.4kΩ |
| 1.8V | 49.2kΩ |
| 2.5V | 30.4kΩ |
| 3.6V | 20.4kΩ |

**Source:** ST DocID029682 Rev 4 (LIS2DW12 datasheet), pages 9–10.

---

## ST25DV04K (SO8N / TSSOP8 / UFDFPN8) — ST dynamic NFC/RFID tag + I2C EEPROM

**Datasheet:** DS10925 Rev 9, February 2021 (ST production data, 200 pages)

### Pin map — SO8 / TSSOP8 / UFDFPN8 — VERIFIED from Figure 2, page 4/200

> **Correction vs. prior assumption:** pin 1 = V_EH (not AC0). AC0 is pin 2, AC1 is pin 3.

| Pin | Name | Notes |
|-----|------|-------|
| 1 | V_EH | Energy harvesting analog output (High-Z when EH disabled or field too weak) |
| 2 | AC0 | Antenna coil — connect exclusively to external NFC coil |
| 3 | AC1 | Antenna coil — connect exclusively to external NFC coil |
| 4 | VSS | Ground |
| 5 | SDA | I2C serial data — open-drain, pullup to VCC required |
| 6 | SCL | I2C serial clock — pullup to VCC required |
| 7 | GPO | Interrupt output — **open-drain** on -IE suffix; requires external pullup > 4.7kΩ to VCC |
| 8 | VCC | Supply voltage (1.8V–5.5V) |

Note: UFDFPN8 package adds an exposed pad (EP) that must be left floating.

### GPO type confirmation
- SO8N and TSSOP8 always use the **-IE** suffix → GPO is open-drain
- "ST25DVxx-IE offers a GPO open drain. This GPO pin must be connected to an external pull-up resistor (> 4.7kΩ) to operate." (DS10925 §2.4.2)
- -JF suffix = CMOS GPO (requires VDCG pin, 10/12-pin packages only — not applicable here)

### I2C notes
- SCL: bus master must use open-drain output; pullup from SCL to VCC required
- SDA: open-drain; pullup from SDA to VCC required
- I2C device select byte: user memory E2=0; system config E2=1

### Decoupling
- VCC: 100nF ceramic to VSS (place close to pin 8) **[UNVERIFIED exact value — datasheet section not read; 100nF is standard ST recommendation]**

**Source:** ST DS10925 Rev 9 (ST25DV04K/16K/64K datasheet), pages 3–7.

---

## FS8205A (TSSOP-8) — Fortune Semiconductor dual N-channel MOSFET for LiPo protection

### Pin map — **[UNVERIFIED — sourced from secondary aggregator sites, not primary datasheet PDF]**

| Pin | Name | Notes |
|-----|------|-------|
| 1 | D1 | Drain of FET 1 |
| 2 | S1 | Source of FET 1 |
| 3 | G1 | Gate of FET 1 (driven by DW01A OD pin) |
| 4 | D (shared) | Shared drain / substrate node |
| 5 | G2 | Gate of FET 2 (driven by DW01A OC pin) |
| 6 | S2 | Source of FET 2 |
| 7 | S2 | Source of FET 2 (doubled for current capacity) |
| 8 | D | Drain (common, internally connected to pin 4) |

Specs: V_DSS = 20V, I_D = 6A, R_DS(on) ≈ 23mΩ (typ) at V_GS = 4.5V.

**Source:** Aggregated from datasheetcafe.com and alltransistors.com secondary listings. Verify against Fortune Semiconductor or CanSheng Industry primary PDF before PCB tapeout.

---

## DW01A + FS8205A — single-cell LiPo protection circuit

### Topology — verified from multiple open-source schematics (Arduino Forum, GitHub Cyclone-92)

```
B+ ──────────────────────────────── P+ (load/charger positive)
     │
   DW01A
   VCC ← 100Ω–470Ω from B+, 100nF to B-
   GND → B-
   CS  → B- (or junction between FS8205A S1/S2 and B-)  [see note]
   OD  → G1 (FS8205A discharge FET gate)
   OC  → G2 (FS8205A charge FET gate)

B- ─── [S1 FET1 D1]──[D shared]──[D2 FET2 S2] ─── P- (load/charger negative)
             ↑                        ↑
            G1 (OD)               G2 (OC)
```

FETs are back-to-back (source-to-source or drain-sharing) in the battery negative return path between B- and P-.

### CS pin resistor — **[UNVERIFIED exact value]**
Most published DW01A application circuits show CS connected directly to the FS8205A source junction (0Ω sense) — the DW01A uses an internal threshold (~150mV across CS→GND) for overcurrent detection. Some designs add a small shunt (10–30mΩ) for precision, but the standard module design uses 0Ω. Verify against DW01A datasheet §overcurrent detection before finalizing.

### Protection thresholds (DW01A internal)
- Overcharge cutoff: 4.28V (typ); release: 4.08V
- Over-discharge cutoff: 2.40V (typ); release: 3.00V
- Overcurrent: ~150mV across CS pin; release: load removal

**Source:** components101.com DW01A overview; Cyclone-92 GitHub KiCad schematic (TP4056-LiPo-charger-with-DW01A-and-FS8205A-MOSFET-protection); Arduino Forum thread (forum.arduino.cc/t/tp4056-dw01a-fs8205a-lipo-circuit/1396223).

---

## Sharp LS013B7DH03 — 128×128 reflective Memory LCD, 10-pin FPC

**Datasheet:** Sharp SPEC No. LCP-1112045, Sep. 6 2012 (27 pages)

### Pin map — VERIFIED from Table 4, Section 4, page 8/27

Candidate order from brief (SCLK, SI, SCS, EXTCOMIN, DISP, VDDA, VDD, EXTMODE, VSS, VSSA) is **confirmed correct**.

| Pin | Code | Voltage | Signal name | Notes |
|-----|------|---------|-------------|-------|
| 1 | SCLK | 0/3.0V | Serial clock | |
| 2 | SI | 0/3.0V | Serial data input | |
| 3 | SCS | 0/3.0V | Chip select | active HIGH |
| 4 | EXTCOMIN | 0/3.0V | COM inversion polarity input | used when EXTMODE=H |
| 5 | DISP | 0/3.0V | Display ON/OFF | H=on, L=all-white (memory kept) |
| 6 | VDDA | 3.0V | Analog power supply | |
| 7 | VDD | 3.0V | Logic power supply | |
| 8 | EXTMODE | 0/3.0V | COM inversion mode select | H=EXTCOMIN HW mode; L=serial SW mode |
| 9 | VSS | 0V | Logic ground | |
| 10 | VSSA | 0V | Analogue ground | |

### EXTMODE strapping
- **EXTMODE = H** (tie to VDD): COM inversion driven by hardware toggle on EXTCOMIN pin (MCU PWM or timer). Recommended for lowest power — no SPI overhead for VCOM.
- **EXTMODE = L** (tie to VSS): COM inversion via serial command embedded in SPI data stream (SI flag). EXTCOMIN not used in this mode.

### Mechanical
- Active area: 23.04mm × 23.04mm (128×128 dots, 0.18mm pitch)
- Module outline: 26.6(W) × 30.3(H) × 0.741(D) mm, mass 1.2g
- FPC connector (applicable connector referenced on Sharp datasheet page 21)

### Power supply note
- Page 5 of datasheet: "As power supply (VDD-GND, VDDA-GND) impedance is lowered during use, bus controller should be inserted near LCD module as much as possible." — add 100nF + 10µF close to FPC pads.

**Source:** Sharp LCP-1112045 specification sheet (Sep. 2012), pages 7–9.

---

## DRV2605L (VSSOP-10 DGS) — TI SLOS854D — haptic driver for ERM/LRA

**Datasheet:** SLOS854D, May 2014, revised March 2018

### Pin map — VERIFIED from Section 5 / Table, pages 4–5

| Pin | Name | Type | Notes |
|-----|------|------|-------|
| 1 | REG | O | 1.8V internal regulator output — **1µF cap required** |
| 2 | SCL | I | I2C clock |
| 3 | SDA | I/O | I2C data |
| 4 | IN/TRIG | I | Multi-mode: PWM / analog / trigger — **tie to GND if unused** |
| 5 | EN | I | Device enable (active HIGH) |
| 6 | VDD/NC | P | Optional supply input — tie to VDD or leave floating |
| 7 | OUT+ | O | Positive haptic driver differential output → ERM+ |
| 8 | GND | P | Supply ground |
| 9 | OUT− | O | Negative haptic driver differential output → ERM− |
| 10 | VDD | P | Supply (2.0V–5.2V) — **1µF cap required** |

### I2C address
- Fixed 7-bit address: **0x5A** (not configurable)

### Typical application
- VDD (pin 10): 1µF to GND (required per datasheet)
- REG (pin 1): 1µF to GND (required per datasheet; also used for internal audio-to-vibe AC coupling — value changed from 0.1µF to 1µF in Rev D)
- ERM motor: OUT+ and OUT− to motor coil directly (differential, ~8–14Ω typical ERM)
- LRA: same OUT+/OUT− pins; enable Smart Loop for automatic resonance tracking
- IN/TRIG: if unused, connect to GND (per Rev B change note)

**Source:** TI SLOS854D (DRV2605L datasheet), pages 1 and 4–5; verified from screenshot of production PDF.

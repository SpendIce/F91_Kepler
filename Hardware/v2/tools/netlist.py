"""F91 Kepler v2 netlist — single source of truth for the generated schematic.

Pin-number→net tables verified against:
  CC2652R7  TI SWRS253B Table 6-1 (RGZ)
  BQ51050B  TI SLUSB42F (VQFN-20 RHL)
  LIS2DW12  ST DocID029682 (LGA-12)
  ST25DV04K ST DS10925 (SO-8)
  DRV2605L  TI SLOS854 (VSSOP-10, KiCad stock symbol pin names)
  FS8205A   Fortune FS8205A-DS-12_EN (TSSOP-8)
  LFB182G45BG5D920 pad map from Electrolama zzh rev A (pad1 UNBAL,
            pad3 RF_N, pad4 RF_P, pads 2/5/6 GND)

DIO plan (docs/phase2/pcb_v2_fab_package.md §4 + BSL + haptic-EN additions):
  DIO_0 BTN_1 (also BSL backdoor) ; DIO_5 SDA ; DIO_6 SCL
  DIO_7 ACC_INT1 ; DIO_9 UART_RX ; DIO_10 UART_TX ; DIO_11 ACC_INT2
  DIO_14 NFC_GPO ; DIO_20 BTN_3 ; DIO_21 CHG_DET ; DIO_22 BUZZER
  DIO_24 HAPTIC_EN ; DIO_25 BTN_2 ; DIO_26 LCD_SCLK ; DIO_27 LCD_MOSI
  DIO_28 LCD_CS ; DIO_29 LCD_EXTCOMIN ; DIO_30 LCD_DISP ; rest NC

Power: VBAT (cell + / BQ BAT out / LDO in) -> XC6206-3.0 -> +3V0 rail.
Battery protection in the negative path: BATN -- FS8205A -- GND, DW01A
referenced to BATN.  [BENCH-TUNE]: Qi resonant C19-C22, RF pi-match C14/C15.
"""

PROJECT_NAME = "f91_kepler_v2"
PAPER = "A2"

C = "Device:C"
R = "Device:R"
F_C0402 = "Capacitor_SMD:C_0402_1005Metric"
F_C0603 = "Capacitor_SMD:C_0603_1608Metric"
F_R0402 = "Resistor_SMD:R_0402_1005Metric"
F_TP = "TestPoint:TestPoint_Pad_1.5x1.5mm"
TP = "Connector:TestPoint"

# (ref, lib_id, value, footprint, (x, y), {pin: net})
INSTANCES = [
    # =================== MCU =====================================
    ("U1", "f91:CC2652R7RGZ", "CC2652R7RGZ", "Package_DFN_QFN:QFN-48-1EP_7x7mm_P0.5mm_EP5.15x5.15mm", (120, 120), {
        "44": "VDDS", "13": "VDDS", "22": "VDDS", "34": "VDDS",
        "45": "VDDR", "48": "VDDR", "33": "DCDC_SW", "23": "DCOUPL",
        "35": "nRESET", "24": "SWD_TMS", "25": "SWD_TCK",
        "47": "X48M_P", "46": "X48M_N", "3": "X32K_1", "4": "X32K_2",
        "1": "RF_P", "2": "RF_N", "49": "GND",
        # DIOs (package pin -> net)
        "5": "BTN_1", "6": "NC", "7": "NC", "8": "NC", "9": "NC",
        "10": "SDA", "11": "SCL", "12": "ACC_INT1", "14": "NC",
        "15": "UART_RX", "16": "UART_TX", "17": "ACC_INT2", "18": "NC",
        "19": "NC", "20": "NFC_GPO", "21": "NC",
        "26": "NC", "27": "NC", "28": "NC", "29": "NC", "30": "BTN_3",
        "31": "CHG_DET", "32": "BUZZER", "36": "NC", "37": "HAPTIC_EN",
        "38": "BTN_2", "39": "LCD_SCLK", "40": "LCD_MOSI", "41": "LCD_CS",
        "42": "LCD_EXTCOMIN", "43": "LCD_DISP",
    }),
    # supply: +3V0 -> ferrite -> VDDS
    ("FB1", "Device:FerriteBead", "BLM18HE152SN1", "Inductor_SMD:L_0603_1608Metric", (40, 40), {"1": "+3V0", "2": "VDDS"}),
    ("C2", C, "100nF", F_C0402, (55, 40), {"1": "VDDS", "2": "GND"}),
    ("C3", C, "100nF", F_C0402, (65, 40), {"1": "VDDS", "2": "GND"}),
    ("C4", C, "100nF", F_C0402, (75, 40), {"1": "VDDS", "2": "GND"}),
    ("C5", C, "22uF", F_C0603, (85, 40), {"1": "VDDS", "2": "GND"}),
    ("C6", C, "100nF", F_C0402, (95, 40), {"1": "VDDS", "2": "GND"}),
    # VDDR from internal DCDC
    ("L2", "Device:L", "6.8uH MLZ2012N6R8LT000", "Inductor_SMD:L_0805_2012Metric", (40, 55), {"1": "DCDC_SW", "2": "VDDR"}),
    ("C7", C, "22uF", F_C0603, (55, 55), {"1": "VDDR", "2": "GND"}),
    ("C8", C, "100nF", F_C0402, (65, 55), {"1": "VDDR", "2": "GND"}),
    ("C9", C, "100nF", F_C0402, (75, 55), {"1": "VDDR", "2": "GND"}),
    ("C10", C, "1uF", F_C0402, (85, 55), {"1": "DCOUPL", "2": "GND"}),
    # reset
    ("R1", R, "100k", F_R0402, (40, 70), {"1": "nRESET", "2": "VDDS"}),
    ("C11", C, "100nF", F_C0402, (55, 70), {"1": "nRESET", "2": "GND"}),
    # crystals
    ("Y1", "Device:Crystal_GND24", "48MHz FA-128", "f91_footprints:Epson_FA-128_2.0x1.6mm", (40, 85), {"1": "X48M_N", "3": "X48M_P", "2": "GND", "4": "GND"}),
    ("Y2", "Device:Crystal", "32.768kHz FC-135", "Crystal:Crystal_SMD_3215-2Pin_3.2x1.5mm", (60, 85), {"1": "X32K_1", "2": "X32K_2"}),
    ("C12", C, "12pF", F_C0402, (75, 85), {"1": "X32K_1", "2": "GND"}),
    ("C13", C, "12pF", F_C0402, (85, 85), {"1": "X32K_2", "2": "GND"}),
    # RF chain: balun -> series jumper -> antenna, pi-match shunt pads DNP
    ("FL1", "f91:LFB182G45BG5D920", "LFB182G45BG5D920", "f91_footprints:Murata_LFB18_1608", (40, 100), {"1": "RF_UNBAL", "4": "RF_P", "3": "RF_N", "2": "GND", "5": "GND", "6": "GND"}),
    ("R2", R, "0R", F_R0402, (60, 100), {"1": "RF_UNBAL", "2": "ANT_FEED"}),
    ("C14", C, "DNP", F_C0402, (70, 100), {"1": "RF_UNBAL", "2": "GND"}),
    ("C15", C, "DNP", F_C0402, (80, 100), {"1": "ANT_FEED", "2": "GND"}),
    ("AE1", "Device:Antenna", "2450AT18A100", "f91_footprints:ANT_2450AT18A100", (95, 100), {"1": "ANT_FEED"}),
    # buttons (active high to +3V0, internal pulldowns in MCU)
    ("SW1", "Switch:SW_Push", "BTN1/BSL", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", (40, 130), {"1": "BTN_1", "2": "+3V0"}),
    ("SW2", "Switch:SW_Push", "BTN2", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", (55, 130), {"1": "BTN_2", "2": "+3V0"}),
    ("SW3", "Switch:SW_Push", "BTN3", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", (70, 130), {"1": "BTN_3", "2": "+3V0"}),
    # pogo pads (programming/debug)
    ("TP1", TP, "POGO_VCC", F_TP, (40, 145), {"1": "+3V0"}),
    ("TP2", TP, "POGO_GND", F_TP, (50, 145), {"1": "GND"}),
    ("TP3", TP, "POGO_RST", F_TP, (60, 145), {"1": "nRESET"}),
    ("TP4", TP, "POGO_TMS", F_TP, (70, 145), {"1": "SWD_TMS"}),
    ("TP5", TP, "POGO_TCK", F_TP, (80, 145), {"1": "SWD_TCK"}),
    ("TP6", TP, "POGO_TX", F_TP, (90, 145), {"1": "UART_TX"}),
    ("TP7", TP, "POGO_RX", F_TP, (100, 145), {"1": "UART_RX"}),
    # buzzer: direct GPIO drive of the case piezo (series R, bleed R)
    ("R3", R, "100R", F_R0402, (40, 160), {"1": "BUZZER", "2": "PIEZO"}),
    ("R4", R, "1k", F_R0402, (50, 160), {"1": "PIEZO", "2": "GND"}),
    ("TP8", TP, "PIEZO_CONTACT", F_TP, (60, 160), {"1": "PIEZO"}),

    # =================== POWER / CHARGING ==========================
    ("U2", "Battery_Management:BQ51050BRHL", "BQ51050B", "f91_footprints:VQFN-20-1EP_4.5x3.5mm_P0.5mm", (220, 60), {
        "1": "GND", "20": "GND", "21": "GND", "2": "AC1", "19": "AC2", "3": "BOOT1", "17": "BOOT2",
        "4": "VBAT", "5": "CLAMP1", "16": "CLAMP2", "6": "COMM1",
        "15": "COMM2", "7": "CHG_DET", "8": "NC", "9": "GND",
        "10": "TERM", "11": "GND", "12": "ILIM", "13": "TS",
        "14": "FOD_TAP", "18": "RECT",
    }),
    # Qi coil + resonant caps [BENCH-TUNE]
    ("TP9", TP, "COIL1", F_TP, (180, 30), {"1": "COIL1"}),
    ("TP10", TP, "COIL2", F_TP, (190, 30), {"1": "COIL2"}),
    ("C19", C, "BENCH-C1a", F_C0603, (200, 30), {"1": "COIL1", "2": "AC1"}),
    ("C20", C, "DNP-C1b", F_C0603, (210, 30), {"1": "COIL1", "2": "AC1"}),
    ("C21", C, "BENCH-C2a", F_C0402, (220, 30), {"1": "AC1", "2": "AC2"}),
    ("C22", C, "DNP-C2b", F_C0402, (230, 30), {"1": "AC1", "2": "AC2"}),
    ("W1", "Device:R", "0R-COIL2", F_R0402, (240, 30), {"1": "COIL2", "2": "AC2"}),
    # BQ support caps
    ("C23", C, "10nF", F_C0402, (180, 90), {"1": "BOOT1", "2": "AC1"}),
    ("C24", C, "10nF", F_C0402, (190, 90), {"1": "BOOT2", "2": "AC2"}),
    ("C25", C, "470nF", F_C0402, (200, 90), {"1": "CLAMP1", "2": "AC1"}),
    ("C26", C, "470nF", F_C0402, (210, 90), {"1": "CLAMP2", "2": "AC2"}),
    ("C27", C, "22nF", F_C0402, (220, 90), {"1": "COMM1", "2": "AC1"}),
    ("C28", C, "22nF", F_C0402, (230, 90), {"1": "COMM2", "2": "AC2"}),
    ("C16", C, "10uF", F_C0603, (240, 90), {"1": "RECT", "2": "GND"}),
    ("C17", C, "10uF", F_C0603, (250, 90), {"1": "RECT", "2": "GND"}),
    ("C18", C, "100nF", F_C0402, (260, 90), {"1": "RECT", "2": "GND"}),
    ("C29", C, "1uF", F_C0402, (270, 90), {"1": "VBAT", "2": "GND"}),
    ("C30", C, "100nF", F_C0402, (280, 90), {"1": "VBAT", "2": "GND"}),
    # programming resistors
    ("R5", R, "3.92k", F_R0402, (180, 105), {"1": "ILIM", "2": "FOD_TAP"}),
    ("R6", R, "200R", F_R0402, (190, 105), {"1": "FOD_TAP", "2": "GND"}),
    ("R7", R, "2.4k", F_R0402, (200, 105), {"1": "TERM", "2": "GND"}),
    ("R8", R, "10k", F_R0402, (210, 105), {"1": "TS", "2": "GND"}),
    ("R9", R, "100k", F_R0402, (220, 105), {"1": "CHG_DET", "2": "VDDS"}),
    # battery + protection (negative path)
    ("BT1", "Device:Battery_Cell", "LiPo 150mAh", "f91_footprints:BatteryPads", (180, 130), {"1": "VBAT", "2": "BATN"}),
    ("TP11", TP, "CHG+", F_TP, (195, 130), {"1": "VBAT"}),
    ("TP12", TP, "CHG-", F_TP, (205, 130), {"1": "GND"}),
    ("R10", R, "470R", F_R0402, (220, 130), {"1": "VBAT", "2": "DW_VCC"}),
    ("C31", C, "100nF", F_C0402, (230, 130), {"1": "DW_VCC", "2": "BATN"}),
    ("U3", "Battery_Management:DW01A", "DW01A", "Package_TO_SOT_SMD:SOT-23-6", (245, 130), {
        "1": "PROT_OD", "2": "PROT_CS", "3": "PROT_OC", "4": "NC", "5": "DW_VCC", "6": "BATN",
    }),
    ("R11", R, "1k", F_R0402, (260, 130), {"1": "PROT_CS", "2": "GND"}),
    ("Q1", "f91:FS8205A", "FS8205A", "Package_SO:TSSOP-8_3x3mm_P0.65mm", (275, 130), {
        "4": "PROT_OD", "5": "PROT_OC", "2": "BATN", "3": "BATN",
        "6": "GND", "7": "GND", "1": "FET_D", "8": "FET_D",
    }),
    # LDO 3.0V
    ("U4", "f91:XC6206PxxxMR_F91", "XC6206P302MR", "Package_TO_SOT_SMD:SOT-23", (180, 155), {"3": "VBAT", "1": "GND", "2": "+3V0"}),
    ("C32", C, "1uF", F_C0402, (195, 155), {"1": "VBAT", "2": "GND"}),
    ("C33", C, "1uF", F_C0402, (205, 155), {"1": "+3V0", "2": "GND"}),

    # =================== PERIPHERALS ===============================
    ("U5", "f91:LIS2DW12", "LIS2DW12", "Package_LGA:LGA-12_2x2mm_P0.5mm", (220, 190), {
        "9": "+3V0", "10": "+3V0", "2": "+3V0", "3": "GND",
        "4": "SDA", "1": "SCL", "12": "ACC_INT1", "11": "ACC_INT2",
        "5": "NC", "7": "GND", "6": "GND", "8": "GND",
    }),
    ("C34", C, "100nF", F_C0402, (240, 190), {"1": "+3V0", "2": "GND"}),
    ("C35", C, "100nF", F_C0402, (250, 190), {"1": "+3V0", "2": "GND"}),
    ("U6", "Driver:DRV2605LDGS", "DRV2605L", "f91_footprints:VSSOP-10_3x3mm_P0.5mm", (220, 220), {
        "1": "DRV_REG", "2": "SCL", "3": "SDA", "4": "GND",
        "5": "HAPTIC_EN", "6": "+3V0", "10": "+3V0",
        "7": "MOT_P", "9": "MOT_N", "8": "GND",
    }),
    ("C36", C, "1uF", F_C0402, (240, 220), {"1": "DRV_REG", "2": "GND"}),
    ("C37", C, "100nF", F_C0402, (250, 220), {"1": "+3V0", "2": "GND"}),
    ("TP13", TP, "MOTOR+", F_TP, (260, 220), {"1": "MOT_P"}),
    ("TP14", TP, "MOTOR-", F_TP, (270, 220), {"1": "MOT_N"}),
    ("U7", "f91:ST25DV04K_SO8", "ST25DV04K", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", (220, 250), {
        "1": "NC", "2": "NFC_A", "3": "NFC_B", "4": "GND",
        "5": "SDA", "6": "SCL", "7": "NFC_GPO", "8": "+3V0",
    }),
    ("C38", C, "100nF", F_C0402, (240, 250), {"1": "+3V0", "2": "GND"}),
    ("R12", R, "100k", F_R0402, (250, 250), {"1": "NFC_GPO", "2": "+3V0"}),
    ("TP15", TP, "NFC_A", F_TP, (260, 250), {"1": "NFC_A"}),
    ("TP16", TP, "NFC_B", F_TP, (270, 250), {"1": "NFC_B"}),
    # I2C pullups (v1 value)
    ("R13", R, "3.3k", F_R0402, (290, 190), {"1": "SDA", "2": "+3V0"}),
    ("R14", R, "3.3k", F_R0402, (300, 190), {"1": "SCL", "2": "+3V0"}),
    # Sharp memory LCD FPC (LS013B7DH03 pinout, LCP-1112045)
    ("J1", "Connector_Generic:Conn_01x10", "LS013B7DH03 FPC", "Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal", (320, 120), {
        "1": "LCD_SCLK", "2": "LCD_MOSI", "3": "LCD_CS", "4": "LCD_EXTCOMIN",
        "5": "LCD_DISP", "6": "+3V0", "7": "+3V0", "8": "+3V0",
        "9": "GND", "10": "GND",
    }),
    ("C39", C, "100nF", F_C0402, (340, 120), {"1": "+3V0", "2": "GND"}),
    ("C40", C, "1uF", F_C0402, (350, 120), {"1": "+3V0", "2": "GND"}),
]

# Back-side assembly uses project-owned canonical snapshots of the verified
# mirrored geometry.  KiCad's stock-library refresh path cannot safely update
# flipped footprints through pcbnew, and leaving locally modified copies under
# stock FPIDs produces library-mismatch warnings at the manufacturing gate.
BACK_CANONICAL_REFS = {
    "Back_C_0402_1005Metric": {
        "C18", "C21", "C22", "C23", "C24", "C25", "C26", "C27", "C28",
        "C29", "C30", "C31", "C32", "C33", "C36", "C37", "C38",
    },
    "Back_C_0603_1608Metric": {"C16", "C17", "C19", "C20"},
    "Back_R_0402_1005Metric": {
        "R5", "R6", "R7", "R8", "R10", "R11", "R12", "R13", "R14", "W1",
    },
    "Back_TestPoint_Pad_1.5x1.5mm": {
        "TP1", "TP2", "TP3", "TP4", "TP5", "TP6", "TP7", "TP8", "TP9",
        "TP10", "TP11", "TP12", "TP13", "TP14", "TP15", "TP16",
    },
    "Back_SOIC-8_3.9x4.9mm_P1.27mm": {"U7"},
    "Back_TSSOP-8_3x3mm_P0.65mm": {"Q1"},
    "Back_SOT-23": {"U4"},
    "Back_SOT-23-6": {"U3"},
}
BACK_FOOTPRINT_BY_REF = {
    ref: f"f91_footprints:{footprint}"
    for footprint, refs in BACK_CANONICAL_REFS.items()
    for ref in refs
}
PROJECT_CANONICAL_FOOTPRINT_BY_REF = {
    "J1": "f91_footprints:F91_LCD_FPC_Hirose_FH12-10",
    "FL1": "f91_footprints:F91_Murata_LFB18_1608",
    "BT1": "f91_footprints:Back_BatteryPads",
    "U6": "f91_footprints:Back_VSSOP-10_3x3mm_P0.5mm",
    "U2": "f91_footprints:Back_VQFN-20-1EP_4.5x3.5mm_P0.5mm",
}
INSTANCES = [
    (
        ref,
        lib_id,
        value,
        PROJECT_CANONICAL_FOOTPRINT_BY_REF.get(
            ref, BACK_FOOTPRINT_BY_REF.get(ref, footprint)
        ),
        position,
        pin_nets,
    )
    for ref, lib_id, value, footprint, position, pin_nets in INSTANCES
]

# Nets fed only through a passive part (ferrite / series R) or referenced to a
# non-GND node — ERC can't see a driver through passives, so flag them.
#   VDDS  <- +3V0 through ferrite FB1
#   VDDR  <- DCDC_SW through inductor L2
#   DW_VCC<- VBAT through R10
#   BATN   = battery negative reference (DW01A VSS pin sits here, not on GND)
# VBAT is NOT flagged: the BQ51050B BAT pin (power_output) already drives it.
PWR_FLAGS = ["GND", "BATN", "VDDS", "VDDR", "DW_VCC"]

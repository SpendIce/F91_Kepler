# F91 Kepler v2 — CC2652R7 (RGZ48) Pin Map — AUTHORITATIVE

Source of truth for firmware (`kepler/kepler_config.h`, `KEPLER_BOARD_V2`)
and hardware. Generated from `f91_kepler_v2.kicad_pcb` after the 2026-07-02
LCD/BTN pin reassignment (U1 south pads are unroutable on the 2-layer
board: U1–U2 gap; LCD moved to top-edge DIOs facing J1).

| Pad | DIO     | Net           | Function                          |
|-----|---------|---------------|-----------------------------------|
| 1   | —       | RF_P          | RF differential + (to FL1 balun)  |
| 2   | —       | RF_N          | RF differential −                 |
| 3   | —       | X32K_1        | 32.768 kHz crystal (Y2)           |
| 4   | —       | X32K_2        | 32.768 kHz crystal (Y2)           |
| 5   | DIO_0   | BTN_1         | Button 1 (SW1, left edge)         |
| 10  | DIO_5   | SDA           | I2C SDA (U5/U6/U7)                |
| 11  | DIO_6   | SCL           | I2C SCL                           |
| 12  | DIO_7   | ACC_INT1      | LIS2DW12 INT1                     |
| 15  | DIO_9   | UART_RX       | BSL/console RX (pogo pad)         |
| 16  | DIO_10  | UART_TX       | BSL/console TX (pogo pad)         |
| 17  | DIO_11  | ACC_INT2      | LIS2DW12 INT2                     |
| 20  | DIO_14  | NFC_GPO       | ST25DV GPO interrupt              |
| 23  | —       | DCOUPL        | Internal regulator decouple (C10) |
| 24  | —       | SWD_TMS       | Debug (TP4)                       |
| 25  | —       | SWD_TCK       | Debug (TP5)                       |
| 30  | DIO_20  | BTN_3         | Button 3 (SW3, right edge)        |
| 31  | DIO_21  | CHG_DET       | Qi charge detect                  |
| 32  | DIO_22  | BUZZER        | Piezo PWM                         |
| 33  | —       | DCDC_SW       | DC/DC switch node (L2)            |
| 35  | —       | nRESET        | Reset (pull to VDDS via R1)       |
| 37  | DIO_24  | HAPTIC_EN     | DRV2605L enable                   |
| 38  | DIO_25  | BTN_2         | Button 2 (SW2, right edge)        |
| 39  | DIO_26  | LCD_EXTCOMIN  | Sharp LCD VCOM toggle             |
| 40  | DIO_27  | LCD_MOSI      | Sharp LCD SPI MOSI                |
| 41  | DIO_28  | LCD_SCLK      | Sharp LCD SPI SCLK                |
| 42  | DIO_29  | LCD_CS        | Sharp LCD CS — **ACTIVE HIGH**    |
| 43  | DIO_30  | LCD_DISP      | Sharp LCD DISP                    |
| 46  | —       | X48M_N        | 48 MHz crystal (Y1)               |
| 47  | —       | X48M_P        | 48 MHz crystal (Y1)               |

Unused DIOs (spares): DIO_1, DIO_2, DIO_3, DIO_4, DIO_8, DIO_12, DIO_13,
DIO_15–19, DIO_23 (pads 6, 7, 8, 9, 14, 18, 19, 21, 26–29, 36 — all
no-connect in schematic).

## History

- 2026-07-02: LCD_MOSI/SCLK/CS/DISP/EXTCOMIN moved from DIO_9–13 (pads
  15–19, U1 south — trapped by U2) to DIO_27/28/29/30/26 (top edge, face
  J1). BTN_1 DIO_15→DIO_0, BTN_3 DIO_26→DIO_20. `tools/repin_lcd.py`.
- 2026-07-02 (b): UART_RX DIO_2→DIO_9, UART_TX DIO_3→DIO_10, ACC_INT2
  DIO_8→DIO_11 — the freed south pads reach the widened U1–U2 channel;
  the old left-column pads could not all escape (2 west lanes for 5
  nets). `tools/surgery3.py`.

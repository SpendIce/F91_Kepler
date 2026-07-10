# PCB v2 — DFM Readiness Gate

> Estado operativo verificado el 2026-07-10 con KiCad CLI 10.0.4 sobre
> `Hardware/v2/f91_kepler_v2.kicad_pcb` y
> `Hardware/v2/f91_kepler_v2.kicad_sch` actuales.
>
> Este documento no sustituye al Plan Maestro para alcance de producto. Sí
> sustituye los estados de avance obsoletos de los documentos de Fase 2 hasta
> que se cierre este gate.

## Veredicto

**NO FABRICAR.** El paquete Gerber existente es un artefacto de avance, no una
salida liberable para JLCPCB: el PCB actual todavía contiene desconexiones y
errores DRC de cobre/borde y de colocación.

## Evidencia ejecutada

Los comandos siguientes fueron de solo lectura y escribieron los reportes a
`/tmp`, sin modificar el diseño:

```sh
python3 Hardware/v2/tools/check_route.py \
  Hardware/v2/f91_kepler_v2.kicad_pcb check
kicad-cli pcb drc --format json --severity-all --schematic-parity \
  --output /tmp/f91_kepler_v2-current-drc.json \
  Hardware/v2/f91_kepler_v2.kicad_pcb
kicad-cli sch erc --format json --severity-all \
  --output /tmp/f91_kepler_v2-current-erc.json \
  Hardware/v2/f91_kepler_v2.kicad_sch
```

| Comprobación | Resultado | Interpretación |
| --- | ---: | --- |
| Ruta manual `+3V0` | `ALL CLEAR` | El script geométrico específico no ve colisiones ni vías a retirar en esa ruta. No equivale a DRC global. |
| DRC global | 654 violaciones | Bloquea fabricación. Incluye 118 `courtyards_overlap` y 57 `copper_edge_clearance` como errores. |
| Conectividad | 63 ítems sin conectar | Bloquea fabricación. Afecta, entre otras, BATN, VDDS, AC1/AC2, SCL/SDA, SWD, UART, LCD y lazos de carga/NFC. |
| Paridad PCB–esquemático | 1 advertencia | U7/ST25DV04K tiene el pin 9 en el símbolo pero no en el footprint SO-8. Ver `v2_schematic_verification.md` §1. |
| ERC | 2 advertencias | U4 no coincide con la copia de librería y U5 SDO/SA0 queda advertido frente al `PWR_FLAG` de GND. No se deben ocultar sin verificar símbolo y strap físico. |

El DRC anterior en `Hardware/v2/DRC.rpt` (2026-06-27) ya no es el estado del
PCB actual: reportaba 786 violaciones y 195 desconexiones. Hay progreso, pero
el gate sigue rojo.

## Contratos que deben resolverse antes de enrutar más

| Tema | Evidencia actual | Acción requerida |
| --- | --- | --- |
| Stackup | El `.kicad_pcb` declara `F.Cu`, `In1.Cu`, `In2.Cu`, `B.Cu`; `tools/gen_fab.sh` exporta esas cuatro capas. `pcb_v2_fab_package.md` y `v2_pcb_layout_guide.md` aún piden una orden de dos capas. | Elegir y documentar **cuatro capas** o volver el layout a dos antes de continuar. No enviar un Gerber de cuatro capas con una orden de dos. |
| Envolvente mecánica | El contorno generado actual es 25×27 mm; el Plan Maestro y la guía refieren 30×28 mm. | Medir asiento real de junta/caja y fijar un datum mecánico. Revalidar borde, bracket, LiPo, motor y pads pogo contra ese datum. |
| RF y carga/NFC | Hay cobre y vías al borde en ANT_FEED, GND, VBAT, +3V0 y otras redes; existen lazos de carga/NFC previstos. | Tras fijar stackup y contorno, revisar keepouts y retorno RF antes de aceptar excepciones DRC. Ninguna excepción puede justificar cobre fuera del borde. |

## Orden de avance ejecutable

1. **Congelar el contrato de fabricación.** Resolver stackup y datum mecánico;
   actualizar el board setup y documentos para una única verdad.
2. **Restaurar conectividad.** Corregir las 63 desconexiones por red, empezando
   por alimentación/protección (`BATN`, `VDDS`, `+3V0`), luego carga (`AC1`,
   `AC2`, `COIL1`, `COIL2`), y por último MCU/IO (`SCL`, `SDA`, SWD, UART, LCD).
   Reejecutar DRC después de cada grupo.
3. **Eliminar errores físicos.** Llevar a cero `copper_edge_clearance` y
   `courtyards_overlap`; el segundo grupo requiere placement real, no
   exclusiones masivas. Revalidar claramente el entorno de la antena.
4. **Cerrar consistencia esquemático–PCB.** Corregir o documentar mediante
   símbolo/footprint válido el pin 9 de U7, resolver el símbolo de U4 y el strap
   SA0 de U5. ERC y paridad deben quedar sin advertencias no justificadas.
5. **DFM de ensamblaje.** Con DRC/ ERC/ paridad limpios, revisar footprint,
   courtyard, orientación, LCSC, DNP y cara de ensamblaje de cada componente.
6. **Exportar y revisar.** Recién entonces ejecutar `tools/gen_fab.sh`, abrir
   Gerbers/drills en un visor independiente, revisar CPL/BOM y preparar la
   orden de PCB y PCBA coherente con el stackup decidido.
7. **Primer lote y banco.** Fabricar el lote mínimo; antes del sellado medir
   carga inductiva, antena/BLE, consumo, programación por pogo/UART y el encaje
   completo en caja con junta original.

## Criterio de liberación a Gerber

- DRC: 0 errores y 0 desconexiones; advertencias sólo con justificación
  individual revisada.
- ERC y paridad: 0 errores; cada advertencia restante tiene dueño, causa y
  aceptación explícita.
- Stackup, espesor, acabado, contorno y reglas del fabricante coinciden entre
  PCB, script de exportación y orden de compra.
- Gerbers, drill, CPL y BOM provienen del mismo commit del diseño.

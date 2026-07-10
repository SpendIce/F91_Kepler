# F91 Kepler — Plan Maestro Consolidado

> **Documento de planificación única y autoritativo del proyecto.** Reemplaza
> a todos los documentos previos (build plan, `CLAUDE.md` del 23/03 y specs de
> sesión) allí donde haya conflicto. `AGENTS.md` sigue siendo autoritativo solo
> para conducta del agente (herramientas, commits y verificación). Integra la
> totalidad de la revisión técnica (R1–R7) como decisiones firmes, no como
> preguntas abiertas.
>
> Versión: 1.0 — consolidado post-revisión · Fecha: 2026-06-09
> MCU objetivo: TI CC2640R2F · SDK base actual: SimpleLink CC2640R2 SDK
> 4.40.0.10 (metadata CCS del repo) · IDE: CCS

---

## Índice

1. Qué es el proyecto
2. Estado del proyecto original y alcance del fork
3. Decisiones de plataforma (MCU, SDK, toolchain)
4. Restricciones duras y presupuestos (RAM / flash / energía)
5. Cambios de hardware respecto del original, con justificación
6. Modificaciones obligatorias derivadas de la revisión (R1–R7)
7. Arquitectura de firmware
8. Modelo de pantallas y de botones
9. Perfil BLE GATT
10. Almacenamiento no volátil (NV)
11. Plan de fases (0–3)
12. Sesiones de agente (Fase 0)
13. App companion Android (Fase 1)
14. PCB v2 (Fase 2)
15. Sellado y resistencia al agua (Fase 3)
16. Consideraciones de riesgo y mitigaciones
17. Criterios de éxito y checklist de aceptación
18. Decisiones abiertas
19. Referencias

---

## 1. Qué es el proyecto

El F91 Kepler es un fork del proyecto open-source homónimo de **PegorK** (licencia
MIT, repo mirror en `https://github.com/PegorK/F91_Kepler`): una PCB de reemplazo
para el interior de un reloj **Casio F91W**, que conserva solo la caja de resina,
los botones y la junta de goma originales.

Este fork **completa y extiende** el diseño original para convertirlo en un
smartwatch BLE de uso diario práctico, manteniendo el factor de forma y la
estanqueidad del F91W intactos (sin agujeros nuevos, sin cutout USB-C, sin
modificar la junta).

**Capacidades objetivo:**

- Display siempre encendido, legible de un vistazo sin presionar botones.
- Carrusel de seis pantallas: MAIN, WEATHER, NOTIFICATIONS, PHONE LOCATOR, STOPWATCH, ALARMS.
- Clima (temperatura + ícono de condición) en MAIN; detalle y pronóstico horario en WEATHER, empujado desde la app.
- Notificaciones BLE de llamadas y mensajes con nombre del remitente y texto.
- Haptic con patrones de vibración distintos por tipo de notificación.
- Wrist-raise que conmuta a NOTIFICATIONS; BTN_3 cicla pantallas; BTN_1 largo = invertir display (reemplaza la luz).
- Phone locator (hace sonar el teléfono pareado desde el reloj).
- Cronómetro clásico con vueltas (sigue corriendo al navegar fuera).
- Lista de alarmas/timers sincronizada desde el teléfono.
- Conteo de pasos por podómetro de hardware (motor del LIS2DW12).
- Tracking de sueño por actigrafía (detección de movimiento nocturno).
- Tag NFC pasivo para credenciales de acceso, tokens y pairing NDEF.
- Batería LiPo recargable inductivamente — sin agujeros, junta de goma original conservada.
- App companion Android para todo lo anterior.

**Explícitamente rechazado (con razón):**

- **Pagos NFC (Google Wallet / tarjetas bancarias):** requieren un Secure Element certificado y un proceso de certificación cerrado con Visa/Mastercard/Google.
- **Sensor óptico de ritmo cardíaco:** requiere ventana en la tapa trasera; rompe caja y junta.
- **Detección de fases de sueño (REM/profundo/ligero):** requiere PPG/HRV; mismo problema que el HR óptico.
- **Puerto de carga USB-C:** requiere agujero en la caja; rompe la estanqueidad.

---

## 2. Estado del proyecto original y alcance del fork

**Estado en que PegorK abandonó el proyecto (2022):**

- MCU: CC2640R2F (Cortex-M3, BLE 4.2 según firmware original).
- Display: SSD1306 OLED 128×64 px (I2C, requiere backlight, no puede ser always-on, 2–15 mA activo).
- Batería: pila de botón no recargable.
- BLE: anuncia y conecta, pero solo operable vía nRF Connect (app de diagnóstico).
- Un solo botón funcional (wake de pantalla); botones 2 y 3 cableados a GPIO pero sin handlers.
- Sin app companion, sin seteo de hora, sin sonido, sin sensores más allá del MCU.
- ~15 días de autonomía.
- Firmware en C, SDK TI SimpleLink, proyecto CCS.
- Excelente calidad de hardware (PCB, bracket impreso 3D, jig de programación SWD) pero firmware incompleto.

El fork retiene la calidad de hardware base y reescribe el firmware completo,
reemplaza el display, agrega sensores, carga, haptic y NFC, e incorpora una app
companion.

---

## 3. Decisiones de plataforma

### 3.1 MCU — CC2640R2F (RETENIDO, NO ACTUALIZABLE)

| Parámetro | Valor |
|---|---|
| Chip | TI CC2640R2F |
| Core | ARM Cortex-M3, 48 MHz |
| Flash | 128 KB |
| SRAM | 20 KB (+8 KB de cache como SRAM, reservada por el RF core) |
| BLE | Bluetooth 5.1 Low Energy |
| Encapsulado | QFN-32 4×4 mm (variante RSM) — verificado contra la PCB Kepler original |

**Por qué no se actualiza el MCU.** Una revisión de toda la familia TI CC26xx
confirmó que **no existe un upgrade pin-compatible ni package-compatible** para el
QFN-32 de 4×4 mm:

- **CC2640R2L:** misma 128 KB / 20 KB — sin mejora de memoria; solo quita el Sensor Controller.
- **CC2642R / CC2652R / CC2652R7:** 352–704 KB flash, 80–144 KB RAM, pero **solo** en QFN-48 de 7×7 mm — ~3× el área de footprint. La caja del F91W "apenas entra" un 4×4 mm; un 7×7 mm físicamente no entra.
- **CC2651R3 (RKP 5 mm):** pinout distinto; exigiría rediseño completo de PCB sin reutilizar el ruteo v1.

**Conclusión:** el proyecto se queda en CC2640R2F. Todo el diseño de firmware
debe respetar los límites duros de **128 KB flash / 20 KB RAM**.

> **Nota de future-proofing (solo informativa, fuera de alcance v1/v2):** si en el
> futuro se rediseña por completo, el **CC2674R10** (Cortex-M33, 1024 KB flash,
> 256 KB RAM, pin-compatible con el CC2652R7 en 7×7 mm) es el camino de upgrade
> de próxima generación. Implica re-port del stack BLE y re-certificación RF.

### 3.2 SDK y toolchain

- **SDK base actual:** TI SimpleLink CC2640R2 SDK 4.40.0.10, según
  `Firmware/f91_kepler_app/.cproject`.
- **Stack BLE:** usar los componentes BLE incluidos en el SDK CC2640R2. Los
  README originales citan BLEstack 2.2.5 y algunos specs previos hablan de SDK
  v4.x genérico; para este fork manda la metadata CCS hasta que haya una
  decisión explícita de migración.
- **IDE:** Code Composer Studio (CCS) — requerido. El stack BLE no soporta Makefile/CMake.
- **Stack BLE:** librería precompilada, ocupa ~80 KB de flash, no modificable.
- **RTOS:** TI-RTOS (SYS/BIOS), incluido en el SDK.
- **Hardware de desarrollo:** LAUNCHXL-CC2640R2F con Sharp LCD cableado para toda la Fase 0.

---

## 4. Restricciones duras y presupuestos

### 4.1 Presupuesto de RAM (20 KB) — ajustado pero viable [R1]

| Ítem | Tamaño aprox. |
|---|---|
| Framebuffer Sharp LCD (144×168, 1 bpp) | 3024 B |
| Ring buffer de notificaciones (10 × ~70 B) | ~700 B |
| Payload de clima | 36 B |
| Alarmas | 66 B |
| Stacks de tareas TI-RTOS (main + ICall/BLE + display) | ~12–14 KB |
| Heap/estado del stack BLE | 8–10 KB del total |

La presión real no viene de los datos de aplicación (modestos) sino del stack BLE
y de los stacks de tareas de TI-RTOS. **Decisión de diseño:** llevar un conteo
acumulado de RAM **desde el día uno**, leyendo el archivo `.map` del linker de CCS
después de cada sesión. Si quedan menos de 4 KB libres tras la reserva del stack
BLE, el primer recorte es el ring buffer de notificaciones (5 entradas en lugar de
10 ahorra ~350 B y 5 sigue siendo de sobra para un reloj); el buffer de vueltas
del cronómetro es el segundo candidato. Verificar además si las páginas de cache
de **NVOCMP** se espejan en RAM en la configuración elegida (puede robar 1–2 KB).

### 4.2 Presupuesto de flash (128 KB) — la restricción peligrosa [R2]

- El stack BLE precompilado ocupa típicamente **70–85 KB**.
- **OAD dual-image** (el esquema estándar de TI para el CC2640R2F) exige **dos slots de imagen** en flash interna → ~2× el código de aplicación + stack + bootloader OAD (~4 KB). En un dispositivo de 128 KB esto es **prácticamente imposible**.
- Los bitmaps de íconos de clima (~1.5 KB) son irrelevantes frente a este problema estructural.
- **El consumidor real de flash, más allá del stack, son las fuentes (bitmaps), no la lógica de pantallas.** [R7]

**Decisiones firmes:**

1. **OAD se difiere a Fase 2** con **flash SPI externa** (la LaunchPad ya la trae; la PCB v2 debe agregarla si se quiere OAD).
2. **Verificación temprana obligatoria:** al terminar la Sesión 1 (driver de display + UI renderer + fuentes), revisar el `.map`. Si `stack BLE + código + fuentes` supera **~110 KB**, hay riesgo de no entrar en una imagen única y se debe replantear (recortar fuentes o evaluar upgrade de MCU para la v2).
3. La pregunta a responder primero no es "¿entra OAD?" sino "¿entra siquiera en una sola imagen de 128 KB?".

### 4.3 Presupuesto de energía — estimación corregida [R3]

> **La estimación previa de 85–100 días era optimista por ~2–3×.**

Conectado (el estado estable de un smartwatch) a un intervalo de conexión de
500 ms, la radio despierta cada 500 ms a ~6–8 mA durante 3–5 ms, lo que promedia
~50–80 µA solo por mantenimiento de conexión; sumado a la corriente activa del MCU
durante esos despertares, la corriente media realista es **~80–120 µA conectado**.

A 100 µA medios con 150 mAh → **~62 días reales**. Sigue siendo muy bueno, pero no
100.

**Optimizaciones de diseño (obligatorias):**

- Intervalo de conexión **adaptativo**: 1–2 s en idle, conmutar a 100–200 ms solo durante ráfagas de notificación, vía el mecanismo de *connection parameter update request*.
- De noche (p. ej. 3 AM) **no mantener conexión activa**: el teléfono desconecta y el reloj cae a advertising. No hay razón para sostener la conexión cuando no hay nada que entregar.

**Consumidores principales (referencia de diseño, LiPo 150 mAh):**

| Componente | Consumo medio |
|---|---|
| Sharp LCD (imagen estática, always-on) | ~5 µA |
| CC2640R2F deep sleep | ~1 µA |
| CC2640R2F activo (evento BLE) | ~6 mA en ráfagas de ms |
| Advertising a 2 s | ~70 µA |
| LIS2DW12 low-power | ~6 µA |
| DRV2605L standby | ~0.6 µA |
| ST25DV04K | ~0 (pasivo, alimentado por el campo del lector) |
| Motor haptic en vibración | ~80 mA en ráfagas de 100–500 ms |

---

## 5. Cambios de hardware respecto del original

### 5.1 Display: SSD1306 OLED → Sharp LS013B7DH03 Memory LCD

| | Original | Nuevo |
|---|---|---|
| Tecnología | OLED 128×64, I2C | Memory LCD reflectivo 144×168, SPI |
| Consumo | 2–15 mA activo | ~5 µA con imagen estática |
| Always-on | No (requiere backlight) | Sí (reflectivo) |

**Particularidades del Sharp LCD (críticas para el driver):**

- **CS es activo en ALTO** (al revés de la mayoría de los SPI) — anotar bien en el driver.
- **VCOM debe alternar continuamente** mientras el display está alimentado (1–2 Hz vía timer de hardware). Si VCOM se detiene, el panel puede dañarse con el tiempo.
- Comandos de escritura en SPI de 3 hilos: byte de modo + byte de dirección + datos.
- Framebuffer 1 bpp: 144×168 = **3024 bytes**, entra cómodo en RAM.
- Soporta **actualización parcial por líneas** — enviar solo las líneas que cambian para minimizar tráfico SPI.
- SPI ≤ **1 MHz**.

### 5.2 Otros cambios

- **Batería:** pila de botón no recargable → **LiPo 150 mAh + protección DW01A**.
- **Carga:** ninguna útil → **inductiva** con **TI BQ51013B** + bobina de espiral en cobre trasero (4–5 vueltas, ~20 mm de diámetro interno). Dock 3D imprimible alimentado por USB (transmisor tipo TP5100, objetivo ~100 mA de carga lenta segura).
- **Acelerómetro:** ninguno → **LIS2DW12** (I2C, LGA 2×2 mm). `WHO_AM_I` debe devolver `0x44`. Se usa el **motor de podómetro por hardware** (banco de registros B) — sin algoritmo de pasos por software.
- **Haptic:** buzzer básico → **DRV2605L** (I2C, WSON-6) + motor ERM coin de 10 mm en recess del bracket (no en la PCB). Se conserva el buzzer PWM para alarmas sonoras.
- **NFC:** ninguno → **ST25DV04K** (I2C, SO8N) + antena loop de una vuelta en el perímetro de la PCB.
- **Programación:** jig SWD → **pads de pogo pin** expuestos en la cara trasera (5: VCC, GND, SWDIO, SWDCLK, RESET).

### 5.3 Direcciones I2C (7-bit)

| Dispositivo | Dirección |
|---|---|
| DRV2605L | 0x5A |
| LIS2DW12 | 0x18 (SDO/SA0 bajo) |
| ST25DV04K — user memory | 0x53 |
| ST25DV04K — system config | 0x57 |

### 5.4 Stackup de antenas (3 zonas concéntricas en cobre trasero)

1. **Exterior:** loop de antena NFC, una vuelta alrededor del perímetro (~30 mm).
2. **Medio:** bobina de carga inductiva, espiral de 4–5 vueltas (~20 mm interno).
3. **Interior:** relleno de masa + ruteo de componentes.

Práctica estándar (usada en Garmin vívofit y similares). Requiere DRC cuidadoso en KiCad.

---

## 6. Modificaciones obligatorias derivadas de la revisión (R1–R7)

Esta sección consolida lo que cambia respecto del plan previo. Cada punto ya está
reflejado en las secciones técnicas correspondientes; acá quedan listadas como
referencia rápida de "qué hay que tocar".

| # | Tema | Decisión / cambio |
|---|---|---|
| **R1** | RAM | Conteo acumulado desde día uno; leer `.map` por sesión; plan de recorte (notificaciones 10→5, vueltas) si quedan <4 KB; verificar espejado RAM de NVOCMP. |
| **R2** | Flash / OAD | OAD dual-image **descartado** en 128 KB; diferir OAD a Fase 2 con flash SPI externa; **chequeo de `.map` tras Sesión 1**; umbral de alarma ~110 KB. |
| **R3** | Batería / BLE | Estimación corregida a **~62 días**; intervalo de conexión **adaptativo** (1–2 s idle / 100–200 ms en ráfaga); caer a advertising de noche. |
| **R4** | LIS2DW12 | **Single INT1 confirmado** (INT2 no ruteado, no necesario); documentar la **secuencia de reconfiguración de registros** en las transiciones de ventana de sueño (22:00 / 08:00). |
| **R5** | Clima | Default cambia a **Open-Meteo** (sin API key); abstracción `WeatherProvider` en Android; OpenWeatherMap como alternativa. |
| **R6** | Cronómetro | Usar **periférico GPT clockeado desde XOSC 48 MHz**, **no** `Clock_create()` (jitter del RTC); deriva ±40 ppm ≈ 14 centésimas/hora. |
| **R7** | Pantallas | 6 archivos `.c/.h` separados es correcto y **no** es el cuello de botella de flash; las **fuentes** sí lo son. No combinar pantallas. |

### Detalle R4 — secuencia de sueño del LIS2DW12

El enfoque de un único INT1 con chequeo de hora del día en software es la opción
**pragmática y correcta**, no un compromiso: rutear INT2 agregaría GPIO y traza
sin ahorrar lógica de software. Lo que **debe** documentarse explícitamente es la
reconfiguración de registros en los límites de la ventana de sueño:

Para wrist-raise diurno:
```
WAKE_UP_THS (0x34) = 0x10          // ~0.5g umbral (ajustable)
WAKE_UP_DUR (0x35) = 0x20          // mínimo 2 muestras sobre umbral
CTRL4_INT1_PAD_CTRL (0x23): set INT1_WU
CTRL7 (0x3F) = 0x20                // re-habilitar INT1
```
En entrada a ventana de sueño (22:00): conmutar de detección wake-up a umbral de
movimiento/free-fall (o loguear FIFO por timer) y **suprimir** wrist-raise.
En salida (08:00): revertir. Hacerlo mal implica o data de actigrafía perdida o
eventos de wrist-raise fantasma de noche.

### Detalle R6 — fuente de reloj del cronómetro

```c
Timer_Params params;
Timer_Params_init(&params);
params.period        = 10000;                 // 10 ms en µs
params.periodUnits   = Timer_PERIOD_US;
params.timerMode     = Timer_CONTINUOUS_CALLBACK;
params.timerCallback = stopwatch_timer_cb;     // incrementa volatile uint32_t centiseconds
```
El RTC de 32 kHz (wall-clock) y el GPT (intervalos del cronómetro) son **dominios
de reloj independientes**: no hay deriva cruzada. El GPT **debe** clockearse desde
XOSC (no RCOSC; con RCOSC habría ±1–2 % → una sesión de 10 min podría errar 6–12 s).
El timer arranca solo en estado RUNNING y se detiene en pausa; el display se
actualiza solo cuando la pantalla STOPWATCH está activa.

---

## 7. Arquitectura de firmware

### 7.1 Cola de eventos central

Todas las fuentes de interrupción (botones, INT1 del acelerómetro, callbacks BLE,
timers) postean **eventos tipados** a una única cola. La tarea principal del
CC2640R2F consume los eventos y llama al handler correspondiente. **Sin I/O de
periféricos dentro de ISRs.** ~24 tipos de evento cubren todas las pantallas,
sensores e interacciones BLE. Estado de energía `Power_STANDBY` (~1 µA) entre
eventos.

### 7.2 Máquina de estados de energía y loop principal

`kepler_main.c` contiene el event loop con el switch/case de las 6 pantallas. El
patrón es: ISR → postea evento → loop despierta → handler → vuelve a STANDBY.

### 7.3 Guardas de compilación

Cada driver de hardware aún no presente (Fase 0 corre en LaunchPad) se envuelve en
guardas para que compile limpio sin hardware:
`KEPLER_HAS_DRV2605L`, `KEPLER_HAS_LIS2DW12`, etc.

---

## 8. Modelo de pantallas y de botones

### 8.1 Carrusel de pantallas

```
  MAIN ──► WEATHER ──► NOTIFICATIONS ──► PHONE LOCATOR
   ▲                                              │
   └──── ALARMS ◄──── STOPWATCH ◄────────────────┘
```

```c
typedef enum {
    UI_SCREEN_MAIN          = 0,   // hora, fecha, pasos, resumen de clima
    UI_SCREEN_WEATHER       = 1,   // actual + pronóstico horario
    UI_SCREEN_NOTIFICATIONS = 2,   // lista scrolleable de notificaciones
    UI_SCREEN_PHONE_LOCATOR = 3,   // botón "encontrar teléfono"
    UI_SCREEN_STOPWATCH     = 4,   // cronómetro con vueltas
    UI_SCREEN_ALARMS        = 5,   // lista de alarmas/timers sincronizada
    UI_SCREEN_COUNT         = 6,
} ui_screen_t;
```

- **BTN_3 short:** avanza (envuelve ALARMS → MAIN).
- **BTN_3 long:** salta a MAIN desde cualquier pantalla.
- **Wrist-raise:** va a NOTIFICATIONS (suprimido durante ventana de sueño).
- **Timeout de inactividad:** vuelve a MAIN (salvo cronómetro corriendo); reseteable con cualquier botón.

### 8.2 Mapeo físico de botones (posiciones originales del F91W)

```
  [BTN_1]  ARRIBA          — posición LIGHT  → ACCIÓN / INVERTIR DISPLAY
  [BTN_2]  ABAJO-IZQUIERDA — posición SET    → CONTEXTO-SET / TIME-SET
  [BTN_3]  ABAJO-DERECHA   — posición MODE   → CICLAR PANTALLA / HOME
```

El F91W (módulo 593) tiene exactamente tres pulsadores laterales: dos del lado
izquierdo (BTN_1 arriba, BTN_2 abajo) y uno del derecho (BTN_3). **BTN_1 + BTN_3
están en lados opuestos y requieren dos manos** para presionarse simultáneamente
— evitar combos que dependan de eso.

> El mapeo a DIO no pudo confirmarse del código fuente (el repo no está indexado a
> nivel de archivo). El mapeo asumido sigue la convención estándar del SDK
> (numeración secuencial arriba→abajo, izq→der). **Verificar contra el `Board.h`
> real al clonar el repo** antes de la Sesión 2.

### 8.3 Reglas universales (toda pantalla, siempre)

| Botón | Evento | Acción |
|---|---|---|
| BTN_3 | Short | Avanzar a la próxima pantalla |
| BTN_3 | Long | Saltar a MAIN |
| BTN_2 | Long | Entrar a time-set (sale a MAIN al confirmar/timeout) |
| BTN_1 | Long | Invertir display por 3 s (equivalente "dark mode" del Sharp LCD) |

**BTN_1 long = invertir display:** el Sharp es reflectivo y sin backlight. Invertir
(XOR de los bytes con 0xFF → blanco sobre negro) mejora contraste en interiores
tenues a ciertos ángulos. Reemplaza funcionalmente al botón de luz del F91W (misma
posición). Es cosmético y temporal (3 s); el contenido del framebuffer no cambia.

### 8.4 Acciones contextuales por pantalla (BTN_1 short / BTN_2 short)

| Pantalla | BTN_1 short | BTN_2 short |
|---|---|---|
| MAIN | — | — |
| WEATHER | Pedir refresh de clima por BLE | Toggle unidad (°C ↔ °F) |
| NOTIFICATIONS | Descartar la seleccionada | Scroll a la siguiente |
| PHONE LOCATOR | Empezar/parar sonido | — |
| STOPWATCH | Start / Stop | Lap (corriendo) · Reset (detenido) |
| ALARMS | Toggle on/off | Scroll a la siguiente |

### 8.5 Modo time-set (entra por BTN_2 long)

| Botón | Short |
|---|---|
| BTN_1 | Incrementa campo (envuelve al máximo) |
| BTN_2 | Confirma campo → avanza (último confirm escribe el RTC) |
| BTN_3 | Decrementa campo (envuelve a 0) |

Secuencia: `SET_HOURS → SET_MINUTES → CONFIRM → MAIN`. Timeout de inactividad de
30 s descarta cambios. El campo en edición parpadea a 2 Hz (250 ms on / 250 ms off).

**Overlay de passkey:** durante pairing, MAIN muestra `PAIR: XXXXXX` en la fuente
de dígitos grande, sobreescribiendo el contenido normal hasta completar o timeout.

---

## 9. Perfil BLE GATT

Servicio custom con **11 características** (0xFF01–0xFF0A + el estándar 0x2A19),
todas testeables desde la app companion (no solo nRF Connect).

| Característica | UUID | Propiedades | Notas |
|---|---|---|---|
| Notificación | 0xFF01 | Write | `notif_payload_t` (64 B) |
| Hora/sync | 0xFF02 | Write | Sync de RTC desde el teléfono |
| Pasos | 0xFF03 | Read + Notify | Conteo del podómetro |
| Sueño/actigrafía | 0xFF04 | Read | Data de la última noche |
| Settings | 0xFF05 | Read + Write | `settings_payload_t` |
| Clima | 0xFF06 | Write | `weather_payload_t` (36 B), provider-agnóstico |
| Phone locator | 0xFF07 | Write/Notify | Trigger de ring |
| Alarmas | 0xFF08 | Read + Write | Lista sincronizada (66 B) |
| Calib. haptic | 0xFF09 | Read + Write | COMP + BEMF del DRV2605L |
| Reservado/extensión | 0xFF0A | — | Para crecimiento futuro |
| Nivel de batería | 0x2A19 | Read + Notify | Battery Service estándar |

Las características 0xFF06–0xFF0A se stubean en la Sesión 5 (entradas correctas en
la tabla de atributos + handlers vacíos) y se completan en la Sesión 6.

**Payload de notificación (64 B):**
```c
typedef struct {
    uint8_t  type;        // 0=mensaje, 1=llamada, 2=calendario, 3=otro
    uint8_t  app_id;      // SMS=0, WhatsApp=1, Gmail=2, Phone=3, ...
    char     sender[20];  // nombre/número del remitente (null-terminated)
    char     text[40];    // primeros 40 chars del mensaje (null-terminated)
    uint8_t  reserved[2];
} notif_payload_t;        // 64 bytes
```

**Seguridad de pairing:** el CC2640R2F soporta comparación numérica y passkey.
Mínimo *Just Works*; *passkey* para producción.

---

## 10. Almacenamiento no volátil (NV) — 9 ítems vía NVOCMP

| NV ID | Contenido | Tamaño | Actualización |
|---|---|---|---|
| 0x01 | Historial de pasos (7 días) | 14 B | Diario a medianoche |
| 0x02 | Actigrafía de sueño (última noche) | 60 B | Salida de ventana de sueño |
| 0x03 | Settings del dispositivo | `sizeof(settings_payload_t)` | Al escribir por BLE |
| 0x04 | Calibración DRV2605L (COMP + BEMF) | 4 B | Primer boot |
| 0x05 | Último clima conocido | 36 B | Al actualizar clima |
| 0x06 | Lista de alarmas | 66 B | Al escribir por BLE |
| 0x07 | Preferencia de unidad de temperatura | 1 B | Al hacer toggle |
| 0x08 | Pasos de hoy (recuperación de crash) | 4 B | Cada 100 pasos |
| 0x09 | Registros de bond BLE | auto | Gestionado por GAPBondMgr |

---

## 11. Plan de fases (0–3)

**Fase 0 — Firmware (ahora, sin hardware nuevo):** 6 sesiones de agente que
implementan todos los módulos. Testeable en LAUNCHXL-CC2640R2F con Sharp LCD
cableado. Estimado: **3–6 semanas part-time**.

**Fase 1 — App companion Android (en paralelo con Fase 0):** NotificationListener-
Service, clima, phone locator, sync de alarmas, display de pasos/sueño, OTA. Las
características BLE están stubeadas desde la Sesión 5, así que se puede desarrollar
en simultáneo.

**Fase 2 — PCB v2 (después de que el firmware de Fase 0 esté estable):** layout
KiCad v2 sobre el esquemático v1. Orden en JLCPCB (~$60–90 total por 10 placas +
componentes).

**Fase 3 — Sellado:** conformal coating + junta original + cerrar caja. Sin cutout
USB-C, sin tapones, sin modificar la caja. Rating ~3 ATM original preservado.

**Secuencia recomendada para minimizar trabajo desperdiciado:**

1. Montar CCS con el firmware Kepler original en la LaunchPad.
2. Firmware Fase 0 sobre hardware existente, stubeando drivers ausentes con guardas de compilación.
3. Escribir primero el driver del Sharp LCD (es el display de todo; desarrollar la UI contra él).
4. Botones, time-set, podómetro.
5. Empezar la app Android en paralelo; iterar el perfil GATT.
6. Cuando el firmware esté sustancialmente completo, empezar el layout KiCad v2.
7. Ordenar PCB en JLCPCB, poblar y testear.
8. Implementar el dock de carga inductiva.
9. Ensamblaje final, conformal coating, cerrar caja.

---

## 12. Sesiones de agente (Fase 0)

Cada sesión lee este Plan Maestro, `00_phase0_overview.md` y el spec de la tarea,
e implementa una sola tarea a la vez.

| Sesión | Contenido |
|---|---|
| **1** | Driver Sharp LCD (SPI, VCOM toggle por timer HW, framebuffer 1 bpp, update parcial por líneas) + UI renderer + fuentes. **Al terminar: chequear `.map` [R2].** |
| **2** | Handlers GPIO de los 3 botones con debounce + máquina de estados de time-set. |
| **3** | Driver haptic DRV2605L + definiciones de patrones de vibración (con guardas `KEPLER_HAS_DRV2605L`). |
| **4** | Driver LIS2DW12 + podómetro por hardware (banco B) + interrupt de wrist-raise + logging de épocas de actigrafía (guardas `KEPLER_HAS_LIS2DW12`). |
| **5** | Cola de eventos central · máquina de estados de energía + event loop de 6 pantallas · BLE manager · servicio GATT (11 características; 0xFF06–0xFF0A stubeadas) · storage NV (9 ítems) · driver de buzzer PWM. |
| **6** | Lógica completa de clima, phone locator, cronómetro y alarmas (completa las características stubeadas). |

Patrón de cada sesión: mostrar primero el `.h`, esperar confirmación, luego el
`.c`. En Codex, pedir confirmación implica detenerse y esperar respuesta. Al
final de la Sesión 5, checklist de integración completo.

**Gate Codex/CCS:** Codex no corre builds por regla repo-local. Para cumplir R1/R2,
la generación del `.map` la hace el usuario en CCS; el agente analiza el archivo
generado y documenta los márgenes de RAM/flash. Los artefactos generados no se
commitean.

**Recomendación de fuentes (verificar legibilidad a 144 px de ancho):**
`u8g2_font_logisoso16_tf` para la hora, `u8g2_font_6x10_tf` para texto de
notificaciones. Recordar que las fuentes son el gran consumidor de flash [R7].

---

## 13. App companion Android (Fase 1)

**Funciones:** NotificationListenerService (relay de notificaciones con whitelist
configurable de apps), clima, phone locator, sync de alarmas, display de pasos/
sueño, OTA (cuando exista flash SPI externa en v2).

### Clima — proveedor abstracto [R5]

Default **Open-Meteo** (sin API key, sin registro, sin límite para uso personal,
buena cobertura en Buenos Aires; mapea códigos WMO 4677 al enum de 8 condiciones).
OpenWeatherMap como alternativa (requiere key gratis).

```kotlin
interface WeatherProvider {
    suspend fun fetchCurrent(lat: Double, lon: Double): WeatherResult
    suspend fun fetchHourly(lat: Double, lon: Double, hours: Int): List<HourlyForecast>
    fun requiresApiKey(): Boolean
    fun providerName(): String
}
class OpenMeteoProvider : WeatherProvider { ... }            // default, sin key
class OpenWeatherMapProvider(apiKey: String) : WeatherProvider { ... }
```

Wizard de primer arranque: ofrece Open-Meteo primero (recomendado); OWM como
alternativa con link de registro + campo para la key. Selección en SharedPreferences,
cambiable en Settings. **El reloj queda agnóstico:** recibe `weather_payload_t`
(36 B) por 0xFF06 sea cual sea el proveedor. Refresh cada 30 min.

**Defaults sugeridos (configurables desde la app):** meta de pasos 8000/día;
ventana de sueño 22:00–08:00.

---

## 14. PCB v2 (Fase 2)

**Cambios respecto de la PCB v1:**

1. Quitar conector OLED SSD1306 y su sub-PCB adaptadora.
2. Agregar conector FPC del Sharp LS013B7DH03.
3. Agregar receptor de carga inductiva BQ51013B.
4. Agregar bobina de carga en cobre trasero (4–5 vueltas, ~20 mm interno).
5. Agregar conector LiPo 150 mAh + protección DW01A.
6. Agregar LIS2DW12 (I2C, LGA 2×2 mm).
7. Agregar DRV2605L (I2C, WSON-6).
8. Agregar pad de motor ERM coin de 10 mm.
9. Agregar ST25DV04K (I2C, SO8N).
10. Agregar traza de antena NFC en el perímetro.
11. Agregar pads de pogo pin de programación (5: VCC, GND, SWDIO, SWDCLK, RESET).
12. Quitar el circuito de carga USB si existiera en v1.
13. (Opcional, para OAD) **flash SPI externa** [R2].
14. Retener: CC2640R2F, cristal, matching RF, los 3 GPIO de botones, trazas de buzzer.

**Restricciones de PCB:**

- El baseline v2 es un contorno redondeado de **25 mm × 27 mm** y 1.0 mm de
  espesor, coherente con una PCB Ollee compatible con el módulo 593. Es una
  envolvente máxima de diseño, no un rectángulo libre: conservar los radios y
  liberar el asiento de la junta. Antes de producción, validar el datum contra
  una caja F91W física y el bracket v2.
- Los bordes de la PCB no deben sobresalir del asiento de la junta.
- El motor ERM va en un recess del bracket impreso, no en la PCB.
- La celda LiPo va detrás de la PCB (entre PCB y tapa).

**Bracket impreso v2:** pocket para motor ERM 10×2.5 mm; retención de la LiPo en
la cara trasera; alineación de acceso a pogo pins; posiciona la PCB para que la
bobina de carga quede centrada con la tapa.

**Especificaciones de orden JLCPCB:** 4 capas, **1.0 mm de espesor** (no el
estándar 1.6 mm — especificarlo), con `In1.Cu` como plano GND continuo y
`In2.Cu` para ruteo/distribución de potencia; relleno de cobre trasero evitando
el área bajo la bobina de carga y la antena NFC; acabado **ENIG** (recomendado
para los pads de pogo); ordenar **10 unidades** (price break + repuestos para
rework). Seleccionar el stackup estándar 4L/1.0 mm del fabricante y verificar
el retorno RF contra sus dieléctricos antes de liberar Gerbers.

---

## 15. Sellado y resistencia al agua (Fase 3)

- Aplicar 2–3 manos de conformal coating de silicona **MG Chemicals 422B** a la PCB v2 poblada.
- Enmascarar: conector del Sharp LCD, pads de pogo, conector del motor, conector LiPo.
- Curado completo (24–48 h) antes del ensamblaje.
- Instalar la junta de goma original del F91W (sin modificación).
- Cerrar la tapa con los tornillos originales.

**Resultado:** ~3 ATM de resistencia a salpicaduras del original, preservado. Sin
agujeros, sin cutout USB-C, sin modificar junta ni caja.

---

## 16. Consideraciones de riesgo y mitigaciones

| Riesgo | Severidad | Mitigación |
|---|---|---|
| No entrar en imagen única de 128 KB | **Alta** | Chequeo de `.map` tras Sesión 1; recorte de fuentes; OAD diferido [R2]. |
| Autonomía real por debajo de lo esperado | Media | Intervalo de conexión adaptativo; advertising nocturno; medir consumo real con la PCB v2 [R3]. |
| Daño del Sharp LCD por VCOM detenido | **Alta** | VCOM en timer de hardware, nunca en software bloqueante; verificar que sigue toggling en todos los estados de energía. |
| CS activo-alto del Sharp mal manejado | Media | Anotar la inversión en el driver; test temprano en Sesión 1. |
| Mapeo de botones distinto al asumido | Media | Verificar `Board.h` real antes de la Sesión 2 [§8.2]. |
| Actigrafía/wrist-raise mal en límites de sueño | Media | Documentar y testear la secuencia de reconfiguración de registros [R4]. |
| Deriva del cronómetro con RCOSC | Baja | Forzar GPT desde XOSC [R6]. |
| DRC de antenas NFC/carga | Media | Stackup de 3 zonas; DRC dedicado en KiCad; prototipar acoplamiento de carga antes del cierre de caja. |
| Espesor de PCB 1.0 mm no especificado | Baja | Indicar explícitamente en la orden JLCPCB. |
| Fricción de la API key de clima | Baja | Open-Meteo por default sin key [R5]. |

---

## 17. Criterios de éxito y checklist de aceptación

### Fase 0 — Firmware

- [ ] Sharp LCD renderiza hora/fecha/pasos legibles; VCOM toggling verificado en osciloscopio.
- [ ] `.map` revisado tras Sesión 1; margen de flash documentado; <110 KB confirmado.
- [ ] Los 3 botones con debounce; mapeo verificado contra `Board.h` real.
- [ ] Time-set completo con timeout de 30 s y parpadeo de campo a 2 Hz.
- [ ] Carrusel de 6 pantallas navegable; BTN_3 long vuelve a MAIN; wrist-raise → NOTIFICATIONS.
- [ ] Cronómetro con GPT desde XOSC; deriva ≤ ~14 cs/hora medida.
- [ ] Cola de eventos sin I/O en ISRs; estado STANDBY entre eventos verificado.
- [ ] GATT con 11 características; todas accesibles desde nRF Connect; 0xFF06–0xFF0A completas tras Sesión 6.
- [ ] 9 ítems NV persisten a través de reset; recuperación de pasos de hoy probada.
- [ ] Conteo de RAM acumulado documentado; ≥4 KB libres o plan de recorte aplicado.
- [ ] Compila limpio con todas las guardas de hardware en off (build de LaunchPad).

### Fase 1 — App Android

- [ ] NotificationListenerService con whitelist configurable; relay de llamada/mensaje/calendario.
- [ ] Open-Meteo por default sin key; abstracción `WeatherProvider` con OWM como alternativa.
- [ ] Sync de hora, alarmas; display de pasos/sueño.
- [ ] Phone locator hace sonar el teléfono desde el reloj.
- [ ] Pairing Just Works funcional; passkey para producción.

### Fase 2 — PCB v2

- [ ] Layout mantiene el contorno redondeado 25×27 mm y bordes fuera del asiento de junta; datum físico validado.
- [ ] Stackup de 3 zonas con DRC limpio.
- [ ] Acoplamiento de carga inductiva validado a ~100 mA antes del cierre.
- [ ] Espesor 1.0 mm y ENIG especificados en la orden.
- [ ] Pogo pads accesibles con el bracket montado.

### Fase 3 — Sellado

- [ ] Conformal coating curado; conectores/pads enmascarados.
- [ ] Junta original intacta; tapa cerrada; sin agujeros nuevos.
- [ ] Test de salpicadura (no inmersión) sin ingreso.

---

## 18. Decisiones abiertas

1. **OAD vs. no-OAD en v2:** ¿se agrega flash SPI externa a la PCB v2 (habilita OAD) o se acepta reprogramar por pogo pins? Define footprint y BOM.
2. **Tamaño del ring buffer de notificaciones:** 10 vs. 5 — depende del margen de RAM real medido tras la Sesión 5.
3. **Patrones de vibración finales del DRV2605L:** mapear cada `type` de notificación a un efecto de la librería ROM del DRV2605L.
4. **Whitelist de notificaciones default:** qué apps relayar de fábrica (sugerido: SMS, WhatsApp, llamadas, calendario).
5. **Corriente de carga objetivo del dock:** ~100 mA (lento/seguro) vs. más rápido; trade-off con calentamiento dentro de la caja sellada.

---

## 19. Referencias

- F91 Kepler (repo): `https://github.com/PegorK/F91_Kepler`
- CC2640R2F: `https://www.ti.com/product/CC2640R2F`
- TI SimpleLink CC2640R2 SDK: `https://www.ti.com/tool/SIMPLELINK-CC2640R2-SDK`
- Sharp LS013B7DH03: `https://www.sharpsde.com/products/displays/model/LS013B7DH03/`
- LIS2DW12: `https://www.st.com/en/mems-and-sensors/lis2dw12.html`
- DRV2605L: `https://www.ti.com/product/DRV2605L`
- BQ51013B: `https://www.ti.com/product/BQ51013B`
- ST25DV04K: `https://www.st.com/en/nfc/st25dv04k.html`
- Open-Meteo: `https://api.open-meteo.com/v1/forecast`
- Writeup Hackaday: `https://hackaday.com/2022/07/20/the-casio-smartwatch-you-never-had/`

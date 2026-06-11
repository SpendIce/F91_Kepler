# F91 Kepler ESP32 peripheral rig — compiles the kepler/ firmware sources
# unchanged against the TI-API shims in tistubs/ + rigshim.c.

set(KEPLER_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../kepler)

idf_component_register(
    SRCS
        rig_main.c
        rigshim.c
        ${KEPLER_DIR}/kepler_i2c.c
        ${KEPLER_DIR}/kepler_time.c
        ${KEPLER_DIR}/display/sharp_lcd.c
        ${KEPLER_DIR}/display/fonts.c
        ${KEPLER_DIR}/display/ui_renderer.c
        ${KEPLER_DIR}/display/weather_icons.c
        ${KEPLER_DIR}/haptic/drv2605l.c
        ${KEPLER_DIR}/haptic/haptic_patterns.c
        ${KEPLER_DIR}/accel/lis2dw12.c
        ${KEPLER_DIR}/accel/pedometer.c
        ${KEPLER_DIR}/accel/actigraphy.c
        ${KEPLER_DIR}/power/event_queue.c
        ${KEPLER_DIR}/ble/weather_service.c
        ${KEPLER_DIR}/ble/alarm_service.c
        ${KEPLER_DIR}/ble/ble_manager.c
        ${KEPLER_DIR}/storage/flash_store.c
    INCLUDE_DIRS
        tistubs
        .
        ${KEPLER_DIR}
    REQUIRES
        driver
        esp_timer
        nvs_flash
)

# Real hardware on the rig: haptic + accel breakouts present.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    KEPLER_HAS_SHARP_LCD=1
    KEPLER_HAS_DRV2605L=1
    KEPLER_HAS_LIS2DW12=1
)

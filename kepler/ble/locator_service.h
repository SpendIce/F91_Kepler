/******************************************************************************
 *
 * @file  locator_service.h
 *
 * @brief Phone locator command channel (0xFF07, Task 6).
 *
 *        Watch notifies 0x01 (start ringing) / 0x00 (stop) on 0xFF07; the
 *        app writes 0x01 back as acknowledgement (EVT_PHONE_LOCATOR_ACK).
 *        A 30 s auto-stop clock posts EVT_PHONE_LOCATOR_STOP if the user
 *        never stops it manually.  Navigating away does NOT stop ringing.
 *
 *****************************************************************************/

#ifndef LOCATOR_SERVICE_H
#define LOCATOR_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/* Construct the auto-stop clock.                                          */
void locator_service_init(void);

/* EVT_PHONE_LOCATOR_START handler: send start, arm auto-stop, set UI.     */
void locator_service_start(void);

/* EVT_PHONE_LOCATOR_STOP handler: send stop, cancel auto-stop, set UI.    */
void locator_service_stop(void);

/* 0xFF07 app write callback (acknowledgement).                            */
bool locator_service_on_write(const uint8_t *data, uint16_t len);

/* True while the phone is (commanded to be) ringing.                      */
bool locator_service_is_ringing(void);

#endif /* LOCATOR_SERVICE_H */

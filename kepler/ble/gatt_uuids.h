/******************************************************************************
 *
 * @file  gatt_uuids.h
 *
 * @brief Kepler custom GATT service characteristic UUIDs (spec 05/06).
 *
 *        Custom service: 0xFFFF (0x0000FFFF-0000-1000-8000-00805F9B34FB).
 *        0xFF06-0xFF0A are stubbed at Session 5 and completed in Task 6.
 *
 *****************************************************************************/

#ifndef GATT_UUIDS_H
#define GATT_UUIDS_H

#define KEPLER_SVC_UUID            0xFFFF

#define KEPLER_CHAR_NOTIF          0xFF01  /* WriteNoRsp  app->watch 64 B  */
#define KEPLER_CHAR_TIME_SYNC      0xFF02  /* Write       app->watch 4 B   */
#define KEPLER_CHAR_STEPS          0xFF03  /* Read+Notify watch->app 14 B  */
#define KEPLER_CHAR_SLEEP          0xFF04  /* Read        watch->app 20 B  */
#define KEPLER_CHAR_SETTINGS       0xFF05  /* Write       app->watch 16 B  */
#define KEPLER_CHAR_WEATHER        0xFF06  /* WriteNoRsp  app->watch 36 B  */
#define KEPLER_CHAR_LOCATOR        0xFF07  /* Write+Notify bidir 1 B       */
#define KEPLER_CHAR_ALARMS         0xFF08  /* WriteNoRsp  app->watch 66 B  */
#define KEPLER_CHAR_ALARM_TRIGGER  0xFF09  /* WriteNoRsp  app->watch 1 B   */
#define KEPLER_CHAR_WX_REFRESH     0xFF0A  /* Notify      watch->app 1 B   */

#define KEPLER_CHAR_BATTERY        0x2A19  /* standard Battery Level       */

#endif /* GATT_UUIDS_H */

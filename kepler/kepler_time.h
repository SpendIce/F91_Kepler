/******************************************************************************
 *
 * @file  kepler_time.h
 *
 * @brief Epoch <-> civil time conversion without libc time functions.
 *
 *        TI's runtime library has no gmtime_r/localtime_r, and mktime
 *        drags in TZ machinery — so the watch does its own conversion.
 *        Seconds_get() holds local wall-clock time (the phone syncs local
 *        time, not UTC); no timezone handling exists or is wanted here.
 *
 *****************************************************************************/

#ifndef KEPLER_TIME_H
#define KEPLER_TIME_H

#include <stdint.h>
#include <time.h>

/* Fill a struct tm (tm_year/mon/mday/wday/hour/min/sec) from an epoch.    *
 * Civil-date algorithm (Howard Hinnant's civil_from_days).                */
void kepler_epoch_to_tm(uint32_t epoch, struct tm *t);

/* Return `epoch` with its time-of-day replaced by hh:mm:00 — the date     *
 * part is preserved.  Replaces the gmtime_r + mktime round trip.          */
uint32_t kepler_epoch_set_hm(uint32_t epoch, uint8_t hours, uint8_t minutes);

#endif /* KEPLER_TIME_H */

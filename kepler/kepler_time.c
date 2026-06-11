/******************************************************************************
 *
 * @file  kepler_time.c
 *
 * @brief Epoch <-> civil time conversion implementation.
 *
 *****************************************************************************/

#include "kepler_time.h"

#include <string.h>

void kepler_epoch_to_tm(uint32_t epoch, struct tm *t)
{
    uint32_t days = epoch / 86400u;
    uint32_t rem  = epoch % 86400u;

    memset(t, 0, sizeof(*t));
    t->tm_hour = (int)(rem / 3600u);
    t->tm_min  = (int)((rem % 3600u) / 60u);
    t->tm_sec  = (int)(rem % 60u);
    t->tm_wday = (int)((days + 4u) % 7u);      /* 1970-01-01 was Thursday  */

    {
        int64_t  z   = (int64_t)days + 719468;
        int64_t  era = (z >= 0 ? z : z - 146096) / 146097;
        uint32_t doe = (uint32_t)(z - era * 146097);
        uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u)
                       / 365u;
        int64_t  y   = (int64_t)yoe + era * 400;
        uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
        uint32_t mp  = (5u * doy + 2u) / 153u;
        uint32_t d   = doy - (153u * mp + 2u) / 5u + 1u;
        uint32_t m   = mp < 10u ? mp + 3u : mp - 9u;

        if (m <= 2u) { y += 1; }
        t->tm_year = (int)(y - 1900);
        t->tm_mon  = (int)(m - 1u);
        t->tm_mday = (int)d;
    }
}

uint32_t kepler_epoch_set_hm(uint32_t epoch, uint8_t hours, uint8_t minutes)
{
    uint32_t midnight = epoch - (epoch % 86400u);
    return midnight + (uint32_t)hours * 3600u + (uint32_t)minutes * 60u;
}

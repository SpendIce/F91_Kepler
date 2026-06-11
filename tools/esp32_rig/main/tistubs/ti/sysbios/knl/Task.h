#ifndef RIGSHIM_TASK_H
#define RIGSHIM_TASK_H

#include <stdint.h>

/* Task_sleep(ticks) — ticks are 10 us; implemented over vTaskDelay.       */
void Task_sleep(uint32_t ticks);

#endif /* RIGSHIM_TASK_H */

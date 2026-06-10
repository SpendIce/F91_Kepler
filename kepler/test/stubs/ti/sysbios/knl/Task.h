#ifndef STUB_TASK_H
#define STUB_TASK_H

#include <stdint.h>

/* Task stub — Task_sleep is a no-op in single-threaded host tests.       */

extern uint32_t mock_task_sleep_ticks;   /* accumulates requested sleeps   */

static inline void Task_sleep(uint32_t ticks) {
    mock_task_sleep_ticks += ticks;
}

#endif /* STUB_TASK_H */

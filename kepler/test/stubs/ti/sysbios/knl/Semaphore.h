#ifndef STUB_SEMAPHORE_H
#define STUB_SEMAPHORE_H

#include <stdint.h>
#include <stddef.h>

/* Counting semaphore stub — non-blocking for host tests.
 * pend() returns immediately: true if count > 0, false otherwise.        */

typedef struct {
    int count;
    int mode;
} Semaphore_Struct;

typedef Semaphore_Struct *Semaphore_Handle;

typedef struct {
    int mode;
} Semaphore_Params;

#define Semaphore_Mode_COUNTING  1
#define Semaphore_Mode_BINARY    0

static inline void Semaphore_Params_init(Semaphore_Params *p) {
    p->mode = Semaphore_Mode_COUNTING;
}

static inline void Semaphore_construct(Semaphore_Struct *s, int count,
                                       Semaphore_Params *p) {
    s->count = count;
    s->mode  = (p != NULL) ? p->mode : Semaphore_Mode_COUNTING;
}

static inline Semaphore_Handle Semaphore_handle(Semaphore_Struct *s) {
    return s;
}

static inline void Semaphore_post(Semaphore_Handle h) {
    if (h) { h->count++; }
}

static inline int Semaphore_pend(Semaphore_Handle h, uint32_t timeout) {
    (void)timeout;
    if (h && h->count > 0) { h->count--; return 1; }
    return 0;
}

#endif /* STUB_SEMAPHORE_H */

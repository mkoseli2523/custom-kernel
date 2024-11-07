// timer.c
//

#include "timer.h"
#include "thread.h"
#include "csr.h"
#include "intr.h"
#include "halt.h" // for assert

// Hard-coded MTIMER device addresses for QEMU virt device

#define MTIME_ADDR 0x200BFF8
#define MTCMP_ADDR 0x2004000

// volatile uint64_t *mtime    = (volatile uint64_t *)MTIME_ADDR;
// volatile uint64_t *mtimecmp = (volatile uint64_t *)MTCMP_ADDR;

// EXPORTED GLOBAL VARIABLE DEFINITIONS
// 

char timer_initialized = 0;

struct condition tick_1Hz;
struct condition tick_10Hz;

uint64_t tick_1Hz_count = 0;
uint64_t tick_10Hz_count = 0;

#define MTIME_FREQ 10000000 // from QEMU include/hw/intc/riscv_aclint.h

// INTERNVAL GLOBAL VARIABLE DEFINITIONS
//

// INTERNAL FUNCTION DECLARATIONS
//

static inline uint64_t get_mtime(void);
static inline void set_mtime(uint64_t val);
static inline uint64_t get_mtimecmp(void);
static inline void set_mtimecmp(uint64_t val);

// EXPORTED FUNCTION DEFINITIONS
//

void timer_init(void) {
    assert (intr_initialized);
    condition_init(&tick_1Hz, "tick_1Hz");
    condition_init(&tick_10Hz, "tick_10Hz");

    // Set mtimecmp to maximum so timer interrupt does not fire

    set_mtime(0);
    set_mtimecmp(UINT64_MAX);
    csrc_mie(RISCV_MIE_MTIE);

    timer_initialized = 1;
}

void timer_start(void) {
    set_mtime(0);
    set_mtimecmp(MTIME_FREQ / 10);
    csrs_mie(RISCV_MIE_MTIE);
}

// timer_handle_interrupt() is dispatched from intr_handler in intr.c

/* timer interrupt handler
broadcasts 1Hz tick and 10Hz every 1 and 1/10 seconds respectively */

void timer_intr_handler(void) {
    static uint64_t tick_counter = 0;
    uint64_t now, next;

    // read the current time from the mtime register
    // now = *mtime;
    now = get_mtime();

    // set when the next interrupt occurs
    // here we want it to fire 10 times a second
    next = now + (MTIME_FREQ / 10);
    // *mtimecmp = next;
    set_mtimecmp(next);

    // signal the tick 10Hz condition
    condition_broadcast(&tick_10Hz);
    tick_10Hz_count++;

    // increment the tick counter
    tick_counter++;

    // every 10 ticks (1 second), signal the tick_1Hz condition
    if (tick_counter >= 10) {
        condition_broadcast(&tick_1Hz);
        tick_1Hz_count++;
        tick_counter = 0;  // reset the counter
    }
}

static inline uint64_t get_mtime(void) {
    return *(volatile uint64_t*)MTIME_ADDR;
}

static inline void set_mtime(uint64_t val) {
    *(volatile uint64_t*)MTIME_ADDR = val;
}

static inline uint64_t get_mtimecmp(void) {
    return *(volatile uint64_t*)MTCMP_ADDR;
}

static inline void set_mtimecmp(uint64_t val) {
    *(volatile uint64_t*)MTCMP_ADDR = val;
}

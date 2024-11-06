// timer.c
//

#include "timer.h"
#include "thread.h"
#include "csr.h"
#include "intr.h"
#include "halt.h" // for assert

// EXPORTED GLOBAL VARIABLE DEFINITIONS
// 

char timer_initialized = 0;

struct condition tick_1Hz;
struct condition tick_10Hz;

uint64_t tick_1Hz_count;
uint64_t tick_10Hz_count;

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

/***********************************************************************
* void timer_intr_handler(void)
* 
* Handles the timer interrupt, updating the 10Hz and 1Hz tick counts, and 
* signaling any threads waiting on the 10Hz or 1Hz conditions. It also 
* schedules the next timer interrupt for 1/10th of a second later.
* 
* Arguments: 
* void
* 
* Returns: 
* void
*
* Effects: 
* - Increments the 10Hz tick count and broadcasts the 10Hz condition.
* - Every 10 ticks, increments the 1Hz tick count and broadcasts the 1Hz condition.
* - Reschedules the timer interrupt for 1/10th of a second in the future.
***********************************************************************/
void timer_intr_handler(void) {
    // FIXME your code goes here
    // Increment the 10Hz tick count (happens every 1/10th of a second)
    tick_10Hz_count++; 

    // Signal the 10Hz condition to wake up any waiting threads
    condition_broadcast(&tick_10Hz);
    
    // For every 10 ticks (once a second), handle the 1Hz condition
    if (tick_10Hz_count % 10 == 0){
        // Increment 1Hz tick count
        tick_1Hz_count++; 

        // Signal the 1Hz condition to wake up any waiting threads
        condition_broadcast(&tick_1Hz);
    }

    // Schedule the next timer interrupt in 1/10th of a second (MTIME_FREQ / 10)
    uint64_t next_timer = get_mtime() + (MTIME_FREQ / 10);
    set_mtimecmp(next_timer);
}

// Hard-coded MTIMER device addresses for QEMU virt device

#define MTIME_ADDR 0x200BFF8
#define MTCMP_ADDR 0x2004000

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

// plic.c - RISC-V PLIC
//

#include "plic.h"
#include "console.h"

#include <stdint.h>

// COMPILE-TIME CONFIGURATION
//

// *** Note to student: you MUST use PLIC_IOBASE for all address calculations,
// as this will be used for testing!

#ifndef PLIC_IOBASE
#define PLIC_IOBASE 0x0C000000
#endif

#define PLIC_SRCCNT 0x400
#define PLIC_CTXCNT 1

// INTERNAL FUNCTION DECLARATIONS
//

// *** Note to student: the following MUST be declared extern. Do not change these
// function delcarations!

extern void plic_set_source_priority(uint32_t srcno, uint32_t level);
extern int plic_source_pending(uint32_t srcno);
extern void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_set_context_threshold(uint32_t ctxno, uint32_t level);
extern uint32_t plic_claim_context_interrupt(uint32_t ctxno);
extern void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno);

// Currently supports only single-hart operation. The low-level PLIC functions
// already understand contexts, so we only need to modify the high-level
// functions (plic_init, plic_claim, plic_complete).

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
    int i;

    // Disable all sources by setting priority to 0, enable all sources for
    // context 0 (M mode on hart 0).

    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(0, i);
    }
}

extern void plic_enable_irq(int irqno, int prio) {
    trace("%s(irqno=%d,prio=%d)", __func__, irqno, prio);
    plic_set_source_priority(irqno, prio);
}

extern void plic_disable_irq(int irqno) {
    if (0 < irqno)
        plic_set_source_priority(irqno, 0);
    else
        debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_irq(void) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(0);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(0, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // pointer to the specific priority address
    volatile uint32_t * priority_address = (uint32_t *)(PLIC_IOBASE + srcno * sizeof(uint32_t));
    *priority_address = level;
}

int plic_source_pending(uint32_t srcno) {
    // the pending bit is stored in bit srcno % 32 of word srcno / 32
    uint32_t pending_bit_index = srcno % 32; 
    uint32_t pending_word_index = srcno / 32;

    // pending bit address calculation
    // the registers are stored at memory address 0x00001000 of plic mem region
    volatile uint32_t * pending_address = (uint32_t *)(PLIC_IOBASE + 0x00001000 + pending_word_index * sizeof(uint32_t));

    if (*pending_address & (1 << pending_bit_index)) {
        return 1;
    } else {
        return 0;
    }
}

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // enable registers are packed the same way as pending
    uint32_t enable_bit_index = srcno % 32;
    uint32_t enable_word_index = srcno / 32;

    enable_word_index *= sizeof(uint32_t);
    enable_word_index += ctxno * 0x80; 

    // enable bit address calculation
    // the registers are stored at mem address 0x00002000 of plic mem region
    uintptr_t enable_address_base = (uintptr_t)(PLIC_IOBASE + 0x00002000 + enable_word_index);
    volatile uint32_t * enable_address = (volatile uint32_t *) enable_address_base; // need this to suppress an error
    *enable_address |= (1 << enable_bit_index);
}

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // similar to enable function
    uint32_t disable_bit_index = srcno % 32;
    uint32_t disable_word_index = srcno / 32;

    disable_word_index *= sizeof(uint32_t);
    disable_word_index += ctxno * 0x80;

    uintptr_t disable_address_base = (uintptr_t)(PLIC_IOBASE + 0x00002000 + disable_word_index);
    volatile uint32_t * disable_address = (volatile uint32_t *) disable_address_base;
    *disable_address &= ~(1 << disable_bit_index);
}

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // threshold address calculation
    // base address is 0x00200000 within plic mem region
    uintptr_t threshold_address_base = (uintptr_t)(PLIC_IOBASE + 0x00200000 + ctxno * 0x1000);
    volatile uint32_t * threshold_address = (volatile uint32_t *) threshold_address_base;

    *threshold_address = level;
}

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // interrupt claim address calculation
    // base address is 0x00200000 + 0x4 within plic mem region
    uintptr_t claim_address_base = (uintptr_t)(PLIC_IOBASE + 0x00200000 + 0x4 + ctxno * 0x1000);
    volatile uint32_t * claim_address = (volatile uint32_t *) claim_address_base;
    
    return *claim_address;
}

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // similar to claim indexing
    uintptr_t complete_address_base = (uintptr_t)(PLIC_IOBASE + 0x00200000 + 0x4 + ctxno * 0x1000);
    volatile uint32_t * complete_address = (volatile uint32_t *) complete_address_base;

    *complete_address = srcno;
}
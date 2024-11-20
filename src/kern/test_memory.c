// main.c - Main function: runs shell to load executable
//

#ifdef MAIN_TRACE
#define TRACE
#endif

#ifdef MAIN_DEBUG
#define DEBUG
#endif

#define INIT_PROC "init0" // name of init process executable

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "memory.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "config.h"

void test_memory_alloc_free();
void test_memory_space_reclaim();

extern uintptr_t main_mtag;       // SATP value for the main memory space

struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

void main(void) {
    struct io_intf * initio;
    struct io_intf * blkio;
    void * mmio_base;
    int result;
    int i;

    console_init();
    memory_init();
    intr_init();
    devmgr_init();
    thread_init();

    console_printf("testing memory_alloc_page() and memory_free_page()\n");
    test_memory_alloc_free();
    console_printf("memory_alloc_page() and memory_free_page() pass\n");

    console_printf("testing memory_space_reclaim()\n");
    test_memory_space_reclaim();
    console_printf("memory_space_reclaim() pass\n");
}

void test_memory_alloc_free() {
    void * page1 = memory_alloc_page();
    assert(page1 != NULL);

    void * page2 = memory_alloc_page();
    assert(page2 != NULL);
    assert(page1 != page2);

    memory_free_page(page1);
    memory_free_page(page2);

    void * page3 = memory_alloc_page();

    assert(page3 == page1 || page3 == page2);

    memory_free_page(page3);
}

void test_memory_space_reclaim() {
    // Create a secondary memory space
    struct pte *secondary_root_pt = (struct pte *)memory_alloc_page();
    assert(secondary_root_pt != NULL);
    memset(secondary_root_pt, 0, PAGE_SIZE);

    console_printf("root pte *: 0x%x\n", secondary_root_pt);

    // Map some pages in the secondary memory space
    uintptr_t test_vaddr1 = 0x400000; // Arbitrary user-space virtual address
    uintptr_t test_vaddr2 = 0x401000; // Next page

    // Map pages in the secondary memory space
    struct pte *pte1 = walk_pt(secondary_root_pt, test_vaddr1, 1);
    assert(pte1 != NULL);

    console_printf("pte1 *: 0x%x\n", pte1);

    void *page1 = memory_alloc_page();
    assert(page1 != NULL);
    uintptr_t pa1 = (uintptr_t) page1;
    pte1->ppn = pa1 >> 12;
    pte1->flags = PTE_V | PTE_R | PTE_W | PTE_U;

    struct pte *pte2 = walk_pt(secondary_root_pt, test_vaddr2, 1);
    assert(pte2 != NULL);

    console_printf("pte2 *: 0x%x\n", pte2);

    void *page2 = memory_alloc_page();
    assert(page2 != NULL);
    uintptr_t pa2 = (uintptr_t) page2;
    pte2->ppn = pa2 >> 12;
    pte2->flags = PTE_V | PTE_R | PTE_W | PTE_U;

    // Switch to the secondary memory space
    // Construct the SATP value for the secondary memory space
    uintptr_t secondary_satp = (8ULL << 60) | (0xFFFF << 44) | (secondary_root_pt->ppn & 0xFFFFFFFFFFFULL);
    // Set the SATP register to switch to the secondary memory space
    csrw_satp(secondary_satp);
    asm inline ("sfence.vma" ::: "memory");

    // Verify that we're in the secondary memory space
    uintptr_t current_satp = csrr_satp();
    assert(current_satp == secondary_satp);

    // Call memory_space_reclaim()
    memory_space_reclaim();

    // Verify that we're back to the main memory space
    current_satp = csrr_satp();
    assert(current_satp == main_mtag);

    // Verify that the pages mapped in the secondary memory space have been reclaimed
    void *page3 = memory_alloc_page();
    assert(page3 != NULL);
    assert(page3 == page1 || page3 == page2);

    void *page4 = memory_alloc_page();
    assert(page4 != NULL);
    assert(page4 == page1 || page4 == page2);
    assert(page3 != page4);

    // Clean up
    memory_free_page(page3);
    memory_free_page(page4);

    console_printf("memory_space_reclaim() test passed: secondary memory space reclaimed and pages freed\n");
}
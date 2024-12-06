#include "console.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "intr.h"
#include "device.h"

void test_non_readable();
void test_non_writable();

void main(void) {
    console_init();
    memory_init();
    intr_init();
    devmgr_init();
    thread_init();

    console_printf("Starting memory fault tests...\n");

    console_printf("Testing non-writable page access...\n");
    test_non_writable();
}

void test_non_writable() {
    uintptr_t non_writable_page;

    // Allocate a page without write permissions (read-only)
    non_writable_page = (uintptr_t)memory_alloc_and_map_page(0xC0100000, PTE_R | PTE_U);

    if (non_writable_page == 0) {
        console_printf("Failed to allocate non-writable page.\n");
        return;
    }

    console_printf("Allocated non-writable page at: 0x%lx\n", non_writable_page);

    // Attempt to write to the non-writable page
    console_printf("Attempting to write to non-writable page...\n");
    *(volatile char *)non_writable_page = 'A';

    // If we reach here, the test failed
    console_printf("Test failed: Successfully wrote to a non-writable page.\n");
}

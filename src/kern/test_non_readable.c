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

    console_printf("Testing non-readable page access...\n");
    test_non_readable();

}

void test_non_readable() {
    uintptr_t non_readable_page;
    char value;

    // Allocate a page without read permissions (write-only)
    non_readable_page = (uintptr_t)memory_alloc_and_map_page(0xC0101000, PTE_W | PTE_U);

    if (non_readable_page == 0) {
        console_printf("Failed to allocate non-readable page.\n");
        return;
    }

    console_printf("Allocated non-readable page at: 0x%lx\n", non_readable_page);

    // Attempt to read from the non-readable page
    console_printf("Attempting to read from non-readable page...\n");
    value = *(volatile char *)non_readable_page;

    // If we reach here, the test failed
    console_printf("Test failed: Successfully read from a non-readable page.\n");
}


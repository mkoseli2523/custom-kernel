#include "syscall.h"
#include "string.h"

void main(void) {
    const char *kernel_address = (const char *)0x80000000; // Kernel space address
    char value;

    _msgout("Attempting to read from kernel space...");
    value = *kernel_address; // Should trigger a page fault
    _msgout("Unexpected success: Read value from kernel space");
}

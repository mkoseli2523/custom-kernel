#include "syscall.h"
#include "string.h"

void main(void) {
    char *kernel_page = (char *)0x80000000; // Kernel address
    char value;

    // Attempt to write to a kernel page
    _msgout("Attempting to write to a kernel page...");
    *kernel_page = 'A'; // Should trigger a page fault
    _msgout("Unexpected success: Wrote to a kernel page");
}

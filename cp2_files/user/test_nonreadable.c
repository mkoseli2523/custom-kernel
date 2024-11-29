#include "syscall.h"
#include "string.h"

void main(void) {
    char *non_readable_page = (char *)0xC0101000; // Address in user space
    char value;

    _msgout("Attempting to read from a non-readable page...");
    
    // Attempt to read from the page (this should trigger a page fault)
    value = *non_readable_page;

    // If the read succeeds, this message should never be printed
    _msgout("Handled page fault.");
}

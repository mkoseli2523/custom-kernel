#include "syscall.h"
#include "string.h"

void main(void) {
    char *non_writable_page = (char *)0xC0100000; // Address in user space

    _msgout("Attempting to write to a non-writable page...");
    
    // Attempt to write to the page (this should trigger a page fault)
    *non_writable_page = 'A';

    // If the write succeeds, this message should never be printed
    _msgout("Handled page fault.");
}

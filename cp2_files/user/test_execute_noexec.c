#include "syscall.h"
#include "string.h"

void execute_invalid(void) {
    _msgout("This should not execute!");
}

void main(void) {
    void (*invalid_code)(void) = (void (*)(void))0xC0100000; // Address in user space

    _msgout("Attempting to execute from a non-executable page...");
    invalid_code(); // Should trigger a page fault
    _msgout("Unexpected success: Executed from a non-executable page");
}
 
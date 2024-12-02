#include "syscall.h"
#include "string.h"
#include "../kern/error.h"

void main(void) {
    char *invalid_buffer = (char *)0x80000000; // Kernel space address (not readable by user)
    char valid_buffer[64] = "Test data for syswrite.";
    int fd, result;

    // Open an existing device (e.g., ser1)
    fd = _devopen(0, "ser", 1);
    if (fd < 0) {
        _msgout("Failed to open device.");
        _exit();
    }

    // **Test Case 1: sysread with invalid buffer**
    _msgout("Testing sysread with invalid buffer...");
    result = _read(fd, invalid_buffer, 64);

    // Verify the result
    if (result == -EINVAL) { 
        _msgout("sysread correctly rejected invalid buffer.");
    } else {
        _msgout("sysread test failed: unexpected behavior.");
    }

    // Test Case 2: syswrite with valid buffer**
    _msgout("Testing syswrite with valid buffer...");
    result = _write(fd, valid_buffer, strlen(valid_buffer));

    // Verify the result
    if (result >= 0) {
        _msgout("syswrite succeeded with valid buffer.");
    } else {
        _msgout("syswrite test failed with valid buffer.");
    }

    // Test Case 3: syswrite with invalid buffer**
    _msgout("Testing syswrite with invalid buffer...");
    result = _write(fd, invalid_buffer, 64);

    // Verify the result
    if (result == -EINVAL) { 
        _msgout("syswrite correctly rejected invalid buffer.");
    } else {
        _msgout("syswrite test failed: unexpected behavior.");
    }

    // Close the file descriptor
    _close(fd);
    _exit();
}

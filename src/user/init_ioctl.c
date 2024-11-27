#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    const char *filename = "hello.txt";
    uint64_t metadata;
    int fd, result;

    // Open the file
    fd = _fsopen(0, filename);
    if (fd < 0) {
        _msgout("Failed to open file");
        _exit();
    }

    // Get file length using IOCTL_GETLEN
    result = _ioctl(0, IOCTL_GETLEN, &metadata);
    if (result == 0) {
        char message[64];
        snprintf(message, sizeof(message), "File length: %llu", metadata);
        _msgout(message);
    } else {
        _msgout("Failed to get file length");
    }

    // Get current file position using IOCTL_GETPOS
    result = _ioctl(0, IOCTL_GETPOS, &metadata);
    if (result == 0) {
        char message[64];
        snprintf(message, sizeof(message), "Current file position: %llu", metadata);
        _msgout(message);
    } else {
        _msgout("Failed to get file position");
    }

    // Set file position to 0 using IOCTL_SETPOS
    metadata = 0;
    result = _ioctl(0, IOCTL_SETPOS, &metadata);
    if (result == 0) {
        _msgout("Successfully set file position to 0");
    } else {
        _msgout("Failed to set file position");
    }

    // Close the file
    _close(0);
    _exit();
}
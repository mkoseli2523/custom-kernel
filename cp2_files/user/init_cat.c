#include "syscall.h"
#include "string.h"

void main(void) {
    const char *filename = "hello.txt";
    char buffer[256];
    int fd, nbytes;

    // Open the file to read
    fd = _fsopen(0, filename);
    if (fd < 0) {
        _msgout("Failed to open file");
        _exit();
    }

    // Read the file and print its contents to the terminal
    while ((nbytes = _read(0, buffer, sizeof(buffer))) > 0) {
        // Null-terminate the buffer to safely use _msgout
        buffer[nbytes] = '\0';
        _msgout(buffer);
    }

    // Check if there was an error reading the file
    if (nbytes < 0) {
        _msgout("Error reading file");
    }

    _close(0);
    _exit();
}

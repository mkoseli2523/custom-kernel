#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {
    int fd;
    int pid;

    // Open the file for writing
    fd = _fsopen(0, "testfile.txt");
    if (fd < 0) {
        _msgout("Parent: _fsopen failed");
        _exit();
    }

    _msgout("Parent: file opened");

    // Fork the process
    pid = _fork();
    if (pid < 0) {
        _msgout("Parent: _fork failed");
        _close(fd);
        _exit();
    }

    if (pid == 0) {
        // Child process
        _msgout("Child: starting writes");
        
        uint64_t offset = 30; // Child writes start at offset 30
        if (_ioctl(fd, IOCTL_SETPOS, &offset) < 0) {
            _msgout("Child: IOCTL_SETPOS failed");
            _close(fd);
            _exit();
        }

        for (int i = 0; i < 3; i++) {
            char message[10]; // Write 10 bytes per write
            snprintf(message, sizeof(message), "CWrite%d\n", i + 1);
            if (_write(fd, message, strlen(message)) < 0) {
                _msgout("Child: _write failed");
                _close(fd);
                _exit();
            }
        }

        _msgout("Child: writes completed, closing file");
        _close(fd);
        _exit();
    } else {
        // Parent process
        _msgout("Parent: starting writes");
        
        uint64_t offset = 0; // Parent writes start at offset 0
        if (_ioctl(fd, IOCTL_SETPOS, &offset) < 0) {
            _msgout("Parent: IOCTL_SETPOS failed");
            _close(fd);
            _exit();
        }

        for (int i = 0; i < 3; i++) {
            char message[10]; // Write 10 bytes per write
            snprintf(message, sizeof(message), "PWrite%d\n", i + 1);
            if (_write(fd, message, strlen(message)) < 0) {
                _msgout("Parent: _write failed");
                _close(fd);
                _exit();
            }
        }

        _msgout("Parent: writes completed");

        // Wait for the child process to complete
        _wait(pid);

        // Read the contents of the file
        char buffer[128];
        int bytes_read;
        offset = 0;
        if (_ioctl(fd, IOCTL_SETPOS, &offset) < 0) {
            _msgout("Parent: IOCTL_SETPOS failed for reading");
            _close(fd);
            _exit();
        }

        _msgout("Parent: reading file contents");
        while ((bytes_read = _read(fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            _msgout(buffer);
        }

        _msgout("Parent: file read completed, closing file");
        _close(fd);
        _exit();
    }
}

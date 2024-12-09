#include "syscall.h"
#include "string.h"

void main(void) {
    int fd;
    int tid;

    // Open a file
    fd = _fsopen(0, "testfile.txt");
    if (fd < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    _msgout("Parent process: opened the file");

    // Fork the process
    tid = _fork();
    if (tid < 0) {
        _msgout("_fork failed");
        _exit();
    }

    if (tid == 0) {
        // Child process
        _msgout("Child process: starting operations after parent closes the file");
        
        // Perform write operation
        const char *child_message = "write something ....\n";
        if (_write(fd, child_message, strlen(child_message)) < 0) {
            _msgout("Child process: _write failed");
            _exit();
        }
        _msgout("Child process: write successful");

        // Close the file
        _close(fd);
        _msgout("Child process: file closed");
        _exit();
    } else {
        // Parent process
        _msgout("Parent process: closing the file");
        _close(fd);
        _msgout("Parent process: file closed");

        // Wait for child to finish
        _msgout("Parent process: waiting for child to exit");
        _wait(tid);

        // Reopen the file and read its contents
        fd = _fsopen(0, "testfile.txt");
        if (fd < 0) {
            _msgout("Parent process: unable to reopen the file");
            _exit();
        }
        _msgout("Parent process: reopened the file");

        char buf[128];
        int n = _read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            _msgout("Parent process: file contents:");
            _msgout(buf);
        }
        _close(fd);
        _msgout("Parent process: exiting");
        _exit();
    }
}

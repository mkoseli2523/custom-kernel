// // #include "fs.h"
// // #include "io.h"
// // #include <string.h>
// // #include "console.h"

// // #define BUFFER_SIZE 4096  // Define buffer size for testing

// // extern char _companion_f_start;  // Start of companion data
// // extern char _companion_f_end;    // End of companion data

// // void test_filesystem_operations() {
// //     // Step 1: Initialize io_lit with the companion data memory range
// //     struct io_lit lit;
// //     struct io_intf *io;
// //     size_t companion_size = &_companion_f_end - &_companion_f_start;

// //     // io = iolit_init(&lit, &_companion_f_start, companion_size); // Initialize with companion data
// //     // if (!io) {
// //     //     console_printf("Failed to initialize io_lit with companion data.\n");
// //     //     return;
// //     // }

// //     // // Step 2: Mount the filesystem using the lit interface
// //     // if (fs_mount(io) < 0) {
// //     //     console_printf("Failed to mount filesystem.\n");
// //     //     return;
// //     // }

// //     // // Step 3: Open a file in the mounted filesystem
// //     // struct io_intf *file_io;
// //     // if (fs_open("hello.txt", &file_io) < 0) {  // Adjust filename as per kfs.raw
// //     //     console_printf("Failed to open file.\n");
// //     //     return;
// //     // }

// //     // // Alternating Write and Read Tests
// //     // char read_buffer[BUFFER_SIZE];
// //     // const char *data1 = "Test write 1 - Hello, filesystem!";
// //     // const char *data2 = "Test write 2 - Overwrite previous data";
// //     // const char *data3 = "Test write 3 - More data after reset";
    
// //     // // First Write and Read
// //     // long bytes_written = fs_write(file_io, data1, strlen(data1));
// //     // console_printf("Bytes written (data1): %ld\n", bytes_written);

// //     // uint64_t position = 0;
// //     // fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset position to start

// //     // long bytes_read = fs_read(file_io, read_buffer, bytes_written);
// //     // read_buffer[bytes_read] = '\0';  // Null-terminate the read data
// //     // console_printf("Data read (after first write): %s\n", read_buffer);

// //     // // Second Write and Read - Overwriting with new data
// //     // fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset to start again
// //     // bytes_written = fs_write(file_io, data2, strlen(data2));
// //     // console_printf("Bytes written (data2): %ld\n", bytes_written);

// //     // fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset for read
// //     // bytes_read = fs_read(file_io, read_buffer, bytes_written);
// //     // console_printf("*** File io before read_buffer: %d\n", file_io);
// //     // read_buffer[bytes_read] = '\0';
// //     // console_printf("*** File io after read_buffer: %d\n", file_io);
// //     // console_printf("Data read (after second write): %s\n", read_buffer);

// //     // // Third Write and Read - Writing additional data without resetting
// //     // bytes_written = fs_write(file_io, data3, strlen(data3));
// //     // console_printf("Bytes written (data3): %ld\n", bytes_written);

// //     // fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset position for reading everything
// //     // console_printf("*** File io before read: %d\n", file_io);
// //     // bytes_read = fs_read(file_io, read_buffer, BUFFER_SIZE);
// //     // console_printf("*** File io after read: %d\n", file_io);
// //     // console_printf("*** File io before read_buffer: %d\n", file_io);

// //     // //read_buffer[bytes_read] = '\0';
// //     // console_printf("*** File io after read_buffer: %d\n", file_io);
// //     // console_printf("Data read (after third write): %s\n", read_buffer);

// //     // // Random Access Tests
// //     // console_printf("*** File io before mid_position: %d\n", file_io);
// //     // uint64_t mid_position = 39;
// //     // console_printf("*** File io before ioctl: %d\n", file_io);
// //     // fs_ioctl(file_io, IOCTL_SETPOS, &mid_position);  // Seek to a position in the middle
// //     // console_printf("*** File io after ioctl: %d\n", file_io);
// //     // bytes_read = fs_read(file_io, read_buffer, 3);  // Read a small portion
// //     // read_buffer[bytes_read] = '\0';
// //     // console_printf("Data read from mid position (10): %s\n", read_buffer);

// //     // fs_close(file_io);  // Close the file after tests
// //     io = iolit_init(&lit, &_companion_f_start, companion_size); // Initialize with companion data
// // if (!io) {
// //     console_printf("Failed to initialize io_lit with companion data.\n");
// //     return;
// // }

// // // Step 2: Mount the filesystem using the lit interface
// // if (fs_mount(io) < 0) {
// //     console_printf("Failed to mount filesystem.\n");
// //     return;
// // }

// // // Step 3: Open a file in the mounted filesystem
// // struct io_intf *file_io;
// // if (fs_open("hello.txt", &file_io) < 0) {  // Adjust filename as per kfs.raw
// //     console_printf("Failed to open file.\n");
// //     return;
// // }

// // // Step 4: Read initial data from the file without writing anything
// // char read_buffer[BUFFER_SIZE];
// // uint64_t position = 0;
// // fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset position to start

// // long bytes_read = fs_read(file_io, read_buffer, BUFFER_SIZE);
// // if (bytes_read < 0) {
// //     console_printf("Failed to read from file.\n");
// //     fs_close(file_io);
// //     return;
// // }

// // //read_buffer[bytes_read] = '\0';  // Null-terminate the read data
// // console_printf("Initial data read from file: %s\n", read_buffer);

// // const char *data1 = "Test write 1 - Hello, filesystem!";
// //     const char *data2 = "Test write 2 - Overwrite previous data";
// //     const char *data3 = "Test write 3 - More data after reset";
    
// //     // First Write and Read
// //     long bytes_written = fs_write(file_io, data1, strlen(data1));
// //     console_printf("Bytes written (data1): %ld\n", bytes_written);

// //     position = 0;
// //     fs_ioctl(file_io, IOCTL_SETPOS, &position);  // Reset position to start

// //     bytes_read = fs_read(file_io, read_buffer, bytes_written);
// //     //read_buffer[bytes_read] = '\0';  // Null-terminate the read data
// //     console_printf("Data read (after first write): %s\n", read_buffer);

// // // Step 5: Close the file after tests
// // fs_close(file_io);
// // }

// // int main() {
// //     test_filesystem_operations();
// //     return 0;
// // }



// #include "console.h"
// #include "thread.h"
// #include "device.h"
// #include "uart.h"
// #include "timer.h"
// #include "intr.h"
// #include "heap.h"
// #include "io.h"
// #include "virtio.h"
// #include "halt.h"
// #include "elf.h"
// #include "fs.h"
// #include "string.h"

// // end of kernel image (defined in kernel.ld)
// extern char _kimg_end[];

// #define RAM_SIZE (8*1024*1024)
// #define RAM_START 0x80000000UL
// #define KERN_START RAM_START
// #define USER_START 0x80100000UL

// #define UART0_IOBASE 0x10000000
// #define UART1_IOBASE 0x10000100
// #define UART0_IRQNO 10

// #define VIRT0_IOBASE 0x10001000
// #define VIRT1_IOBASE 0x10002000
// #define VIRT0_IRQNO 1

// extern char _companion_f_start[];
// extern char _companion_f_end[];

// int main(void) {
//     struct io_lit fs_lit;
//     struct io_intf* fs_io;
//     int result;
//     char* name;

//     // Initialize the filesystem interface
//     fs_io = iolit_init(&fs_lit, _companion_f_start, _companion_f_end - _companion_f_start);

//     // Mount the filesystem
//     result = fs_mount(fs_io);
//     if (result != 0){   
//         panic("fs_mount failed");
//         return -1;
//     }
//     console_printf("File System Mounted Successfully!\n");

//     // Declare a pointer for the file I/O interface (uninitialized)
//     struct io_intf* hello_io_ptr = NULL;

//     // Open the file named "hello"
//     name = "hello.txt";
//     result = fs_open(name, &hello_io_ptr); // Pass pointer to hello_io_ptr
//     if (result != 0){   
//         panic("fs_open failed");
//         return -1;
//     }
//     console_printf("Hello Opened Successfully!\n");

//     // Read from the file into a buffer
//     console_printf("Hello_io_ptr: %p\n", *hello_io_ptr);
//     char readBuffer[3];
//     result = fs_read(hello_io_ptr, readBuffer, sizeof(readBuffer)); // sizeof(buffer) ensures correct size
//     if (result < 0){   
//         panic("fs_read failed");
//         return -1;
//     }

//     // Print out each byte read from the file
//     for(int i = 0; i < result; i++) {
//         console_printf("%d\n", readBuffer[i]);
//     }
    
//     console_printf("Read Bytes: %d\n", result);

   
//     char writeBuffer[4] = {'D','O','M',"X"};
//     int x = 0;
//     fs_ioctl(hello_io_ptr, IOCTL_SETPOS, &x);
//     console_printf("Hello_io_ptr: %p\n", *hello_io_ptr);

//     result = fs_write(hello_io_ptr, writeBuffer, sizeof(writeBuffer)-1); // sizeof(buffer) ensures correct size
//     if (result < 0){   
//         panic("fs_write failed");
//         return -1;
//     }

//     // Print out each byte read from the file
//     console_printf("Bytes Written: %d\n", result);

//     console_printf("reading changed file\n");
//     x = 0;
//     fs_ioctl(hello_io_ptr, IOCTL_SETPOS, &x);
 


//     char read2Buffer[20];
//     result = fs_read(hello_io_ptr, read2Buffer, sizeof(read2Buffer)); // sizeof(buffer) ensures correct size
//     if (result < 0){   
//         panic("fs_read failed");
//         return -1;
//     }

//     for(int i = 0; i < result; i++) {
//         console_printf("%d\n", read2Buffer[i]);
//     }
    
//     console_printf("Read Bytes: %d\n", result);

// }





#include "console.h"
#include "fs.h"
#include "io.h"
#include <stdint.h>

// end of kernel image (defined in kernel.ld)
extern char _companion_f_start[];
extern char _companion_f_end[];

int main(void) {
    struct io_lit fs_lit;
    struct io_intf* fs_io;
    int result;
    char* name;

    // Initialize the filesystem interface
    fs_io = iolit_init(&fs_lit, _companion_f_start, _companion_f_end - _companion_f_start);

    // Mount the filesystem
    result = fs_mount(fs_io);
    if (result != 0) {   
        console_printf("fs_mount failed");
        return -1;
    }
    console_printf("Filesystem mounted successfully!\n");

    // Declare a pointer for the file I/O interface (uninitialized)
    struct io_intf* hello_io_ptr = NULL;

    // Open a file named "hello.txt"
    name = "hello.txt";
    result = fs_open(name, &hello_io_ptr);
    if (result != 0) {   
        console_printf("fs_open failed");
        return -1;
    }
    console_printf("File '%s' opened successfully!\n", name);

    // Check the length of the file using fs_ioctl
    uint64_t file_length = 0;
    fs_ioctl(hello_io_ptr, IOCTL_GETLEN, &file_length);
    console_printf("File length: %llu bytes\n", file_length);

    // Check the initial position of the file (should be 0)
    uint64_t file_position = 0;
    fs_ioctl(hello_io_ptr, IOCTL_GETPOS, &file_position);
    console_printf("Initial file position: %llu\n", file_position);

    // Read the file in its entirety into a buffer
    char readBuffer[64];  // Ensure buffer size is sufficient
    result = fs_read(hello_io_ptr, readBuffer, sizeof(readBuffer));
    if (result < 0) {   
        console_printf("fs_read failed");
        return -1;
    }

    console_printf("Read %d bytes:\n", result);
    for (int i = 0; i < result; i++) {
        console_printf("%d", readBuffer[i]);
    }
    console_printf("\n");

    // Demonstrate writing to the file to overwrite contents
    char writeBuffer[] = "NEW CONTENT";
    file_position = 0;
    fs_ioctl(hello_io_ptr, IOCTL_SETPOS, &file_position);  // Seek to beginning
    result = fs_write(hello_io_ptr, writeBuffer, sizeof(writeBuffer) - 1);
    if (result < 0) {   
        console_printf("fs_write failed");
        return -1;
    }
    console_printf("Wrote %d bytes to overwrite contents.\n", result);

    // Re-read the entire file to verify overwritten contents
    file_position = 0;
    fs_ioctl(hello_io_ptr, IOCTL_SETPOS, &file_position);
    result = fs_read(hello_io_ptr, readBuffer, sizeof(readBuffer));
    if (result < 0) {   
        console_printf("fs_read failed");
        return -1;
    }

    console_printf("Re-read %d bytes after overwrite:\n", result);
    for (int i = 0; i < result; i++) {
        console_printf("%d", readBuffer[i]);
    }
    console_printf("\n");

    // Demonstrate seeking (changing position) within the file and reading
    file_position = 5;
    fs_ioctl(hello_io_ptr, IOCTL_SETPOS, &file_position);
    result = fs_read(hello_io_ptr, readBuffer, 10);  // Read 10 bytes from position 5
    if (result < 0) {   
        console_printf("fs_read failed at position 5");
        return -1;
    }
    console_printf("Read %d bytes from position 5:\n", result);
    for (int i = 0; i < result; i++) {
        console_printf("%d", readBuffer[i]);
    }
    console_printf("\n");

    // Check block size using fs_ioctl
    uint64_t block_size = 0;
    fs_ioctl(hello_io_ptr, IOCTL_GETBLKSZ, &block_size);
    console_printf("Filesystem block size: %llu bytes\n", block_size);

    return 0;
}

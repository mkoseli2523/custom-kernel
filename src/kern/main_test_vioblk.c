#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "string.h"

extern char _kimg_end[];

#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

void test_vioblk_read(struct io_intf *blkio);
void test_vioblk_write(struct io_intf *blkio);
void test_ioctl(struct io_intf *blkio);

void main(void) {
    struct io_intf *blkio;
    void *mmio_base;
    int result;

    // Initialize system components
    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void*)USER_START);

    // Attach UART
    for (int i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }

    // Attach VirtIO devices
    for (int i = 0; i < 8; i++) {
        mmio_base = (void*)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE - VIRT0_IOBASE) * i;
        virtio_attach(mmio_base, VIRT0_IRQNO + i);
    }

    intr_enable();
    timer_start();

    // Open the block device
    result = device_open(&blkio, "blk", 0);
    if (result != 0) {
        panic("device_open failed");
    }

    // perform read and write tests
    test_vioblk_write(blkio);
    test_vioblk_read(blkio);
    test_ioctl(blkio);
    // int tid1 = thread_spawn("writer_thread", (void*)test_vioblk_write, blkio);
    // int tid2 = thread_spawn("reader_thread", (void*)test_vioblk_read, blkio);

    // thread_join(tid1);
    // thread_join(tid2);

    // close the block device and halt the system
    ioclose(blkio);
    // halt();
}

// Function to test writing multiple blocks to the block device
void test_vioblk_write(struct io_intf *blkio) {
    // create a buffer with data to write
    const unsigned long data_len = 4096; // Write 4KB of data (assuming 512-byte blocks, that's 8 blocks)
    // const unsigned long data_len = 512; // Write 4KB of data (assuming 512-byte blocks, that's 8 blocks)
    char *data = kmalloc(data_len);
    if (!data) {
        kprintf("Error allocating memory for write buffer\n");
        return;
    }

    // fill the buffer with a known pattern
    for (unsigned long i = 0; i < data_len; i++) {
        data[i] = (char)((i % 26) + 'A'); // ASCII characters A-Z
    }

    long bytes_written;
    int result;

    // set position to 0
    uint64_t position = 0;
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &position);
    if (result != 0) {
        kprintf("Error setting position: %d\n", result);
        kfree(data);
        return;
    }

    // write data to the block device
    bytes_written = blkio->ops->write(blkio, data, data_len);
    if (bytes_written < 0) {
        kprintf("Error writing to device: %ld\n", bytes_written);
    } else {
        kprintf("Written %ld bytes to device\n", bytes_written);
    }

    // print the data written to the terminal
    kprintf("Data written to the device:\n");
    for (unsigned long i = 0; i < data_len; i++) {
        kprintf("0x%x ", data[i]);

        if (i % 512 == 511) {
            kprintf("\n\n");
        }
    }
    kprintf("\n");

    kfree(data);
}

// function to test reading multiple blocks from the block device
void test_vioblk_read(struct io_intf *blkio) {
    const unsigned long data_len = 4096; // Read 4KB of data
    // const unsigned long data_len = 512; // Read 4KB of data
    char *buffer = kmalloc(data_len);
    if (!buffer) {
        kprintf("Error allocating memory for read buffer\n");
        return;
    }

    long bytes_read;
    int result;

    // set position to 0
    uint64_t position = 0;
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &position);
    if (result != 0) {
        kprintf("Error setting position: %d\n", result);
        kfree(buffer);
        return;
    }

    // read data from the block device
    bytes_read = blkio->ops->read(blkio, buffer, data_len);
    if (bytes_read < 0) {
        kprintf("Error reading from device: %ld\n", bytes_read);
    } else {
        kprintf("Read %ld bytes from device\n", bytes_read);
    }

    // print the data read from the device to the terminal
    kprintf("Data read from the device:\n");
    for (long i = 0; i < bytes_read; i++) {
        kprintf("0x%x ", buffer[i]);

        if (i % 512 == 511) {
            kprintf("\n\n");
        }
    }
    kprintf("\n");

    // verify that the data read matches what was written
    // recreate the expected data pattern
    char *expected_data = kmalloc(data_len);
    if (!expected_data) {
        kprintf("Error allocating memory for expected data\n");
        kfree(buffer);
        return;
    }
    for (unsigned long i = 0; i < data_len; i++) {
        expected_data[i] = (char)((i % 26) + 'A');
    }

    if (memcmp(buffer, expected_data, data_len) == 0) {
        kprintf("Success: Data read matches data written.\n");
    } else {
        kprintf("Failure: Data read does not match data written.\n");
    }

    kfree(buffer);
    kfree(expected_data);
}

void test_ioctl(struct io_intf *blkio) {
    int result, position;
    position = 0;

    // set position
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &position);
    if (result != 0) {
        kprintf("Error setting position: %d\n", result);
        return;
    }

    kprintf("set position to: %d\n", position);

    int get_length, get_pos, get_blksz;

    // get length
    result = blkio->ops->ctl(blkio, IOCTL_GETLEN, &get_length);
    if (result != 0) {
        kprintf("Error getting length: %d\n", result);
        return;
    }

    // get position
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &get_pos);
    if (result != 0) {
        kprintf("Error getting position: %d\n", result);
        return;
    }

    // get blocksize
    result = blkio->ops->ctl(blkio, IOCTL_GETBLKSZ, &get_blksz); 
    if (result != 0) {
        kprintf("Error getting blocksize: %d\n", result);
    }

    kprintf("get length: %d\nget position: %d\nget blocksize: %d\n", get_length, get_pos, get_blksz);
}
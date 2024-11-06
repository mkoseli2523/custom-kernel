//           test_elf_loader.c - Main function: runs shell to load executable
//          

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "io.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];
extern char _companion_f_start[];
extern char _companion_f_end[];


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

static void shell_main(struct io_intf * termio);

void main(void) {
    struct io_intf * termio;
    // struct io_intf * blkio;

    void * mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void*)USER_START);

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }

    result = device_open(&termio, "ser", 1);

    if (result != 0)
        panic("Could not open ser1");
    
    shell_main(termio);

}

void shell_main(struct io_intf * termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    struct io_lit elflit;
    struct io_intf * exeio;
    void (*exe_entry)(struct io_intf*);
    char cmdbuf[9] = "test_elf";
    int tid;
    int result;

    termio = ioterm_init(&ioterm, termio_raw);

    ioputs(termio, "Testing ELF Loader using memory-backed io_lit.");

    // Initialize io_lit with the companion ELF file in memory
    void *buf = _companion_f_start;
    size_t size = _companion_f_end - _companion_f_start;

    exeio = iolit_init(&elflit, buf, size);

    // Test elf_load with the memory-backed I/O interface
    ioprintf(termio, "Loading ELF executable from memory\n");
    
    result = elf_load(exeio, &exe_entry);

    ioprintf(termio, "elf_load returned %d\n", result);

    if (result < 0) {
        ioprintf(termio, "%s: Error %d\n", cmdbuf, -result);
    } else {
        ioprintf(termio, "Spawning thread for ELF entry at %p\n", exe_entry);
        tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

        if (tid < 0) {
            ioprintf(termio, "%s: Error %d\n", cmdbuf, -tid);
        } else {
            thread_join(tid);
        }
    }

    ioclose(exeio);
}

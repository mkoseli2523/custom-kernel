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
#define USER_END   0x81000000UL


#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

static void shell_main(struct io_intf * termio);
static void test_elf_loader(struct io_intf * termio);


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
    
    test_iolit_operations(termio);

    test_elf_loader(termio);

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
    console_printf("elf_load returned %d\n", result);

    ioprintf(termio, "\nelf_load returned %d\n", result);

    if (result < 0) {
        ioprintf(termio, "%s: Error %d\n", cmdbuf, -result);
        console_printf("%s: Error %d\n", "test_elf", -result);
    } else {
        ioprintf(termio, "Spawning thread for ELF entry at %p\n", exe_entry);
        console_printf("\nSpawning thread for ELF entry at %p\n", exe_entry);

        tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

        if (tid < 0) {
            ioprintf(termio, "%s: Error %d\n", cmdbuf, -tid);
        } else {
            thread_join(tid);
        }
    }

    ioclose(exeio);
}


void test_elf_loader(struct io_intf *termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    struct io_lit elflit;
    struct io_intf *exeio;
    void (*exe_entry)(struct io_intf*);
    int result;

    termio = ioterm_init(&ioterm, termio_raw);

    console_printf("Testing ELF Loader using memory-backed io_lit.\n");

    // Use the companion ELF file in memory for testing
    void *buf = _companion_f_start;
    size_t size = _companion_f_end - _companion_f_start;
    exeio = iolit_init(&elflit, buf, size);

    // === Test 1: Check for little-endian RISC-V 64-bit ELF ===
    console_printf("\nRunning Test 1: Verifying ELF endianness and architecture...\n");
    Elf64_Ehdr *test_elf_header = (Elf64_Ehdr*)buf;
    test_elf_header->e_ident[5] = ELFDATA2MSB; // Set to big-endian temporarily to simulate failure

    result = elf_load(exeio, &exe_entry);
    if (result == -9) {
        console_printf("Test 1.1 Passed: ELF loader rejected big-endian ELF\n");
    } else {
        console_printf("Test 1.1 Failed: Expected rejection, but got %d\n", result);
    }

    // Sub-test 1.2: Verify RISC-V 64-bit architecture check
    test_elf_header->e_machine = 0xFFFF; // Set to an unsupported machine temporarily to simulate failure
    result = elf_load(exeio, &exe_entry);

    if (result == -3) {
        console_printf("Test 1.2 Passed: ELF loader rejected non-RISC-V 64-bit ELF\n");
    } else {
        console_printf("Test 1.2 Failed: Expected rejection for architecture, but got %d\n", result);
    }

    test_elf_header->e_machine = RV64_MACHINE;

    // Reset endianness for subsequent tests
    test_elf_header->e_ident[5] = ELFDATA2LSB;

    // === Test 2: Loading ELF sections into kernel memory ===
    console_printf("\nRunning Test 2: Loading ELF sections into memory...\n");

    result = elf_load(exeio, &exe_entry);
    if (result == 0) {
    Elf64_Ehdr *test_elf_header = (Elf64_Ehdr*)buf;

    for (uint16_t i = 1; i < test_elf_header->e_phnum; i++) {
        // Calculate the offset for the current program header
        Elf64_Phdr *phdr = (Elf64_Phdr *)(buf + test_elf_header->e_phoff + i * sizeof(Elf64_Phdr));

        // Check if the program header segment is within valid memory bounds
        if ((void *)phdr->p_vaddr >= (void *)USER_START && 
            (void *)(phdr->p_vaddr + phdr->p_filesz) <= (void *)LOAD_END) {

            // Only call memcmp if the range is valid
            if (memcmp((void *)phdr->p_vaddr, buf + phdr->p_offset, phdr->p_filesz) == 0) {
                console_printf("Test 2 Passed: ELF sections loaded correctly\n");
            } else {
                console_printf("Test 2 Failed: Memory contents incorrect after load\n");
            }
        } else {
            console_printf("Test 2 Failed: Segment is out of valid memory bounds\n");
        }
    }
    } else {
    console_printf("Test 2 Failed: ELF loader returned error %d\n", result);
    }


    // === Test 3: Verifying entry pointer updates ===
    console_printf("\nRunning Test 3: Checking ELF entry point...\n");
    
    if (result == 0 && exe_entry == (void(*)(struct io_intf*))test_elf_header->e_entry) {
        console_printf("Test 3 Passed: Entry pointer correctly updated to %p\n", exe_entry);
    } else {
        console_printf("Test 3 Failed: Entry pointer mismatch or elf_load failed\n");
    }

    ioclose(exeio);
}

void test_iolit_operations(struct io_intf *termio_raw) {
    console_printf("\nTesting io_lit Operations...\n");

    // Define test buffer and io_lit setup
    char buffer[100] = "Hello, world!";
    struct io_lit iolit;
    struct io_intf *io = iolit_init(&iolit, buffer, sizeof(buffer));
    long result;
    char read_buf[20];
    char write_data[] = "TestWrite";
    uint64_t pos;

    // === Test 1: Read from in-memory buffer ===
    console_printf("\nTest 1: Read from buffer\n");
    iolit.pos = 0;  // Reset position to the beginning
    result = ioread(io, read_buf, sizeof(read_buf) - 1);
    if (result > 0) {
        read_buf[result] = '\0'; // Null-terminate read data
        console_printf("Read Data: %s\n", read_buf);
    } else {
        console_printf("Read failed with result: %ld\n", result);
    }

    // === Test 2: Write to in-memory buffer ===
    console_printf("\nTest 2: Write to buffer\n");
    iolit.pos = 0;  // Reset position to the beginning
    result = iowrite(io, write_data, sizeof(write_data) - 1);
    if (result > 0) {
        console_printf("Write successful. Buffer now contains: %s\n", buffer);
    } else {
        console_printf("Write failed with result: %ld\n", result);
    }

    // === Test 3: Get Length with ioctl ===
    console_printf("\nTest 3: Get Length with ioctl\n");
    result = ioctl(io, IOCTL_GETLEN, &pos);
    if (result == 0) {
        console_printf("Buffer length: %llu\n", pos);
    } else {
        console_printf("Get length failed with result: %ld\n", result);
    }

    // === Test 4: Get Position with ioctl ===
    console_printf("\nTest 4: Get Position with ioctl\n");
    result = ioctl(io, IOCTL_GETPOS, &pos);
    if (result == 0) {
        console_printf("Current Position: %llu\n", pos);
    } else {
        console_printf("Get position failed with result: %ld\n", result);
    }

    // === Test 5: Set Position with ioctl ===
    console_printf("\nTest 5: Set Position with ioctl\n");
    pos = 6; // Arbitrary position within buffer bounds
    result = ioctl(io, IOCTL_SETPOS, &pos);
    if (result == 0) {
        // Verify position set
        ioctl(io, IOCTL_GETPOS, &pos);
        console_printf("Position set successfully. Current Position: %llu\n", pos);
    } else {
        console_printf("Set position failed with result: %ld\n", result);
    }

    // === Test 6: Attempt to set position out of bounds ===
    console_printf("\nTest 6: Attempt to set position out of bounds\n");
    pos = 150; // Out of bounds
    result = ioctl(io, IOCTL_SETPOS, &pos);
    if (result == -1) {
        console_printf("Out of bounds position correctly rejected\n");
    } else {
        console_printf("Failed: Out of bounds position set with result: %ld\n", result);
    }
}


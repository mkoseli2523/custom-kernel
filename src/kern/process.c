// process.c - user process
//

#include "process.h"

#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif


// COMPILE-TIME PARAMETERS
//

// NPROC is the maximum number of processes

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_PID 0

// The main user process struct

static struct process main_proc;

// A table of pointers to all user processes in the system

struct process * proctab[NPROC] = {
    [MAIN_PID] = &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

/**
 * initializes the process manager by setting up the main process
 * 
 * this function initializes the process manager by, initializing each struct field
 * of the main_proc variable, main proc id is always set to 0.
 */

void procmgr_init(void) {
    // initialize the main user process struct
    // init proc id
    main_proc.id = MAIN_PID;
    
    // init tid
    main_proc.tid = running_thread();

    // init mem space identifier
    main_proc.mtag = active_memory_space();

    thread_set_process(main_proc.tid, &main_proc);
    
    // init io 
    memset(main_proc.iotab, 0, sizeof(main_proc.iotab));

    // mark process manager as initialized
    procmgr_initialized = 1;
}



/**
 * executes a program referred to by the I/O interface passed in as an argument
 * 
 * this function performs the following steps
 * (a) virtual memory mappings belonging to other user processes should be unmapped
 * (b) fresh root page table should be created and initialized with the default mappings
 * for a user process (not required for cp2)
 * (c) executable should be loaded from the I/O interface provided as an argument into the 
 * mapped pages
 * (d) the thread associated with the process needs to be started in user-mode
 * 
 * @param exeio     pointer to the io interface represanting the executable load
 * 
 * @return          returns 0 on succes, negative value on failure
 */

int process_exec(struct io_intf * exeio) {
    int result;
    void (*entry_point)(void);
    struct thread_stack_anchor* stack_anchor;
    uintptr_t usp;

    // (a) unmap any virtual memory mappings begongin to other user processes
    memory_unmap_and_free_user();

    // (b) no need to implement for cp2

    // (c) load teh executable from io interface into memory
    result = elf_load(exeio, &entry_point);

    if (result < 0) {
        kprintf("process_exec: elf load failed\n");
        return -1;
    }

    // ensure entry point is within the user mem space
    if ((uintptr_t) entry_point < USER_START_VMA || (uintptr_t) entry_point >= USER_END_VMA) {
        console_printf("process_exec: start address is not within the valid range\n");
        return -1;
    }

    // (d) start the process in user mode
    // set up the stack
    usp = USER_STACK_VMA;

    // allocate and map the user stac page
    void * user_stack = memory_alloc_and_map_page(usp - PAGE_SIZE, PTE_R | PTE_W | PTE_U);

    if (!user_stack) {
        kprintf("process_exec: stack allocation failed\n");
        return -1;
    }

    // flush tlb to ensure cpu recognizes the update
    asm inline ("sfence.vma" ::: "memory");
    
    // set up stack anchor
    // copied from thread_spawn
    void * stack_page = memory_alloc_page();
    stack_anchor = stack_page + PAGE_SIZE - sizeof(struct thread_stack_anchor);
    stack_anchor->thread = cur_thread();
    stack_anchor->reserved = 0;

    // call thread_jump_user (in thrasm.s) to finish switching to umode
    _thread_finish_jump(stack_anchor, usp, (uintptr_t)entry_point);

    // this line should not execute
    panic("process_exec: returned from user mode execution\n");
    return -1;
}



/**
 * cleans up after a finished process by reclaimin the resources of the process
 * 
 * cleans up anything associated with the process at the initial execution including
 * process memory space, open io interface, and associated kernel thread
 */

void __attribute__ ((noreturn)) process_exit(void) {
    struct process* current_proc = current_process();

    if (!current_proc) panic("prcess_exit: current process doesn't exist, ::confused_face_emoji\n");

    // reclaim the memory space
    memory_space_reclaim();

    // close open io device
    for (int i = 0; i < PROCESS_IOMAX; i++) {
        if (current_proc->iotab[i]) {
            current_proc->iotab[i]->ops->close(current_proc->iotab[i]);
            current_proc->iotab[i] = NULL;
        }
    }

    // terminate the associated thread
    thread_exit();

    // thread_exit should not return
    panic("process_exit: thread_exit() returned ::confused_face_emoji\n");
}
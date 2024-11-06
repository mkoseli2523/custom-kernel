# thrasm.s - Special functions called from thread.c
#

# struct thread * _thread_swtch(struct thread * resuming_thread)
        
# Switches from the currently running thread to another thread and returns when
# the current thread is scheduled to run again. Argument /resuming_thread/ is
# the thread to be resumed. Returns a pointer to the previously-scheduled
# thread. This function is called in thread.c. The spelling of swtch is
# historic.

        .text
        .global _thread_swtch
        .type   _thread_swtch, @function

_thread_swtch:

        # We only need to save the ra and s0 - s12 registers. Save them on
        # the stack and then save the stack pointer. Our declaration is:
        # 
        #   struct thread * _thread_swtch(struct thread * resuming_thread);
        #
        # The currently running thread is suspended and resuming_thread is
        # restored to execution. swtch returns when execution is switched back
        # to the calling thread. The return value is the previously executing
        # thread. Interrupts are enabled when swtch returns.
        #
        # tp = pointer to struct thread of current thread (to be suspended)
        # a0 = pointer to struct thread of thread to be resumed
        # 

        sd      s0, 0*8(tp)
        sd      s1, 1*8(tp)
        sd      s2, 2*8(tp)
        sd      s3, 3*8(tp)
        sd      s4, 4*8(tp)
        sd      s5, 5*8(tp)
        sd      s6, 6*8(tp)
        sd      s7, 7*8(tp)
        sd      s8, 8*8(tp)
        sd      s9, 9*8(tp)
        sd      s10, 10*8(tp)
        sd      s11, 11*8(tp)
        sd      ra, 12*8(tp)
        sd      sp, 13*8(tp)

        mv      tp, a0

        ld      sp, 13*8(tp)
        ld      ra, 12*8(tp)
        ld      s11, 11*8(tp)
        ld      s10, 10*8(tp)
        ld      s9, 9*8(tp)
        ld      s8, 8*8(tp)
        ld      s7, 7*8(tp)
        ld      s6, 6*8(tp)
        ld      s5, 5*8(tp)
        ld      s4, 4*8(tp)
        ld      s3, 3*8(tp)
        ld      s2, 2*8(tp)
        ld      s1, 1*8(tp)
        ld      s0, 0*8(tp)
                
        ret

        .global _thread_setup
        .type   _thread_setup, @function

# void _thread_setup (
#      struct thread * thr,             in a0
#      void * sp,                       in a1
#      void (*start)(void * arg),       in a2
#      void * arg)                      in a3
#
# Sets up the initial context for a new thread. The thread will begin execution
# in /start/, receiving /arg/ as the first argument. 

/***********************************************************************
* void _thread_setup(struct thread *thr, void *sp, void (*start)(void *), void *arg)
* 
* Initializes the thread context, setting the stack pointer, start function, 
* and argument. Ensures the thread calls `thread_exit` after completing execution.
* 
* Arguments: 
*  - struct thread *thr: Thread context pointer.
*  - void *sp: Stack pointer for the thread.
*  - void (*start)(void *): Pointer to the start function.
*  - void *arg: Argument for the start function.
* 
* Returns: 
*  - void
*
* Effects: 
*  - Prepares the thread to execute the start function with its argument.
*  - Sets the stack pointer and ensures proper thread termination.
***********************************************************************/

_thread_setup:
        # a0 = pointer to thread (thr)
        # a1 = stack pointer (sp)
        # a2 = start function (start)
        # a3 = argument for start function (arg)

        # Store the stack pointer (sp) in the thread's context
        sd      a1, 13*8(a0)

        # Store the argument (arg) in s1 of the thread's context
        sd      a3, 1*8(a0)

        # Store the start function (start) in s0 of the thread's context
        sd      a2, 0*8(a0)

        # Set return address to call_thread_exit (this function ensures clean termination)
        la      t0, call_thread_exit
        sd      t0, 12*8(a0)       # Store return address in the thread context

        ret                         # Return from _thread_setup


/***********************************************************************
* call_thread_exit:
* 
* Helper function that:
* - Calls the thread's start function with its argument.
* - Ensures that after the start function completes, it jumps to `thread_exit`
*   to properly terminate the thread.
* 
* Arguments: 
*  - The thread context pointer (tp) is expected to be set prior to the call.
* 
* Effects: 
*  - Loads the thread's start function and its argument.
*  - Jumps to the start function.
*  - Ensures that upon completion, it jumps to `thread_exit`.
***********************************************************************/
call_thread_exit:
        # Load the argument (stored in s1) into a0, which is the argument register for the start function
        ld      a0, 1*8(tp)        # tp holds the current thread pointer, load the argument (arg) from s1

        # Load the start function address (stored in s0) into t0
        ld      t0, 0*8(tp)        # Load the start function from s0 into t0

        # Jump to the start function with the argument passed in a0
        jalr    t0                 # Jump to the start function

        # When the start function finishes, jump to thread_exit to terminate the thread
        j       thread_exit        # Jump to thread_exit (no return expected)




# Statically allocated stack for the idle thread.

        .section        .data.idle_stack
        .align          16
        
        .equ            IDLE_STACK_SIZE, 1024
        .equ            IDLE_GUARD_SIZE, 0

        .global         _idle_stack
        .type           _idle_stack, @object
        .size           _idle_stack, IDLE_STACK_SIZE

        .global         _idle_guard
        .type           _idle_guard, @object
        .size           _idle_guard, IDLE_GUARD_SIZE

_idle_stack:
        .fill   IDLE_STACK_SIZE, 1, 0xA5

_idle_guard:
        .fill   IDLE_GUARD_SIZE, 1, 0x5A
        .end


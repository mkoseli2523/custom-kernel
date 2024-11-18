// memory.c - Memory management
//

#ifndef TRACE
#ifdef MEMORY_TRACE
#define TRACE
#endif
#endif

#ifndef DEBUG
#ifdef MEMORY_DEBUG
#define DEBUG
#endif
#endif

#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"
#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

#include <stdint.h>

// EXPORTED VARIABLE DEFINITIONS
//

char memory_initialized = 0;
uintptr_t main_mtag;

// IMPORTED VARIABLE DECLARATIONS
//

// The following are provided by the linker (kernel.ld)

extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// INTERNAL TYPE DEFINITIONS
//

union linked_page {
    union linked_page * next;
    char padding[PAGE_SIZE];
};

struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

// INTERNAL MACRO DEFINITIONS
//

#define VPN2(vma) (((vma) >> (9+9+12)) & 0x1FF)
#define VPN1(vma) (((vma) >> (9+12)) & 0x1FF)
#define VPN0(vma) (((vma) >> 12) & 0x1FF)
#define MIN(a,b) (((a)<(b))?(a):(b))

// INTERNAL FUNCTION DECLARATIONS
//
struct pte* walk_pt(struct pte* root, uintptr_t vma, int create);

static inline int wellformed_vma(uintptr_t vma);
static inline int wellformed_vptr(const void * vp);
static inline int aligned_addr(uintptr_t vma, size_t blksz);
static inline int aligned_ptr(const void * p, size_t blksz);
static inline int aligned_size(size_t size, size_t blksz);

static inline uintptr_t active_space_mtag(void);
static inline struct pte * mtag_to_root(uintptr_t mtag);
static inline struct pte * active_space_root(void);

static inline void * pagenum_to_pageptr(uintptr_t n);
static inline uintptr_t pageptr_to_pagenum(const void * p);

static inline void * round_up_ptr(void * p, size_t blksz);
static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz);
static inline size_t round_up_size(size_t n, size_t blksz);
static inline void * round_down_ptr(void * p, size_t blksz);
static inline size_t round_down_size(size_t n, size_t blksz);
static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz);

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

static inline void sfence_vma(void);

// INTERNAL GLOBAL VARIABLES
//

static union linked_page * free_list;

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

// EXPORTED VARIABLE DEFINITIONS
//

// EXPORTED FUNCTION DEFINITIONS
// 

void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    union linked_page * page;
    void * heap_start;
    void * heap_end;
    size_t page_cnt;
    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic("Kernel too large");

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level page table
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] =
        ptab_pte(main_pt0_0x80000, PTE_G);

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging. This part always makes me nervous.

    main_mtag =  // Sv39
        ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
        pageptr_to_pagenum(main_pt2);
    
    csrw_satp(main_mtag);
    sfence_vma();

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = round_up_ptr(heap_start, PAGE_SIZE);
    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += round_up_size (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("Not enough memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_list = heap_end; // heap_end is page aligned
    page_cnt = (RAM_END - heap_end) / PAGE_SIZE;

    kprintf("Page allocator: [%p,%p): %lu pages free\n",
        free_list, RAM_END, page_cnt);

    // Put free pages on the free page list
    // TODO: FIXME implement this (must work with your implementation of
    // memory_alloc_page and memory_free_page).
    
    // Allow supervisor to access user memory. We could be more precise by only
    // enabling it when we are accessing user memory, and disable it at other
    // times to catch bugs.

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

// rest of the functions go here


void memory_space_reclaim(void) {
    
}

/**
 * Allocates a physical memory page and maps it to a virtual address
 * 
 * This function allocates a single physical memory page and establishes a 
 * mapping between the specified virtual memory address and the allocated physical page.
 * The mapping is created in the current virtual memory space with the specified access permissions.
 * 
 * @param vma           virtual memory address to be mapped. Must be page aligned and well-formed for the current paging scheme.
 * @param rwxug_flags   A combination of the access permission flags for the mapping:
 *                      - PTE_R: Readable
 *                      - PTE_W: Writable
 *                      - PTE_E: Executable
 *                      - PTE_U: User-Accessible 
 *                      - PTE_G: Global
 * 
 * @return - A pointer to the virtual memory address if successful.
 *         - NULL if the allocation or mapping fails
 */
void *memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags){
    // Ensure virtual address is well-formed and page-aligned
    if(!wellformed_vma(vma) || !aligned_addr(vma, PAGE_SIZE)){
        return NULL;
    }

    // Allocate physical page
    void *physical_page = memory_alloc_page();
    if(!physical_page) {
        panic("Out of physical memory!");
        return NULL;
    }

    // Traverse or create page tables for the virtual address
    struct pte *pte = walk_pt(active_space_root(), vma, 1);
    if (!pte) {
        memory_free_page(physical_page);
        return NULL;
    }

    // Set up the leaf PTE to point to the allocated physical page
    *pte = leaf_pte(physical_page, rwxug_flags);

    // Flush TLB to ensure new mapping is recognized
    sfence_vma();
    
    // Return mapped virtual address
    return (void *) vma;
}

void memory_handle_page_fault(const void * vptr){
    // Ensure vma is well-formed
    if (!wellformed_vma((uintptr_t)vptr) || !aligned_ptr(vptr, PAGE_SIZE)){
        console_printf("Page fault at invalid virtual address: %p\n", vptr);
        panic("Page fault: Address validation failed");
        return;
    }
    
    // Check if address is within user region
    if ((uintptr_t)vptr < USER_START_VMA || (uintptr_t)vptr >= USER_END_VMA){
        console_printf("Page fault, address %p is outside user region\n", vptr);
        panic("Page fault: Address not in user region");
        return;
    }

    // Allocate and map a new page
    void *new_page = memory_alloc_page();
    if(!new_page){
        console_printf("Out of physical memory for address: %p\n", vptr);
        panic("Page fault: Out of physical memory");
        return;
    }

    // Map new page to faulting virtual address with the appropriate permissions
    if (!memory_alloc_and_map_page((uintptr_t)vptr, PTE_R | PTE_W | PTE_U)){
        console_printf("Failed to map address: %p\n", vptr);
        panic("Page Fault: Failed to map address");
        return;
    }

    kprintf("Page fault handled: mapped new page for address %p\n", vptr);
}
// helper function

/**
 * walks the page table to find or create the pte corresponding to a virtual address
 * 
 * this function traverses the page table hierarchy starting from the root and locates
 * the pte that maps the 4kb page containing the given vma. If create is non-zero the 
 * function will allocate a new page table(s) as needed to complete the walk down to the 
 * leaf level (level 0). This function does not map mega or giga pages. 
 * 
 * @param root      pointer to the root page table
 * @param vma       virtual memory address for which the PTE is sought
 * @param create    if non-zero, indicates that missing pts should be created
 * 
 * @return          returns a pointer to the pte containing vma
 *                  if the pte can't be found or created returns NULL
 */

struct pte* walk_pt(struct pte* root, uintptr_t vma, int create) {
    struct pte* pt = root;

    // virtual page number bits
    uint64_t vpn[3];
    vpn[0] = VPN0(vma);
    vpn[1] = VPN1(vma);
    vpn[2] = VPN2(vma);

    // walk down the page table starting from the highest level (ie level 2)
    for (int level = 2; level > 0; level--) {
        // grab the page table entry of the next level
        struct pte* pte = &pt[vpn[level]];

        // check if the entry is valid
        if (pte->flags & PTE_V) {
            // if pte has flags r=0, w=0, and x=0, pte refers to next level
            if (pte->flags & (PTE_R | PTE_W | PTE_X)) {
                // leaf pte encountered at a non-leaf level, return
                return NULL;
            } else {
                // pte is valid pointing to the next level
                // grab the physical address from the pte
                uintptr_t pa = (uintptr_t)(pte->ppn << PAGE_ORDER);

                // convert it to va and have our pt pointing to it
                pt = (struct pte*)pa;
            }
        } else if (create) {
            // entry isn't valid create the entry
            // allocate a new page table
            struct pte* new_pt = (struct pte*)memory_alloc_page(); // should panic if no pages available

            // get the physical address of the new page table
            uintptr_t pa = (new_pt->ppn << 12) | (vma & 0xFFF);
            
            // set up the pte to point to the new page table
            pte->ppn = pa >> 12;
            pte->flags = PTE_V; // this might need to change
            pt = new_pt;
        } else {
            // entry isn't valid return NULL
            return NULL;
        }
    }

    // at level 0, return the pte corresponding to vpn[0]
    return &pt[vpn[0]];
}


// INTERNAL FUNCTION DEFINITIONS
//

static inline int wellformed_vma(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline int wellformed_vptr(const void * vp) {
    return wellformed_vma((uintptr_t)vp);
}

static inline int aligned_addr(uintptr_t vma, size_t blksz) {
    return ((vma % blksz) == 0);
}

static inline int aligned_ptr(const void * p, size_t blksz) {
    return (aligned_addr((uintptr_t)p, blksz));
}

static inline int aligned_size(size_t size, size_t blksz) {
    return ((size % blksz) == 0);
}

static inline uintptr_t active_space_mtag(void) {
    return csrr_satp();
}

static inline struct pte * mtag_to_root(uintptr_t mtag) {
    return (struct pte *)((mtag << 20) >> 8);
}


static inline struct pte * active_space_root(void) {
    return mtag_to_root(active_space_mtag());
}

static inline void * pagenum_to_pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline uintptr_t pageptr_to_pagenum(const void * p) {
    return (uintptr_t)p >> PAGE_ORDER;
}

static inline void * round_up_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)(p + blksz-1) / blksz * blksz);
}

static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz) {
    return ((addr + blksz-1) / blksz * blksz);
}

static inline size_t round_up_size(size_t n, size_t blksz) {
    return (n + blksz-1) / blksz * blksz;
}

static inline void * round_down_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)p / blksz * blksz);
}

static inline size_t round_down_size(size_t n, size_t blksz) {
    return n / blksz * blksz;
}

static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz) {
    return (addr / blksz * blksz);
}

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags)
{
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pageptr_to_pagenum(pptr)
    };
}

static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag)
{
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pageptr_to_pagenum(ptab)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}

static inline void sfence_vma(void) {
    asm inline ("sfence.vma" ::: "memory");
}

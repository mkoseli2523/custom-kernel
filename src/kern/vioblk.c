//           vioblk.c - VirtIO serial port (console)
//          

#include "virtio.h"
#include "intr.h"
#include "halt.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"

//           COMPILE-TIME PARAMETERS
//          

#define VIOBLK_IRQ_PRIO 1

//           INTERNAL CONSTANT DEFINITIONS
//          

//           VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

//           INTERNAL TYPE DEFINITIONS
//          

//           All VirtIO block device requests consist of a request header, defined below,
//           followed by data, followed by a status byte. The header is device-read-only,
//           the data may be device-read-only or device-written (depending on request
//           type), and the status byte is device-written.

struct vioblk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

//           Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1

//           Status byte values

#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

//           Main device structure.
//          
//           FIXME You may modify this structure in any way you want. It is given as a
//           hint to help you, but you may have your own (better!) way of doing things.

struct vioblk_device {
    volatile struct virtio_mmio_regs * regs;
    struct io_intf io_intf;
    uint16_t instno;
    uint16_t irqno;
    int8_t opened;
    int8_t readonly;

    //           optimal block size
    uint32_t blksz;
    //           current position
    uint64_t pos;
    //           sizeo of device in bytes
    uint64_t size;
    //           size of device in blksz blocks
    uint64_t blkcnt;

    struct {
        //           signaled from ISR
        struct condition used_updated;

        //           We use a simple scheme of one transaction at a time.

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        //           The first descriptor is an indirect descriptor and is the one used in
        //           the avail and used rings. The second descriptor points to the header,
        //           the third points to the data, and the fourth to the status byte.

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status;
    } vq;

    //           Block currently in block buffer
    uint64_t bufblkno;
    //           Block buffer
    char * blkbuf;
};

//           INTERNAL FUNCTION DECLARATIONS
//          

static int vioblk_open(struct io_intf ** ioptr, void * aux);

static void vioblk_close(struct io_intf * io);

static long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz);

static long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n);

static int vioblk_ioctl (
    struct io_intf * restrict io, int cmd, void * restrict arg);

static void vioblk_isr(int irqno, void * aux);

// define a struct that contains pointers to our driver functions

static const struct io_ops vioblk_io_ops = {
    .close = vioblk_close,
    .read = vioblk_read,
    .write = vioblk_write,
    .ctl = vioblk_ioctl
};

//           IOCTLs

static int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr);
static int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr);
static int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr);
static int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr);

//           EXPORTED FUNCTION DEFINITIONS
//          

//           Attaches a VirtIO block device. Declared and called directly from virtio.c.

// void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno);
//
// initializes virtio block device with the necessary IO operation functions adn sets the required
// feature bits. argument regs is the mmio registers for the given block device. argument irqno
// is the interrupt request no for the given block device. 
// 
// this function should be used to register the block device. it sets feature bits, initializes 
// device fields, and virtque, attaches the virtque to the device, registers the device with the 
// OS and the ISR. 

void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed
    console_printf("vioblk_attach called\n");
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device * dev;
    uint_fast32_t blksz;
    int result;

    assert (regs->device_id == VIRTIO_ID_BLOCK);

    //           Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    //           fence o,io
    __sync_synchronize();

    //           Negotiate features. We need:
    //            - VIRTIO_F_RING_RESET and
    //            - VIRTIO_F_INDIRECT_DESC
    //           We want:
    //            - VIRTIO_BLK_F_BLK_SIZE and
    //            - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    // virtio_featset_add(needed_features, VIRTIO_F_EVENT_IDX); 
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs, enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    //           If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    //           Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);
    memset(dev, 0, sizeof(struct vioblk_device));
    
    // if (virtio_featset_test(enabled_features, VIRTIO_F_EVENT_IDX)) 
    //     dev->vq.avail.flags = 0;
    // else
    //     console_printf("device doesn't want notifications \n");

    // perform device-specific setup
    // initialize device fields
    dev->regs = regs;
    dev->irqno = irqno;
    dev->blksz = blksz;
    dev->opened = 0;
    dev->readonly = 0;
    dev->pos = 0;
    dev->bufblkno = (uint64_t)(-1);
    __sync_synchronize();

    uint64_t capacity = regs->config.blk.capacity;
    dev->size = capacity * 512;
    dev->blkcnt = dev->size / dev->blksz;
    // console_printf("capacity: %d", dev->blkcnt);

    // initialize block buffer
    dev->blkbuf = kmalloc(blksz * sizeof(char));
    if (!dev->blkbuf) {
        kprintf("Failed to allocate block buffer\n");
        kfree(dev);
        return;
    }

    // initializing virtq
    // select the queue (first queue is 0)
    regs->queue_sel = 0;
    __sync_synchronize();
    
    // check if the queue is already in use: 
    // read QueueReady, and expect a returned value of one

    // read maximum queue size
    // if returned value is zero queue is not available

    // allocate and zero the queue memory
    // queue memory is already included in device. can skip this step

    // notify the device about the queue size

    // initialize condition variable
    condition_init(&dev->vq.used_updated, "vioblk_used_updated");

    // initialize the virtqueue descriptors
    // descriptor 0: indirect descriptor
    dev->vq.desc[0].addr = (uint64_t)&dev->vq.desc[1];
    dev->vq.desc[0].len = sizeof(struct virtq_desc) * 3;
    dev->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT;
    dev->vq.desc[0].next = 1;

    // descriptor 1: request header
    dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header;
    dev->vq.desc[1].len = sizeof(struct vioblk_request_header);
    dev->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;
    dev->vq.desc[1].next = 2;

    // descriptor 2: data buffer
    dev->vq.desc[2].addr = (uint64_t)&dev->blkbuf;
    dev->vq.desc[2].len = blksz;
    dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    dev->vq.desc[2].next = 3;

    // descriptor 3: status byte
    dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status;
    dev->vq.desc[3].len = sizeof(uint8_t);
    dev->vq.desc[3].flags = VIRTQ_DESC_F_WRITE;
    dev->vq.desc[3].next = 0;

    // attach the virtqueue to the device
    virtio_attach_virtq(regs, 0, 1, (uint64_t)&dev->vq.desc[0], (uint64_t)&dev->vq.used, (uint64_t)&dev->vq.avail);

    // write 0x1 to QueueReady
    regs->queue_ready = 1; 
    __sync_synchronize();

    // initialize I/O interface
    dev->io_intf.ops = &vioblk_io_ops;

    // register the device with the OS
    uint16_t instno = device_register("blk", &vioblk_open, dev);

    if (instno < 0) {
        kprintf("failed to register vioblk device\n");
        kfree(dev->blkbuf);
        kfree(dev);
        return;
    }

    dev->instno = instno;

    // register isr
    intr_register_isr(irqno, VIOBLK_IRQ_PRIO, vioblk_isr, dev);

    // final step: signal the device that the driver is ready
    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    //           fence o,oi
    __sync_synchronize();

    debug("%p: vioblk device attached successfully\n", regs);
}

// int vioblk_open(struct io_intf ** ioptr, void * aux);
//
// sets the virtq_avail and virtq_used such that they are available for use
// argument ioptr returns the io operations, argument aux is the pointer to the
// device. returns 0 in success
// 
// should be used to open a device. it enables the interrupt line for the virtio
// device and sets necessary flags in vioblk_device

int vioblk_open(struct io_intf ** ioptr, void * aux) {
    struct vioblk_device * dev = (struct vioblk_device *)aux;

    // check if the device is already opened
    if (dev->opened) {
        return -EBUSY;
    }

    // initialize the avail ring
    dev->vq.avail.flags = 0;
    dev->vq.avail.idx = 0;
    dev->vq.avail.ring[0] = 0;

    // initialize the used ring
    dev->vq.used.flags = 0;
    dev->vq.used.idx = 0;
    dev->vq.used.ring[0].id = 0;
    dev->vq.used.ring[0].len = 0;
    __sync_synchronize();

    virtio_enable_virtq(dev->regs, dev->regs->queue_num);
    __sync_synchronize();
    virtio_notify_avail(dev->regs, dev->regs->queue_num);

    // enabling interrupt line for virtio device
    // clear any pending interrupts
    dev->regs->interrupt_ack = dev->regs->interrupt_status;
    __sync_synchronize();

    intr_enable_irq(dev->irqno);

    // reset the device position to the beginning
    dev->pos = 0;
    dev->bufblkno = (uint64_t)(-1);

    // return the io interface through ioptr
    *ioptr = &dev->io_intf;

    // mark device as opened
    dev->opened = 1;

    return 0;
}

//           Must be called with interrupts enabled to ensure there are no pending
//           interrupts (ISR will not execute after closing).

// void vioblk_close(struct io_intf * io);
//
// resets the virtq_avail and virtq_used queeus and sets necessary flags in vioblk_device
// arg io is the pointer to the io_intf of the block device
//
// should be used to close the device

void vioblk_close(struct io_intf * io) {
    struct vioblk_device *dev = (void *)io - offsetof(struct vioblk_device, io_intf);

    // reset the avail ring 
    dev->vq.avail.idx = 0;
    dev->vq.avail.flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    
    // reset the used ring
    dev->vq.used.idx = 0;
    dev->vq.used.flags = 0;

    // invalidate any buffered block data
    dev->bufblkno = (uint64_t)(-1);

    // reset the device position to the beginning
    virtio_reset_virtq(dev->regs, dev->regs->queue_num);

    // mark the device as not opened
    dev->opened = 0;

    // select the virtqueue
    dev->regs->queue_sel = 0;
    __sync_synchronize();

    // reset the virtqueue by clearing queue_ready
    dev->regs->queue_ready = 0;
    __sync_synchronize();
}

// long vioblk_read(struct io_intf * restrict io,
//                  void * restrict buf,
//                  unsigned long bufsz);
//
// Reads bufsz number of bytes from the disk and writes them to buf. Achieves this by repeatedly
// setting the appropriate registers to request a block from the disk, waiting until the data has been
// populated in block buffer cache, and then writes that data out to buf. argument io points to the
// io of the device given, buf is the buffer to read and bufsz is how many bytes of data we want to read
//
// Thread sleeps while waiting for the disk to service the request. Returns the number of bytes
// successfully read from the disk.

int vioblk_read_block(struct vioblk_device * dev, uint64_t blkno);

long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz)
{
    struct vioblk_device * dev = (void *)io - offsetof(struct vioblk_device, io_intf);
    long total_read = 0;
    
    while (bufsz > 0) {
        // check if we reached the end of the device
        if (dev->pos >= dev->size) {
            // end of device
            return 0;
        }

        // determine the current block number and offset within the block
        uint64_t blkno = dev->pos / dev->blksz;
        uint32_t blkoff = dev->pos % dev->blksz;

        // console_printf("%d, %d, %d\n", blkoff, blkno, dev->pos);

        // determine how many bytes we can read from this block
        uint32_t bytes_in_block = dev->blksz - blkoff;
        uint32_t to_read = (bufsz < bytes_in_block) ? bufsz : bytes_in_block;

        // ensure we are not exceeding device size
        to_read = (dev->pos + to_read > dev->size) ? dev->size - dev->pos : to_read;

        // check if the block is already in the block buffer
        if (dev->bufblkno != blkno) {
            // console_printf("made it inside\n");
            int result = vioblk_read_block(dev, blkno);

            // check if there was an error
            if (result != 0) {
                return (total_read > 0) ? total_read : result;
            }
        }
        
        console_printf("buf: %u\nblkbuf: %u\n", buf + total_read, dev->blkbuf);
        memcpy(buf + total_read, dev->blkbuf, to_read);

        // console_printf("%d to_read\n", to_read);
        // console_printf("%d total_read\n", total_read);

        // for (int i = 0; i < dev->blksz; i += 128) {
        //     console_printf("%d", *(char *)(buf + i));
        // }
        
        dev->pos += to_read;
        bufsz -= to_read;
        total_read += to_read;
        dev->bufblkno = blkno;
    }

    return total_read;
}

// int vioblk_read_block(struct vioblk_device * dev, uint64_t blkno);
//
// helper function that reads a block at a time from the device buffer args are
// the pointer to the device being read and the block number being read

int vioblk_read_block(struct vioblk_device * dev, uint64_t blkno) {
    // prepare request header
    dev->vq.req_header.type = VIRTIO_BLK_T_IN;
    dev->vq.req_header.reserved = 0;
    dev->vq.req_header.sector = (blkno * dev->blksz) / 512;
    console_printf("sector: %d\n", dev->vq.req_header.sector);

    // ensure data buffer descriptor has the correct flags
    // descriptor 0: indirect descriptor

    // descriptor 1: request header
    // dev->vq.desc[1].addr = (uint64_t)&dev->vq.req_header;

    // descriptor 2: data buffer
    dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;

    // descriptor 3: status byte
    // dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status;

    // increment avail idx
    uint16_t avail_idx = dev->vq.avail.idx;
    dev->vq.avail.ring[avail_idx % 1] = 0;
    __sync_synchronize();
    dev->vq.avail.idx += 1;
    __sync_synchronize();

    virtio_notify_avail(dev->regs, dev->regs->queue_num);

    // console_printf("used: %d \navail: %d\n", dev->vq.used.idx, dev->vq.avail.idx);

    uint64_t intr_state = intr_disable();
    // while (dev->vq.used.idx != dev->vq.avail.idx) {
        condition_wait(&dev->vq.used_updated);
    // }
    intr_restore(intr_state);
    __sync_synchronize();

    return 0;
}

// long vioblk_write (
//    struct io_intf * restrict io,
//    const void * restrict buf,
//    unsigned long n);
//
// Writes n number of bytes from the parameter buf to the disk. The size of the virtio device should
// not change. You should only overwrite existing data. Write should also not create any new files.
// Achieves this by filling up the block buffer cache and then setting the appropriate registers to request
// the disk write the contents of the cache to the specified block location. arg io points to the device
// arg buf contains the data to be written, and arg n is the # of bytes to write
//
// Thread sleeps while waiting for the disk to service the request. Returns the number of bytes 
// successfully written to the disk.

int vioblk_write_block(struct vioblk_device * dev, uint64_t blkno);

long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n)
{
    struct vioblk_device *dev = (void *)io - offsetof(struct vioblk_device, io_intf);
    long total_written = 0;

    if (dev->readonly) {
        return -EINVAL;
    }

    while (n > 0) {
        if (dev->pos >= dev->size) {
            break;
        }

        // determine the current block number and offset within the block
        uint64_t blkno = dev->pos / dev->blksz;
        uint32_t blkoff = dev->pos % dev->blksz;

        // determine how many bytes we can read from this block
        uint32_t bytes_in_block = dev->blksz - blkoff;
        uint32_t to_write = (n < bytes_in_block) ? n : bytes_in_block;

        // ensure we are not exceeding device size
        to_write = (dev->pos + to_write > dev->size) ? dev->size - dev->pos : to_write;
        // console_printf("to_write: %d\n", blkno);

        // copy data from the user buffer to the block buffer
        console_printf("buf: %u\nblkbuf: %u\n", buf + total_written, dev->blkbuf);
        memcpy(dev->blkbuf, buf + total_written, to_write);

        // for (int i = 0; i < dev->blksz; i += 128) {
        //     console_printf("%d ", *(dev->blkbuf + blkoff + i));
        //     console_printf("%d\n", *(char *)(buf + total_written + i));
        // }

        // write the block buffer to the device
        console_printf("blkno: %d\npos: %d\nblkcnt: %d\n", blkno, dev->pos, dev->blkcnt);
        int result = vioblk_write_block(dev, blkno);

        // check if an error has occured
        if (result != 0) {
            return (total_written > 0) ? total_written : result;
        }
        dev->bufblkno = blkno;

        // update positions
        dev->pos += to_write;
        n -= to_write;
        total_written += to_write;
    }

    return total_written;
}

// helper function to write to the device
// thread sleeps while waiting for the device to process our request

int vioblk_write_block(struct vioblk_device * dev, uint64_t blkno) {
    // prepare the request header
    dev->vq.req_header.type = VIRTIO_BLK_T_OUT;
    dev->vq.req_header.reserved = 0;
    dev->vq.req_header.sector = (blkno * dev->blksz) / 512;
    // console_printf("sector: %d\n", dev->vq.req_header.sector);

    // clear the flag for the data buffer
    // descriptor 0: indirect descripto

    // descriptor 1: request header
    // dev->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;

    // descriptor 2: data buffer
    dev->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;

    // descriptor 3: status byte
    // dev->vq.desc[3].addr = (uint64_t)&dev->vq.req_status;

    // add the descriptor chain to the avail ring
    uint16_t avail_idx = dev->vq.avail.idx;
    dev->vq.avail.ring[avail_idx % 1] = 0;
    __sync_synchronize();
    dev->vq.avail.idx += 1;
    __sync_synchronize();

    // console_printf("used: %d \navail: %d\n", dev->vq.used.idx, dev->vq.avail.idx);

    // notify the device
    // console_printf("dev->regs->queue_num: %d\n", dev->regs->queue_num);
    virtio_notify_avail(dev->regs, dev->regs->queue_num);
    
    // wait for request to complete
    uint64_t intr_state = intr_disable();
    // while (dev->vq.used.idx != dev->vq.avail.idx) {
        condition_wait(&dev->vq.used_updated);
    // }
    intr_restore(intr_state);
    __sync_synchronize();

    return 0;
}

int vioblk_ioctl(struct io_intf * restrict io, int cmd, void * restrict arg) {
    struct vioblk_device * const dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);
    
    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);
    
    switch (cmd) {
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

// void vioblk_isr(int irqno, void * aux);
//
// Sets the appropriate device registers and wakes the thread up after waiting 
// for the disk to finish servicing a request. aux points to the device
// and irqno is the interrupt request no. 

void vioblk_isr(int irqno, void * aux) {
    struct vioblk_device * dev = (struct vioblk_device *)aux;

    // read the interrupt status register to determine the cause of the interrupt
    uint32_t interrupt_status = dev->regs->interrupt_status;

    // handle virtqueue interrupts
    if (interrupt_status & 0x1) {
        condition_broadcast(&dev->vq.used_updated);   
        // write to acknowledge register
        dev->regs->interrupt_ack = interrupt_status;
        __sync_synchronize();
    }
}

// int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr);
//
// Ioctl helper function which provides the device size in bytes. arg dev points
// to the device. arg lenptr points to the len. returns 0 on success

int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr) {
    // check if arguments are valid
    if (!dev || !lenptr) {
        return -EINVAL;
    }

    *lenptr = dev->size;

    return 0;
}

// int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr);
//
// Ioctl helper function which gets the current position in the disk which is currently 
// being written to or read from. arg dev points to the device, arg posptr points
// to the position. returns 0 on success

int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr) {
    // check if arguments are valid
    if (!dev || !posptr) {
        return -EINVAL;
    }

    // retrieve current pos
    *posptr = dev->pos;

    return 0;
}

// int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr);
//
// Ioctl helper function which sets the current position in the disk which is currently 
// being written to or read from. arg dev points to the device, and arg posptr is the
// pointer to the position. returns 0 on success

int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr) {
    // check if arguments are valid
    if (!dev || !posptr) {
        return -EINVAL;
    }

    uint64_t new_pos = *posptr;

    // check if the new position is within the device size
    if (new_pos > dev->size) {
        return -EINVAL;
    }

    // update the device's current position
    dev->pos = new_pos;

    return 0;
}

// int vioblk_getblksz(const struct vioblk_device * dev, uint32_t * blkszptr);
// 
// helper function which provides the device block size argument dev points to the 
// block device, argument blkszptr points to the block size. returns 0 on success

int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr)
{
    // check if arguments are valid
    if (!dev || !blkszptr) {
        return -EINVAL;
    }

    // write the block size to the provided pointer
    *blkszptr = dev->blksz;

    // success return 0
    return 0;
}

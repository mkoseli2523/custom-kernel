// kfs.c 
//

#include "io.h"
#include "fs.h"
#include "device.h"
#include "string.h"
#include "halt.h"
#include "error.h"
#include "memory.h"

// constant definitions
#define FS_BLKSZ      4096
#define FS_NAMELEN    32
#define FS_MAXOPEN    32

// internal type definitions
// Disk layout:
// [ boot block | inodes | data blocks ]

typedef struct dentry_t{
    char file_name[FS_NAMELEN];
    uint32_t inode;
    uint8_t reserved[28];
}__attribute((packed)) dentry_t; 

typedef struct boot_block_t{
    uint32_t num_dentry;
    uint32_t num_inodes;
    uint32_t num_data;
    uint8_t reserved[52];
    dentry_t dir_entries[63];
}__attribute((packed)) boot_block_t;

typedef struct inode_t{
    uint32_t byte_len;
    uint32_t data_block_num[1023];
}__attribute((packed)) inode_t;

typedef struct data_block_t{
    uint8_t data[FS_BLKSZ];
}__attribute((packed)) data_block_t;

// file struct. see 7.2 in cp1 docs

struct file_struct {
    struct io_intf io;
    uint64_t file_position;
    uint64_t file_size;
    uint64_t inode_number;
    uint64_t flags;  
};

// internal function definitions
int fs_mount(struct io_intf* blkio);
int fs_open(const char* name, struct io_intf** ioptr);
void fs_close(struct io_intf* io);
long fs_write(struct io_intf* io, const void* buf, unsigned long n);
long fs_read(struct io_intf* io, void* buf, unsigned long n);
int fs_ioctl(struct io_intf* io, int cmd, void* arg);
int fs_getlen(struct file_struct* fd, void* arg);
int fs_getpos(struct file_struct* fd, void* arg);
int fs_setpos(struct file_struct* fd, void* arg);
int fs_getblksz(struct file_struct* fd, void* arg);

// struct that contains the pointers to our fs functions
static const struct io_ops fs_io_ops = {
    .close = fs_close,
    .read = fs_read,
    .write = fs_write,
    .ctl = fs_ioctl
};

// global variables
struct io_intf * vioblk_io;
char fs_initialized; 
struct boot_block_t boot_block;
struct file_struct file_structs[FS_MAXOPEN];
inode_t inode;
data_block_t data_block;

// int fs_mount(struct io_intf * blkio);
//
// sets up the file system for future fs_open operations. Takes in an argument
// of type io_intf * that points to the filesystem provider, sets up the file system
// and returns whether the function was successful (0) or unsuccessfull (error code) 

int fs_mount(struct io_intf* blkio) {
    // store the block device interface
    vioblk_io = blkio;

    // check if fs has already been initialized
    if (fs_initialized) {
        console_printf("fs_is already initialized\n");
        return -1;
    }

    // set position to the beginning of io device
    uint64_t offset = 0;
    if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &offset) != 0) {
        console_printf("issue setting block device offset to 0\n");
        return -1;
    }

    // attempt to read bootblock
    if (ioread(blkio, (void *)&boot_block, FS_BLKSZ) < 0) {
        console_printf("error: failed to read bootblock\n");
        return -1;
    }

    console_printf("boot block read successfully, inodes: %u, data blocks: %u\n", boot_block.num_inodes, boot_block.num_data);

    // mark fs as initialized
    fs_initialized = 1;
    
    // init file structs array
    memset(file_structs, 0, sizeof(file_structs));
    return 0;
}



// int fs_open(const char* name, struct io_intf** io);
//
// takes the name of the file to be opened and modifies the given pointer to contain the io_intf
// of the file. this function also associates a specific file struct with the file and 
// marks it as in-use. the user program will use io_intf to interact with the file 

int fs_open(const char* name, struct io_intf** ioptr) {
    // check if file system is initialized before calling open
    if (!fs_initialized) {
        console_printf("filesystem not initialized\n");
        return -1;
    }

    // new file
    struct file_struct * file = NULL;

    for (int i = 0; i < FS_MAXOPEN; i++) {
        if (file_structs[i].flags == 0) {
            file = &file_structs[i];
            console_printf("found available slot at index %d\n", i);
            break;
        }
    }

    // check if we found a valid file slot
    if (file == NULL) {
        console_printf("no available file slots\n");
        return -1;
    }

    // search for file in directory entries
    struct dentry_t * dentry = NULL;
    
    for (int i = 0; i < boot_block.num_dentry; i++) {
        // ensure file name is null terminated
        char fname[FS_NAMELEN + 1];
        strncpy(fname, boot_block.dir_entries[i].file_name, FS_NAMELEN);
        fname[FS_NAMELEN] = '\0';

        // compare the provided name with the directory entry
        if (strncmp(name, fname, FS_NAMELEN) == 0) {
            dentry = &boot_block.dir_entries[i];
            break;
        }
    }

    if (!dentry) {
        console_printf("file not found in directory entries\n");
        return -1;
    }

    // set file position
    file->file_position = 0;
    file->inode_number = dentry->inode;

    // set position to inode start
    uint64_t inode_pos = FS_BLKSZ + file->inode_number * FS_BLKSZ;
    if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &inode_pos) != 0) {
        console_printf("can't set file position\n");
        return -1;
    }

    // read inode data
    uint64_t bytes_read = vioblk_io->ops->read(vioblk_io, &inode, sizeof(struct inode_t));
    if (bytes_read != sizeof(struct inode_t)) {
        console_printf("can't read inode\n");
        return -1;
    }

    // initialize file structure with inode data
    file->file_size = inode.byte_len;
    file->flags = 1;                        // mark file as in use
    file->io.ops = &fs_io_ops;
    *ioptr = &file->io;
    
    // succesfully opened file return 0
    console_printf("file opened successfully. file position: %d file size: %d inode number: %d\n", 
                                    file->file_position, file->file_size, file->inode_number);
    return 0;
}



// void fs_close(struct intf_t * io);
//
// marks the file struct associated with io as unused. takes an input to the io that we want to close.
// the function accomplishes this by going through the list of open files and marking the file as unused
// once it is found

void fs_close(struct io_intf* io) {
    for (int i = 0; i < FS_MAXOPEN; i++) {
        if (&file_structs[i].io == io) {
            file_structs[i].flags = 0;
            break;
        }
    }

    return;
}



// long fs_write(struct io_intf* io, const void* buf, unsigned long n);
//
// writes n bytes from the buf into the file associated with io. takes in three arguments:
// pointer to the io, pointer to buffer that contains the data to be written, and unsigned 
// long n that shows how many bytes we are going to write. returns number of bytes written
// if successfull otherwise returns a negative value. 
// size of the file does not change, this function only overwrites existing data. it also does not
// create any new files. updates metadata in the file struct as appropriate

long fs_write(struct io_intf* io, const void* buf, unsigned long n) {
    // make sure the parameters are valid
    if (!io || !buf) {
        return -1;
    }

    // retrieve file struct from io_intf
    struct file_struct* file = (struct file_struct*)((char*)io - offsetof(struct file_struct, io));

    // ensure the file struct is valid and in use
    if (file->flags == 0) {
        return -2; 
    }

    // make sure the file system is initialized
    if (!fs_initialized) {
        return -3; 
    }

    // check if we are at the end of a file
    if (file->file_position >= file->file_size) {
        return 0; 
    }

    // make sure n does not exceed the number of bytes in the file
    if (file->file_position + n > file->file_size) {
        n = file->file_size - file->file_position;
    }

    // read the inode associated with the file
    uint32_t inode_number = file->inode_number;

    // calculate the inode's offset in the filesystem
    uint64_t inode_offset = FS_BLKSZ + (inode_number * FS_BLKSZ);

    // read the inode
    if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &inode_offset) != 0) {
        return -1; // Error setting position
    }

    long bytes_read = vioblk_io->ops->read(vioblk_io, &inode, sizeof(inode_t));
    if (bytes_read != sizeof(inode_t)) {
        return -5; 
    }

    // initialize variables for reading the data
    unsigned long total_bytes_written = 0;
    unsigned long bytes_to_write = n;
    uint64_t file_pos = file->file_position;
    long bytes_written;

    while (bytes_to_write > 0) {
        // calculate the current data block index and the index within the block
        uint32_t block_index = file_pos / FS_BLKSZ;
        uint32_t block_offset = file_pos % FS_BLKSZ;

        // check if block index exceeds the max number of blocks allowed
        if (block_index >= sizeof(struct inode_t)) {
            break;
        }

        // get the data block number
        uint32_t data_block_num = inode.data_block_num[block_index];

        // calculate the offset of the data block in the filesystem
        uint64_t data_block_offset = FS_BLKSZ                             // boot block size
                                   + (boot_block.num_inodes * FS_BLKSZ)   // total inode size
                                   + (data_block_num * FS_BLKSZ)          // data block offset
                                   + block_offset;                        // mem offset

        // write to the data block
        if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &data_block_offset) != 0) {
            return -6; 
        }

        // calculate how many bytes we can write to this block
        unsigned long bytes_available = FS_BLKSZ - block_offset;
        unsigned long bytes_this_write = (bytes_to_write < bytes_available) ? bytes_to_write : bytes_available;

        // copy the data to the buffer
        memcpy(data_block.data, (char*)buf + total_bytes_written, bytes_this_write);

        bytes_written = vioblk_io->ops->write(vioblk_io, &data_block, bytes_this_write);
        if (bytes_written != bytes_this_write) {
            return -7; 
        }

        // update counters
        total_bytes_written += bytes_this_write;
        bytes_to_write -= bytes_this_write;
        file_pos += bytes_this_write;
    }

    // update file position
    file->file_position = file_pos;

    // return the number of bytes read
    return total_bytes_written; 
}



// long fs_read(struct io_intf* io, void* buf, unsigned long n)
// 
// reads n bytes from the file associated with io into buf. takes in 3 arguments pointer to the io,
// pointer to the buffer that will eventually contain the read data, and the size of the memory 
// we want to read. if successful returns 0 else returns negative value
// updates metadata as appropriate. use fs_open to get io

long fs_read(struct io_intf* io, void* buf, unsigned long n)
{
    // make sure the parameters are valid
    if (!io || !buf) {
        return -1; 
    }

    // retrieve file struct from io_intf
    struct file_struct* file = (struct file_struct*)((char*)io - offsetof(struct file_struct, io));

    // ensure the file struct is valid and in use
    if (file->flags == 0) {
        return -1; 
    }

    // make sure the file system is initialized
    if (!fs_initialized) {
        return -1; 
    }

    // check if we are at the end of a file
    if (file->file_position >= file->file_size) {
        return 0; 
    }

    // make sure n does not exceed the number of bytes in the file
    if (file->file_position + n > file->file_size) {
        n = file->file_size - file->file_position;
    }

    // read the inode associated with the file
    uint32_t inode_number = file->inode_number;

    // calculate the inode's offset in the filesystem
    uint64_t inode_offset = FS_BLKSZ + (inode_number * FS_BLKSZ);

    // read the inode
    if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &inode_offset) != 0) {
        return -1; // Error setting position
    }

    long bytes_read = vioblk_io->ops->read(vioblk_io, &inode, sizeof(inode_t));
    if (bytes_read != sizeof(inode_t)) {
        return -1; 
    }

    // initialize variables for reading the data
    unsigned long total_bytes_read = 0;
    unsigned long bytes_to_read = n;
    uint64_t file_pos = file->file_position;

    while (bytes_to_read > 0) {
        // calculate the current data block index and the index within the block
        uint32_t block_index = file_pos / FS_BLKSZ;
        uint32_t block_offset = file_pos % FS_BLKSZ;

        // check if block index exceeds the max number of blocks allowed
        if (block_index >= sizeof(struct inode_t)) {
            break;
        }

        // get the data block number
        uint32_t data_block_num = inode.data_block_num[block_index];

        // calculate the offset of the data block in the filesystem
        uint64_t data_block_offset = FS_BLKSZ                             // boot block size
                                   + (boot_block.num_inodes * FS_BLKSZ)   // total inode size
                                   + (data_block_num * FS_BLKSZ);         // data block offset

        // read the data block
        if (vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &data_block_offset) != 0) {
            return -1; 
        }

        // calculate how many bytes we can read from this block
        unsigned long bytes_available = FS_BLKSZ - block_offset;
        unsigned long bytes_this_read = (bytes_to_read < bytes_available) ? bytes_to_read : bytes_available;

        bytes_read = vioblk_io->ops->read(vioblk_io, &data_block, sizeof(data_block_t));
        if (bytes_read != sizeof(data_block_t)) {
            return -1; 
        }

        // copy the data to the buffer
        memcpy(buf + total_bytes_read, data_block.data + block_offset, bytes_this_read);

        // update counters
        total_bytes_read += bytes_this_read;
        bytes_to_read -= bytes_this_read;
        file_pos += bytes_this_read;
    }

    // update file position
    file->file_position = file_pos;

    // return the number of bytes read
    return total_bytes_read; 
}



// int fs_ioctl(struct io_intf* io, int cmd, void* arg)
//
// handles control commands. takes in a pointer to the io, control command to execute, and
// a pointer to additional arguments or output data depending on the command. returns the 
// result of the helper function for the command if successful
// this function is used to perform various control operations on a file, such as retrieving
// or setting the file's position, retrieving its length, or obtaining the blksize

int fs_ioctl(struct io_intf* io, int cmd, void* arg) {
    // retrieve file struct from io_intf
    struct file_struct* file = (struct file_struct*)((char*)io - offsetof(struct file_struct, io));

    // check if the file is valid and open
    if (!file || !file->flags) {
        return -1;
    }

    // route the command to the appropriate helper function
    switch(cmd) {
        case IOCTL_GETLEN:
            return fs_getlen(file, arg);
        
        case IOCTL_GETPOS:
            return fs_getpos(file, arg);

        case IOCTL_SETPOS:
            return fs_setpos(file, arg);

        case IOCTL_GETBLKSZ:
            return fs_getblksz(file, arg);

        default:
            return -ENOTSUP;
    }
}



// int fs_getlen(struct file_struct* fd, void* arg);
//
// retrieves the size of the specified file. fd is the pointer to the file struct
// and arg points to where the length will be stored. used to obtain the size of a file

int fs_getlen(struct file_struct* fd, void* arg) {
    // check if fd and arg are valid
    if (!fd || !arg) {
        return -1;
    }

    // store the file size in the mem location pointed by arg
    *(uint64_t*)arg = fd->file_size;
    return 0;
}



// int fs_getpos(struct file_struct* fd, void* arg);
//
// retrieves the position of the specified file. fd is the pointer to the file struct
// and arg points to where the length will be stored. used to obtain the position within a file

int fs_getpos(struct file_struct* fd, void* arg) {
    // check if fd and arg are valid
    if (!fd || !arg) {
        return -1;
    }

    // store the current file position in the memory location pointed by arg
    *(uint64_t*)arg = fd->file_position;
    return 0;
}



// int fs_setpos(struct file_struct* fd, void* arg);
//
// this function is used to set the current position within a file. typically to adjust read/write
// offsets. fd is the pointer to the file struct and arg points to where the length will be stored. 
// used to set position within a file

int fs_setpos(struct file_struct* fd, void* arg) {
    // check if fd and arg are valid
    if(!fd || !arg) {
        return -1;
    }
    
    // retrieve new position
    uint64_t new_pos = *(uint64_t*)arg;

    // ensure the new position is smaller than the total file size
    if (new_pos > fd->file_size) {
        return -1;
    }

    // set the file position to the new value
    fd->file_position = new_pos;
    return 0;  
}



// int fs_getblksz(struct file_struct* fd, void* arg);
// 
// this function is used to obtain the blocksize of the filesystem, which is assumed to be 
// a constant value of 4096 bytes. 

int fs_getblksz(struct file_struct* fd, void* arg) {
    // check if fd and arg are valid pointers
    if (!fd || !arg) {
        return -1;
    }

    // store block size in memory location pointed by arg
    *(uint64_t*)arg = FS_BLKSZ;
    return 0;
}
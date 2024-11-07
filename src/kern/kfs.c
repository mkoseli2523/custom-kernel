//		kfs.c - Kernel File System Interface
//

#include "fs.h"
#include "io.h"
//#include "vioblk.c"
// #include "../util/mkfs.c" // might need to change this later if compilation issues occur - defintely need to change it
			  // read a post saying we dont need to include mkfs.c
			  // do I just copy all the structs over then?
			  // how to use iolit - dont need iolit
			  // how to interact via vioblk - dont need to interact with vioblk
// go through all the code writing all 0he important key information right


#include "kfs.h"
#include "error.h"
#include <string.h>
#include "console.h"

char fs_initialized;
struct io_intf * vioblk_io;

// defining the io_ops for the file system
struct io_ops file_io_ops = {
    .close = fs_close,
    .read = fs_read,
    .write = fs_write,
    .ctl = fs_ioctl,
}; 

// making file struct array
struct file_struct array_file_structs[32];

// boot block global var
struct boot_block_t boot_block;

// this function is used for mounting and initializing the file system for future use
// blkio io is the pointer to the block we want to copy the vioblk and the boot_block from
int fs_mount(struct io_intf * blkio) {
    vioblk_io = blkio;
    console_printf("Entering fs_mount function\n");
    // check if initialized
    if (fs_initialized) {
        console_printf("Filesystem already initialized\n");
        return -1;
    }
    struct boot_block_t *boot_block_pointer = &boot_block;
    console_printf("boot_block_pointer assigned, attempting to read boot block\n");

    // copy over to the boot block gloabl var
    long return_from_read = ioread(blkio, (void *)boot_block_pointer, sizeof(struct boot_block_t)); // might need to change size to 4096
    console_printf("Return from ioread: %ld\n", return_from_read);
    //check
    if (return_from_read < 0) {
        console_printf("Error: Failed to read boot block\n");
        return -1;
    }

    console_printf("Boot block read successfully\n");
    console_printf("Number of inodes: %u, Number of data blocks: %u\n", boot_block.num_inodes, boot_block.num_data);
    // checks
    if (boot_block.num_inodes == 0 || boot_block.num_data == 0) {
        console_printf("Error: Invalid boot block data (inodes or data blocks are zero)\n");
        return -1; // Invalid boot block data
    }

    fs_initialized = 1;
    console_printf("Filesystem mounted successfully, fs_initialized set to 1\n");

    return 0;
}

// opens a file - Takes the name of the file to be opened and modifies the given pointer to contain the io_intf of the file
// ioptr is the pointer to the iointf and name is what we are trying to match with
int fs_open(const char * name, struct io_intf ** ioptr) {
    console_printf("Entering fs_open with file name: %s\n", name);
    // checks
    if (fs_initialized != 1) {
        console_printf("Filesystem not initialized.\n");
        return -1;
    }

    // Define new file
    struct file_struct *file = NULL;
    for (int i = 0; i < 32; i++) {
        if (array_file_structs[i].flags == 0) {
            file = &array_file_structs[i];
            console_printf("Found available file slot at index %d.\n", i);
            break;
        }
    }
    // checks
    if (file == NULL) {
        console_printf("No available file slots.\n");
        return -1;  // No available file slots
    }

    // Find dentry
    struct dentry_t *dentry = NULL;
    for (int i = 0; i < boot_block.num_dentry; i++) {
        console_printf("Checking dentry index %d with file name: %s\n", i, boot_block.dir_entries[i].file_name);
        if (strncmp(boot_block.dir_entries[i].file_name, name, 32) == 0) {
            dentry = &boot_block.dir_entries[i];
            console_printf("File found in directory entries at index %d.\n", i);
            break;
        }
    }
    if (dentry == NULL) {
        console_printf("File not found in directory entries.\n");
        return -1;  // File not found
    }

    // Locate inode
    int position_diff = (4096 + (dentry->inode * sizeof(struct inode_t)));
    vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &position_diff); // set the position to the start of the inode
    inode_t inode;
    vioblk_io->ops->read(vioblk_io, &inode, sizeof(struct inode_t));

    // struct inode_t *cur_inode = (struct inode_t*)((char*)&boot_block + 4096 + dentry->inode * sizeof(struct inode_t));
    console_printf("Inode located. Inode number: %d, File size: %d bytes\n", dentry->inode, inode.byte_len);

    // Initialize file structure
    file->file_position = 0;
    file->file_size = inode.byte_len;
    file->inode_num = dentry->inode;
    file->flags = 1;
    file->io.ops = &file_io_ops;
    *ioptr = &file->io;

    console_printf("File opened successfully. File position: %d, File size: %d, Inode number: %d\n", file->file_position, file->file_size, file->inode_num);
    return 0;
}



// closes file in use
// io is the io_intf pointer for the file we are trying to close
void fs_close(struct io_intf* io){
    for(int i = 0; i < 32; i++){
	    if(&array_file_structs[i].io == io){
		    array_file_structs[i].flags = 0;
		    break;
	    }
    }
}

// writes to a certain block
// io is the pointer to file we want to write to
// buf is where we get the data from
// n is the amount of data we are copying over
long fs_write(struct io_intf* io, const void* buf, unsigned long n){
    struct file_struct *file = NULL;
    for(int i = 0; i < 32; i++){
            if(&array_file_structs[i].io == io){
                    file = &array_file_structs[i]; 
                    break;
            }
    }    
    if (file == NULL || file->flags == 0){
	    return -1;
    }
    //if (file->file_position + n > file->file_size) {
      //  n = file->file_size - file->file_position; 
    //}
    int position_diff = (4096 + (file->inode_num * sizeof(struct inode_t)));
    vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &position_diff); // set the position to the start of the inode
    inode_t inode;
    vioblk_io->ops->read(vioblk_io, &inode, sizeof(struct inode_t));
    //struct inode_t *cur_inode = (struct inode_t*)((char*)&boot_block + 4096 + file->inode_num * sizeof(struct inode_t)); // maybe should change this to 
															 // 4096 or size instead of 
															 // sizeof
    unsigned long finished_data = 0;
    unsigned long remaining_data = n;
    // file->file_position = 0;
    unsigned long position_of_file = file->file_position;
    while(remaining_data>0){
	    unsigned long offset_from_0th_data_block = position_of_file/4096;
	    unsigned long offset_in_block = position_of_file%4096;
	    unsigned long bytes_to_write;
	    //if(offset_from_0th_data_block > 1023 || offset_from_0th_data_block > cur_inode->byte_len/4096){ // not too sure about this one
	//	   return -1;
	  //  }
        if(offset_from_0th_data_block > 1023){
            return -1;
        }
        int data_block_position = 4096 + (boot_block.num_inodes * sizeof(struct inode_t)) + (inode.data_block_num[offset_from_0th_data_block] * 4096) + offset_in_block;

        // Set the position to the start of the specified data block
        vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &data_block_position);

        // Now, read the data block into a buffer
        // char data_block_buffer[4096];
        // vioblk_io->ops->read(vioblk_io, data_block_buffer, 4096);

	    //char *destination_block = (char*)((char*)&boot_block + 4096 + (boot_block.num_inodes * sizeof(struct inode_t)) +(cur_inode->data_block_num[offset_from_0th_data_block] * 4096));  // need to change this line
	    if (remaining_data < 4096 - offset_in_block) {
		    bytes_to_write = remaining_data;
	    }
	    else {
		    bytes_to_write = 4096 - offset_in_block;
	    }
        vioblk_io->ops->write(vioblk_io, buf, 4096);
	    // memcpy(data_block_buffer + offset_in_block, (char*)buf + finished_data, bytes_to_write);
	    finished_data += bytes_to_write;
	    position_of_file += bytes_to_write;
	    remaining_data -= bytes_to_write;


    }
    file->file_position = position_of_file;
    if (file->file_position > file->file_size) {
            file->file_size = file->file_position;
    }
    return finished_data;

}



// reads from a certain block
// io is the pointer to file we want to read from
// buf is where we store the data to
// n is the amount of data we are copying over
long fs_read(struct io_intf* io, void* buf, unsigned long n){
    struct file_struct *file = NULL;
    for(int i = 0; i < 32; i++){
            if(&array_file_structs[i].io == io){
                    file = &array_file_structs[i]; 
                    break;
            }
    }    
    if (file == NULL || file->flags == 0){
	    return -1;
    }
//    if (file->file_position + n > file->file_size) {
  //      n = file->file_size - file->file_position; 
   // }
    int position_diff = (4096 + (file->inode_num * sizeof(struct inode_t)));
    vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &position_diff); // set the position to the start of the inode
    inode_t inode;
    vioblk_io->ops->read(vioblk_io, &inode, sizeof(struct inode_t));
    // struct inode_t *cur_inode = (struct inode_t*)((char*)&boot_block + 4096 + file->inode_num * sizeof(struct inode_t));
    unsigned long finished_data = 0;
    unsigned long remaining_data = n;
    unsigned long position_of_file = file->file_position;
    while(remaining_data>0){
	    unsigned long offset_from_0th_data_block = position_of_file/4096;
	    unsigned long offset_in_block = position_of_file%4096;
	    unsigned long bytes_to_read;

        if(offset_from_0th_data_block > 1023){
            return -1;
        }
        // Calculate the position for the data block
        int data_block_position = 4096 + (boot_block.num_inodes * sizeof(struct inode_t)) + (inode.data_block_num[offset_from_0th_data_block] * 4096) + offset_in_block;
        // make this a unit 64 t
        // Set the position to the start of the specified data block
        vioblk_io->ops->ctl(vioblk_io, IOCTL_SETPOS, &data_block_position);
        // ioseek


        // Now, read the data block into a buffer
        char data_block_buffer[4096];
        // vioblk_io->ops->read(vioblk_io, data_block_buffer, 4096);
        

	    //char *source_block = (char*)((char*)&boot_block + 4096 + (boot_block.num_inodes * sizeof(struct inode_t)) +(cur_inode->data_block_num[offset_from_0th_data_block] * 4096));  // need to change this line 
																						  //
	    if (remaining_data < 4096 - offset_in_block) {
		    bytes_to_read = remaining_data;
	    }
	    else {
		    bytes_to_read = 4096 - offset_in_block;
	    }
        vioblk_io->ops->read(vioblk_io, buf, bytes_to_read); // changing this for now change back later

	    //memcpy( (char*)buf + finished_data, data_block_buffer + offset_in_block, bytes_to_read);
	    finished_data += bytes_to_read;
	    position_of_file += bytes_to_read;
	    remaining_data -= bytes_to_read;


    }
    file->file_position = position_of_file;
    return finished_data;

}

// get length helper function 
// fd is pointer to file struct
// arg is where we store legnth
int fs_getlen(struct file_struct *fd, void *arg) {
    if (!arg) return -1; // Check if arg is a valid pointer

    *(uint64_t*)arg = fd->file_size;
    return 0;
}
//gets position
// fd is the pointer to file struct
// arg is where we store pos
int fs_getpos(struct file_struct *fd, void *arg) {
    if (!arg) return -1;

    *(uint64_t*)arg = fd->file_position;
    return 0;
}
// sets position helper function
// fd is the pointer to file struct
// arg is where we the value for set from
int fs_setpos(struct file_struct *fd, void *arg) {
    //if (!arg) return -1;

    uint64_t new_pos = *(uint64_t*)arg;

    // Ensure the new position is within the file size
    if (new_pos > fd->file_size) {
        return -1; // Invalid position
    }

    fd->file_position = new_pos;
    return 0;
}

int fs_getblksz(struct file_struct *fd, void *arg) {
    if (!arg) return -1;

    *(uint32_t*)arg = 4096; // Assume 4096 as the block size
    return 0;
}



// the io control function with bunch of helper function
// we go to the helped function using command
// arg is the argument pointer
// io is the pointer to the io_intf
int fs_ioctl(struct io_intf *io, int cmd, void *arg){
    // find file
    struct file_struct *file = NULL;
    for(int i = 0; i < 32; i++){
            if(&array_file_structs[i].io == io){
                    file = &array_file_structs[i]; 
                    break;
            }
    }    
    // checks
    if (file == NULL || file->flags == 0){
	    return -1;
    }
    // switches to the right helper function
    switch (cmd) {
        case IOCTL_GETLEN:
            return fs_getlen(file, arg);

        case IOCTL_GETPOS:
            return fs_getpos(file, arg);

        case IOCTL_SETPOS:
            return fs_setpos(file, arg);

        case IOCTL_GETBLKSZ:
            return fs_getblksz(file, arg);

        default:
            return -ENOTSUP;  // from error.h
    }

}







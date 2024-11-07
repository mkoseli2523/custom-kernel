#ifndef KFS_H
#define KFS_H


#define FS_NAMELEN    32

#define FS_BLKSZ      4096

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

typedef struct file_struct {
    struct io_intf io;
    uint64_t file_position;
    uint64_t file_size;
    uint64_t inode_num;
    uint64_t flags;
}file_struct;
#endif

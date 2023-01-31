#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

//ext4 stuff but worth copying some over
//https://docs.kernel.org/filesystems/ext4/dynamic.html
struct inode{
    uint32_t user_id;
    uint32_t group_id;
    uint32_t mode;//permissions, file type (dir, file, special)
    uint32_t size_hi, size_lo;//size i bytes
    uint32_t access_time, change_time, data_time;//time of last access, changes to inode(ex. name), and last data modification time
    uint32_t creation_time, deletion_time;
    uint32_t links;//linked count
    uint32_t blocks_hi, blocks_lo;//num of blocks
    uint32_t direct_blocks[12], indirect_blocks, dbl_indirect, trpl_indirect;//holds block positions for blocks
    uint32_t flags;
    uint32_t checksum;


};
typedef struct inode node;


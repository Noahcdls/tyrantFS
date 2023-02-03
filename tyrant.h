#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>


#define BLOCKSIZE 4096
#define MEMSIZE 1024*1024*1024 //1GB
#define END_OF_INODE 3 //3 is first data block, 2 is last inode
#define NUM_FREE_BLOCKS 64


//ext4 stuff but worth copying some over
//https://docs.kernel.org/filesystems/ext4/dynamic.html
struct inode{
    uint32_t user_id;
    uint32_t group_id;
    uint32_t mode;//permissions, file type (dir, file, special)
    uint32_t size_hi;
    uint32_t size_lo;//size i bytes
    uint32_t access_time;
    uint32_t change_time;
    uint32_t data_time;//time of last access, changes to inode(ex. name), and last data modification time
    uint32_t creation_time;
    uint32_t deletion_time;
    uint32_t links;//linked count
    uint32_t blocks_hi;
    uint32_t blocks_lo;//num of blocks
    uint32_t direct_blocks[12];
    uint32_t indirect_blocks;
    uint32_t  dbl_indirect;
    uint32_t trpl_indirect;//holds block positions for blocks
    uint32_t flags;
    uint32_t checksum;


};
typedef struct inode node;
 
#define DIRECTORY 0x4000
#define FILE      0x8000




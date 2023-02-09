#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
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
 
#define INODE_SIZE_BOUNDARY (uint32_t)pow(2, ceil(log(sizeof(node))/log(2)))

#define IFIFO     0x1000  //FIFO
#define IFCHR     0x2000  //Character Device
#define IFDIR     0x4000  //Directory
#define IFBLK     0x6000  //Block device
#define IFREG     0x8000  //regular file
#define IFLNK     0xA000  //Symbolic link
#define IFSOCK    0xC000  //Socket


int tfs_mkfs(void *fs_space);
void *allocate_inode(void *fs_space);
int free_inode(void *inode);
int read_inode(void * inode, void * buff);
int write_inode(void * inode, void * buff);
uint32_t read_block(void *buff, void* block, off_t offset, uint32_t bytes);
uint32_t write_block(void *buff, void* block, off_t offset, uint32_t bytes);
void * allocate_block(void* fs_space);
int free_block(void* fs_space, void * block);
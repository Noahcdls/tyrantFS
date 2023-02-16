#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#define BLOCKSIZE 4096
#define MEMSIZE 1024*1024*1024 //1GB
#define END_OF_INODE 3 //3 is first data block, 2 is last inode
#define NUM_FREE_BLOCKS 1024
#define ADDR_LENGTH sizeof(uint8_t*)


//ext4 stuff but worth copying some over
//https://docs.kernel.org/filesystems/ext4/dynamic.html
struct inode{
    uint32_t user_id;
    uint32_t group_id;
    uint32_t mode;//permissions, file type (dir, file, special)
    uint64_t size;//size i bytes
    uint64_t access_time;
    uint64_t change_time;
    uint64_t data_time;//time of last access, changes to inode(ex. name), and last data modification time
    uint64_t creation_time;
    uint64_t deletion_time;
    uint32_t links;//linked count
    uint64_t blocks;//number of blocks used
    uint8_t * direct_blocks[12];
    uint8_t * indirect_blocks;
    uint8_t * dbl_indirect;
    uint8_t * trpl_indirect;//holds block positions for blocks
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


node * root;
#define NAME_BOUNDARY 64


int tfs_mkfs(void *fs_space);
void *allocate_inode(void *fs_space);
int free_inode(void *inode);
int read_inode(void * inode, void * buff);
int write_inode(void * inode, void * buff);
uint32_t read_block(void *buff, void* block, off_t offset, uint64_t bytes);
uint32_t write_block(void *buff, void* block, off_t offset, uint64_t bytes);
void * allocate_block(void* fs_space);
int free_block(void* fs_space, void * block);
void * fetch_block(void * my_node, uint64_t block_no);
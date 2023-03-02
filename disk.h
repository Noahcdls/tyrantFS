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
#define ADDR_LENGTH sizeof(uint64_t)//update to uint64_t for disk since it's an offset


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
    uint64_t direct_blocks[12];
    uint64_t indirect_blocks;
    uint64_t dbl_indirect;
    uint64_t trpl_indirect;//holds block positions for blocks
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


// extern node * root_node;
extern uint64_t root_node;//offset to root
extern uint64_t drivesize;
extern uint64_t num_blocks;
extern uint64_t inode_blocks;
extern uint64_t data_blocks;
extern int drive;
#define NAME_BOUNDARY 64


int tfs_mkfs(int fd);
uint64_t allocate_inode(int fd);
int free_inode(uint64_t inode);
int read_inode(uint64_t inode, void * buff);
int write_inode(uint64_t inode, void * buff);
int read_block(void *buff, uint64_t block, off_t offset, uint64_t bytes);
int write_block(void *buff, uint64_t block, off_t offset, uint64_t bytes);
uint64_t allocate_block(int fd);
int free_block(int fd, uint64_t block);
void * fetch_block(uint64_t my_node, uint64_t block_no, void * block);
void * fetch_inode(uint64_t node, void* buff);
int commit_inode(node *my_node, uint64_t node_loc);
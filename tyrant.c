#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "include/fuse.h"
#include "tyrant.h"

uint8_t *memspace;
/*
@brief mkfs for tyrant DEFAULT
@param fs_space pointer to where beginning of memory space to write over
*/
void tfs_mkfs(void *fs_space)
{
    for (int i = 1; i < END_OF_INODE; i++)
    {
        for (int j = 0; j < BLOCKSIZE; j += 128)
        {
            node *inode_init = fs_space + BLOCKSIZE * i + j; // set up a basic node
            bzero(inode_init, 128);                          // clear out inode info
        }
    }
    *((uint32_t *)fs_space) = END_OF_INODE;                       // first superblock
    uint8_t *free_blockptr = fs_space + END_OF_INODE * BLOCKSIZE; // shift pointer
    uint32_t block_counter = END_OF_INODE;
    bzero(free_blockptr, BLOCKSIZE * 3);
    for (int i = 0; i < 3; i++)
    { // fill three blocks with addresses for fun
        for (int j = 0; j < BLOCKSIZE / 4; j++)
        {                                                   // fill entire block
            *(uint32_t *)free_blockptr = block_counter + 1; // write in next free block
            block_counter++;                                // increment next available block
            if (j == BLOCKSIZE / 4 - 1)
            {                                                         // at end before reiterate
                free_blockptr = fs_space + block_counter * BLOCKSIZE; // jump to the next block
                block_counter++;                                      // additional increment since we used block counter to hold next free block
            }
            else
            {
                free_blockptr += 4; // go to next 4 bytes in block
            }
        }
    }
}

/*
@brief read part or all of memory block
*/
uint32_t read_block(void *buff, uint32_t block, off_t offset, uint32_t bytes)
{
    uint8_t *start = memspace + block * BLOCKSIZE + offset;
    uint32_t bytes_avail = (block + 1) * BLOCKSIZE - (block * BLOCKSIZE + offset);
    if (bytes_avail < bytes)
    { // asking for more bytes than what is left so read what we can
        memcpy(buff, start, bytes_avail);
        return bytes_avail;
    }
    else
    { // can fill buffer with correct number of bytes
        memcpy(buff, start, bytes);
        return bytes;
    }
}

/*
@brief write part or all of memory block
*/
uint32_t write_block(void *buff, uint32_t block, off_t offset, uint32_t bytes)
{
    uint8_t *start = memspace + block * BLOCKSIZE + offset;
    uint32_t bytes_avail = (block + 1) * BLOCKSIZE - (block * BLOCKSIZE + offset);
    if (bytes_avail < bytes)
    { // want to write more bytes than what is left in block
        memcpy(start, buff, bytes_avail);
        return bytes_avail;
    }
    else
    { // write bytes to block
        memcpy(start, buff, bytes);
        return bytes;
    }
}

/*
@brief find next free block and allocate it
*/
uint32_t allocate_block()
{
    uint32_t free_block = 0;
    uint8_t *next_block = memspace + *(uint32_t *)memspace * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == memspace)
        return 0; // no more free blocks since next block is super block
    for (uint32_t i = 0; i < BLOCKSIZE; i += 4)
    {
        free_block = *(uint32_t *)next_block;
        if (free_block != 0)
        {
            bzero(next_block, sizeof(uint32_t)); // zero out available block
            if (i == BLOCKSIZE - 4)
            {
                free_block = *(uint32_t *)memspace;                // set free block as current block we are looking in
                *(uint32_t *)memspace = *(uint32_t *)(next_block); // last 4 bytes contain what to set next super block value
            }
            return free_block;
        }
        next_block = next_block + 4; // advance 4 bytes
    }
    free_block = *(uint32_t*) memspace;//set to block given by super block since zeroed out
    *(uint32_t*) memspace = 0;
    return free_block;//we only get here if block is filled with zeros meaning it was the last free block
}

/*
@brief add a block to the free list. Returns 0 on successful return, 1 otherwise
*/
uint32_t free_block(uint32_t block)
{
    uint32_t free_block = 0;
    uint8_t *next_block = memspace + *(uint32_t *)memspace * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == memspace)
    { // no more free blocks then add as next free block
        *(uint32_t *)memspace = block;
        bzero(memspace + block * BLOCKSIZE, BLOCKSIZE); // clear block
        return 0;                                       // success
    }
    for (uint32_t i = 0; i < BLOCKSIZE; i += 4)
    {
        free_block = *(uint32_t *)next_block;
        if (free_block == 0)//a free spot to write in a new block
        {
            *(uint32_t*)next_block = block;//write in block to free space
            if (i == BLOCKSIZE - 4)
            {
                bzero(memspace + block * BLOCKSIZE, BLOCKSIZE);//zero if we are set up to jump here
            }
            return 0;
        }
        next_block = next_block + 4; // advance 4 bytes
    }
    //if the whole block is full, make block next super block with link to the full one
    bzero(memspace+block*BLOCKSIZE, BLOCKSIZE);
    *(uint32_t*)(memspace+(block+1)*BLOCKSIZE-4) = *(uint32_t*)memspace;//write in old super block to link to
    *(uint32_t*)memspace = block;
    return 0;
}

void tfs_mkdir()
{
}
void tfs_getattr()
{
}

static struct fuse_operations operations = {
    .mkdir = tfs_mkdir,
    .getattr = tfs_getattr,
    // .readdir = tfs_readdir,
    // .rmdir = tfs_rmdir,
    // .open = tfs_open,
    // .read = tfs_read,
    // .write = tfs_write,
    // .create = tfs_create,
    // .rename = tfs_rename
    // .unlink = tfs_unlink

};

int main(int argc, char *argv[])
{
    memspace = malloc(MEMSIZE);
}

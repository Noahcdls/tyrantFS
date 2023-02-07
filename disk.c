
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "disk.h"
/*
@brief mkfs for tyrant DEFAULT
@param fs_space pointer to where beginning of memory space to write over
*/
void tfs_mkfs(void *fs_space)
{
    for (int i = BLOCKSIZE; i < END_OF_INODE * BLOCKSIZE; i += INODE_SIZE_BOUNDARY)
    {

        node *inode_init = fs_space + i; // set up a basic node
        bzero(inode_init, INODE_SIZE_BOUNDARY);          // clear out inode info
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
@brief allocate an available inode
@return memory location of inode. Returns NULL if no inode is available
*/
void *allocate_inode(void *fs_space)
{
    if (fs_space == NULL)
        return NULL;
    for (int i = BLOCKSIZE; i < END_OF_INODE * BLOCKSIZE; i += INODE_SIZE_BOUNDARY)
    {
        node *inode_ptr = fs_space + i;
        if (inode_ptr->mode == 0)
        { // cleared mode means not in use
            return inode_ptr;
        }
    }
    return NULL;
}

/*
@brief free a given inode back to the fs
@param inode Pointer to the inode that is to be freed
@return 0 for proper return, -1 for error
*/
int free_inode(void *inode)
{
    if (inode == NULL)
        return -1;
    node * node_ptr = inode;
    bzero(node_ptr, INODE_SIZE_BOUNDARY);
    return 0;
}

/*
@brief read a given inode back to a buffer
@param inode pointer to the inode that is to be read
@param buff buffer we want to write inode information to
@return 0 for proper return, -1 for error
*/
int read_inode(void * inode, void * buff){
    if(inode == NULL || buff == NULL)
        return -1;
    uint8_t * valid = memcpy(buff, inode, sizeof(node));
    if(valid != NULL)
        return 0;
    return -1;
}

/*
@brief write over an inode
@param inode pointer to the inode that is to be written to
@param buff buffer containing new inode info
@return 0 for proper return, -1 for error
@note buff must contain a full inode. 
The easiest way to write and edit is to read a copy, make changes, and write back
*/
int write_inode(void * inode, void * buff){
    if(inode == NULL || buff == NULL)
        return -1;
    uint8_t * valid = memcpy(inode, buff, sizeof(node));
    if(valid != NULL)
        return 0;
    return -1;
}

/*
@brief read part or all of memory block
@return valid number of bytes read
*/
uint32_t read_block(void *fs_space, void *buff, uint32_t block, off_t offset, uint32_t bytes)
{
    uint8_t *start = fs_space + block * BLOCKSIZE + offset;
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
uint32_t write_block(void *fs_space, void *buff, uint32_t block, off_t offset, uint32_t bytes)
{
    uint8_t *start = fs_space + block * BLOCKSIZE + offset;
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
uint32_t allocate_block(void *fs_space)
{
    uint32_t free_block = 0;
    uint8_t *next_block = fs_space + *(uint32_t *)fs_space * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == fs_space)
        return 0; // no more free blocks since next block is super block
    for (uint32_t i = 0; i < BLOCKSIZE; i += 4)
    {
        free_block = *(uint32_t *)next_block;
        if (free_block != 0)
        {
            bzero(next_block, sizeof(uint32_t)); // zero out available block
            if (i == BLOCKSIZE - 4)
            {
                free_block = *(uint32_t *)fs_space;                // set free block as current block we are looking in
                *(uint32_t *)fs_space = *(uint32_t *)(next_block); // last 4 bytes contain what to set next super block value
            }
            return free_block;
        }
        next_block = next_block + 4; // advance 4 bytes
    }
    free_block = *(uint32_t *)fs_space; // set to block given by super block since zeroed out
    *(uint32_t *)fs_space = 0;
    return free_block; // we only get here if block is filled with zeros meaning it was the last free block
}

/*
@brief add a block to the free list. Returns 0 on successful return, 1 otherwise
*/
uint32_t free_block(void *fs_space, uint32_t block)
{
    uint32_t free_block = 0;
    uint8_t *next_block = fs_space + *(uint32_t *)fs_space * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == fs_space)
    { // no more free blocks then add as next free block
        *(uint32_t *)fs_space = block;
        bzero(fs_space + block * BLOCKSIZE, BLOCKSIZE); // clear block
        return 0;                                       // success
    }
    for (uint32_t i = 0; i < BLOCKSIZE; i += 4)
    {
        free_block = *(uint32_t *)next_block;
        if (free_block == 0) // a free spot to write in a new block
        {
            *(uint32_t *)next_block = block; // write in block to free space
            if (i == BLOCKSIZE - 4)
            {
                bzero(fs_space + block * BLOCKSIZE, BLOCKSIZE); // zero if we are set up to jump here
            }
            return 0;
        }
        next_block = next_block + 4; // advance 4 bytes
    }
    // if the whole block is full, make block next super block with link to the full one
    bzero(fs_space + block * BLOCKSIZE, BLOCKSIZE);
    *(uint32_t *)(fs_space + (block + 1) * BLOCKSIZE - 4) = *(uint32_t *)fs_space; // write in old super block to link to
    *(uint32_t *)fs_space = block;
    return 0;
}
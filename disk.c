
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "disk.h"
#include<time.h>
/*
@brief mkfs for tyrant DEFAULT
@param fs_space pointer to where beginning of memory space to write over
@note deprecated
*/
void tfs_mkfs_v1(void *fs_space)
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
@brief mkfs for tyrant with define options
@param fs_space pointer to where beginning of memory space to write over
@return 0 on success, -1 failure
@note future implementations could have inputs for amount of inodes and blocks depending on algorithm
*/
int tfs_mkfs(void *fs_space)
{
    if(fs_space == NULL)
        return -1;
    for (int i = BLOCKSIZE; i < END_OF_INODE * BLOCKSIZE; i += INODE_SIZE_BOUNDARY)
    {

        node *inode_init = fs_space + i; // set up a basic node
        bzero(inode_init, INODE_SIZE_BOUNDARY);          // clear out inode info
    }

    *((uint32_t *)fs_space) = END_OF_INODE;                       // superblock holds first free block address
    uint8_t *free_blockptr = fs_space + END_OF_INODE * BLOCKSIZE; // shift pointer over to his free block
    uint64_t block_counter = END_OF_INODE;

    uint64_t addr_counter = 0;
    while(addr_counter < NUM_FREE_BLOCKS){
        bzero(free_blockptr, BLOCKSIZE);
        uint8_t * block_ptr = free_blockptr+BLOCKSIZE;
        for(uint64_t i = 0; i < BLOCKSIZE; i += 8){
            if(addr_counter >= NUM_FREE_BLOCKS)
                break;
            addr_counter++;
            // *(uint64_t*)(free_blockptr+i) = block_ptr;//write in address
            memcpy(free_blockptr+i, &block_ptr, sizeof(block_ptr));
            block_ptr += BLOCKSIZE;
        }
        free_blockptr = block_ptr-BLOCKSIZE;//go back and write the last address stored
    }
    root = allocate_inode(fs_space);
    root->mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;//User, group, and other can only read
    root->access_time = (uint32_t) time(NULL);
    root->creation_time = (uint32_t) time(NULL);
    uint8_t * block = allocate_block(fs_space);
    root->direct_blocks[0] = block;
    char path1 []= ".";
    char path2 [] = "..";
    write_block(path1, block, 0, sizeof(path1));//56 bytes for directory name
    write_block(&root, block, 56, sizeof(root));//write root address

    write_block(path2, block, 64, sizeof(path2));
    write_block(&root, block, 64*2-8, sizeof(root));//write root address
    root->data_time = time(NULL);
    root->size = NAME_BOUNDARY*2;
    root->blocks = 1;
    return 0;
}



/*
@brief allocate an available inode
@return memory location of inode. Returns NULL if no inode is available
@note User must set mode once pointer is returned to properly take it off the free lost.
Alternative is to make change here in the code.
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
@param buff buffer you want to read into
@param block pointer to block
@param offset byte offset to read from
@param bytes bytes you want to read
@return valid number of bytes read
*/
uint32_t read_block(void *buff, void * block, off_t offset, uint32_t bytes)
{
    if(buff == NULL || block == NULL || block == 0)
        return 0;
    if(offset > BLOCKSIZE || offset < 0)
        return 0;
    uint8_t *start = block+offset;
    uint32_t bytes_avail = BLOCKSIZE-offset;
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
    return 0;
}

/*
@brief write into block
@param buff buffer containting write data
@param block pointer to block
@param offset byte offset to write from
@param bytes bytes you want to write
@return valid number of bytes written
*/
uint32_t write_block(void *buff, void * block, off_t offset, uint32_t bytes)
{
    if(buff == NULL || block == NULL)
        return 0;
    if(offset > BLOCKSIZE || offset < 0)
        return 0;
    uint8_t *start = block+offset;
    uint32_t bytes_avail = BLOCKSIZE-offset;
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
    return 0;
}

/*
@brief find next free block and allocate it
@param fs_space pointer to disk/memory
@return pointer to where block is
*/
void * allocate_block(void *fs_space)
{
    uint64_t free_block = 0;
    uint8_t *next_block = fs_space + *(uint64_t *)fs_space * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == fs_space)
        return NULL; // no more free blocks since next block is super block
    for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)//8 byte addresses for large storage drives (32bit )
    {
        free_block = *(uint64_t *)next_block;
        if (free_block != 0)
        {
            bzero(next_block, sizeof(uint64_t)); // zero out available block
            if (i == BLOCKSIZE - ADDR_LENGTH)//only 1 available address. Update super block
            {
                free_block = *(uint64_t *)fs_space;                // set free block as current block we are looking in
                *(uint64_t *)fs_space = *(uint64_t *)(next_block); // last 8 bytes contain what to set next super block value
            }
            return next_block;
        }
        next_block = next_block + ADDR_LENGTH; // advance 8 bytes
    }
    //The block was completely zeroed out which means it was the only block available
    free_block = *(uint64_t *)fs_space; // set to block given by super block since zeroed out
    next_block = fs_space;
    *(uint64_t *)fs_space = 0;
    return next_block; // we only get here if block is filled with zeros meaning it was the last free block
}

/*
@brief add a block to the free list
@param fs_space pointer to start of disk/memory
@param block we want to free
@return returns 0 on success, -1 on failure
@note zeroing only occurs when we link this block as the next place to jump to (last stored addr or super block)
This is an easy hack to reduce zero overhead since read and write pointers will prevent reading
old data
*/
int free_block(void *fs_space, void * block)
{
    if(fs_space == NULL || block == NULL)
        return -1;
    uint64_t free_block = 0;
    uint8_t *next_block = fs_space + *(uint64_t *)fs_space * BLOCKSIZE; // go to the block containing free address blocks
    if (next_block == fs_space)//Add as super block if there aren't any other free blocks
    {
        // *(uint64_t *)fs_space = block;
        memcpy(fs_space, &block, sizeof(block));
        bzero(block, BLOCKSIZE); // clear block
        return 0;// success
    }
    // next_block += BLOCKSIZE-8;//get to last stored address.
    for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        free_block = *(uint64_t *)next_block;
        if (free_block == 0) // a free spot to write in a new block
        {
            // *(uint64_t *)next_block = block; // write in block to free space
            memcpy(next_block, &block, sizeof(block));
            if (i == BLOCKSIZE-ADDR_LENGTH)//first address at end
            {
                bzero(block, BLOCKSIZE); // zero if we are set up to jump here
            }
            return 0;
        }
        next_block = next_block + ADDR_LENGTH; // advance 8 bytes
    }
    // if the whole block is full, make block next super block with link to the full one
    bzero(block, BLOCKSIZE);
    // *(uint64_t *)(block+BLOCKSIZE-8) = *(uint64_t *)fs_space; //last address in freed block is current superblock that's full
    memcpy(block+BLOCKSIZE-8, fs_space, ADDR_LENGTH);
    memcpy(fs_space, &block, sizeof(block));
    // *(uint64_t *)fs_space = block;//set new super block to be our freed block with addr of next block at end
    return 0;
}
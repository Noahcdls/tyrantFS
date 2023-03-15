
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "disk.h"
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

// node * root_node = NULL;
uint64_t root_node = BLOCKSIZE;

int drive = -1; // field descriptor of drive we are working with
uint64_t drivesize = 0;
uint64_t inode_byte_boundary = 0;
uint64_t num_blocks = 0;
uint64_t inode_blocks = 0;
uint64_t data_blocks = 0;

/*
@brief mkfs for tyrant with define options
@param fs_space pointer to where beginning of memory space to write over
@return 0 on success, -1 failure
@note future implementations could have inputs for amount of inodes and blocks depending on algorithm
*/
int tfs_mkfs(int fd)
{
    if (fd < 0)
        return -1;
    drive = fd;
    // set up boundaries
    uint8_t buff[BLOCKSIZE];
    bzero(buff, BLOCKSIZE);
    ioctl(fd, BLKGETSIZE64, &drivesize);
    num_blocks = drivesize / BLOCKSIZE;
    inode_blocks = num_blocks / 65; // following ext4 standard of 1 inode for every 16KB of data or 1 inode block for every 64 data blocks
    data_blocks = inode_blocks * 64 - 1;//leave 1 for superblock
    inode_byte_boundary = (inode_blocks + 1) * BLOCKSIZE;
    //print("Drive is %.3f GB and has %ld blocks!\ninode blocks: %ld\ndata blocks: %ld\n\n", (float)drivesize/1024/1024/1024, num_blocks, inode_blocks, data_blocks);
    lseek(fd, 0, SEEK_SET);
    for (uint64_t i = 0; i < inode_blocks + 1; i++) // clear out inodes and super block
        write(fd, buff, BLOCKSIZE);
    //print("Cleared out inodes and super block");
    lseek(fd, 0, SEEK_SET);
    write(fd, &inode_byte_boundary, ADDR_LENGTH); // superblock holds first free block address

    uint64_t addr_counter = 0;
    uint64_t block_ptr = 0;
    uint64_t free_blockptr = inode_byte_boundary + addr_counter * BLOCKSIZE;
    while (addr_counter < data_blocks)
    {
        lseek(fd, free_blockptr, SEEK_SET); // seek to beginning of block
        write(fd, buff, BLOCKSIZE);         // clear out block
        lseek(fd, -BLOCKSIZE, SEEK_CUR);    // go back to beginning
	////print("Free block ptr is at %ld\n", free_blockptr);
	//sleep(1);
        block_ptr = inode_byte_boundary + addr_counter * BLOCKSIZE + BLOCKSIZE; // next free block
        for (uint64_t i = 0; i < BLOCKSIZE; i += 8)                             // write one full block
        {
            if (addr_counter >= data_blocks){// break when we've hit our max
                break;
	    }
            write(fd, &block_ptr, ADDR_LENGTH); // automatically increments write ptr by 8bytes so need to shift
            block_ptr += BLOCKSIZE;           // add blocksize to offset where next block is
            addr_counter++;                   // added 1 block to list
	    //printf("Addr counter updated to %ld\n", addr_counter);
        }
	//sleep(2);
	//printf("Old block pointer %ld\n", free_blockptr);
        free_blockptr = block_ptr - BLOCKSIZE; // go back and write the last address stored
	//printf("New block pointer %ld\n", free_blockptr);
    }

    //print("Finished writing in data blocks\n");
    root_node = allocate_inode(fd);
    node temp_node;
    //print("Allocated a root at %ld!\n", root_node);
    lseek(fd, root_node, SEEK_SET);
    read(fd, &temp_node, sizeof(node));
    temp_node.mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH; // User, group, and other can only read
    //print("Temp node mode %x\n", temp_node.mode);
    temp_node.access_time = (uint64_t)time(NULL);
    temp_node.creation_time = (uint64_t)time(NULL);
    uint64_t block = allocate_block(drive);
    //print("Block is at %ld\n", block);
    temp_node.direct_blocks[0] = block;
    char path1[] = ".";
    char path2[] = "..";
    write_block(buff, block, 0, BLOCKSIZE);
    write_block(path1, block, 0, sizeof(path1));           // NAME_BOUNDARY - ADDR_LENGTH bytes for directory name
    write_block(&root_node, block, NAME_BOUNDARY-ADDR_LENGTH, ADDR_LENGTH); // write root_node address

    write_block(path2, block, NAME_BOUNDARY, sizeof(path2));
    write_block(&root_node, block, NAME_BOUNDARY * 2 - ADDR_LENGTH, ADDR_LENGTH); // write root_node address

    read_block(buff, block, NAME_BOUNDARY-ADDR_LENGTH, ADDR_LENGTH);
    //print("Double checking . for root node that %ld == %ld\n", root_node,* (uint64_t*)buff);
    read_block(buff, block, NAME_BOUNDARY+NAME_BOUNDARY-ADDR_LENGTH, ADDR_LENGTH);
    //print("Double checking .. for root node that %ld == %ld\n", root_node, *(uint64_t*)buff);
    temp_node.data_time = time(NULL);
    temp_node.size = NAME_BOUNDARY * 2;
    temp_node.blocks = 1;
    temp_node.links = 1;
    commit_inode(&temp_node, root_node);
    fetch_inode(root_node, &temp_node);
    //print("double check mode %x\n", temp_node.mode);
    printf("Finished making FS\n");
    return 0;
}


int tfs_disk_info(int fd)
{
    if (fd < 0)
        return -1;
    drive = fd;
    // set up boundaries
    uint8_t buff[BLOCKSIZE];
    bzero(buff, BLOCKSIZE);
    ioctl(fd, BLKGETSIZE64, &drivesize);
    num_blocks = drivesize / BLOCKSIZE;
    inode_blocks = num_blocks / 65; // following ext4 standard of 1 inode for every 16KB of data or 1 inode block for every 64 data blocks
    data_blocks = inode_blocks * 64 - 1;//leave 1 for superblock
    inode_byte_boundary = (inode_blocks + 1) * BLOCKSIZE;
    printf("Drive is %.3f GB and has %ld blocks!\ninode blocks: %ld\ndata blocks: %ld\n\n", (float)drivesize/1024/1024/1024, num_blocks, inode_blocks, data_blocks);
    return 0;
}

/*
@brief allocate an available inode
@return memory location of inode. Returns -1 if no inode is available
@note User must set mode once pointer is returned to properly take it off the free lost.
Alternative is to make change here in the code.
*/
uint64_t allocate_inode(int fd)
{
    if (drive < 0)
        return 0;
    uint64_t sought_to = lseek(drive, BLOCKSIZE, SEEK_SET);
    //print("Moving head initially to %lx\n", sought_to);
    node *inode_ptr = malloc(sizeof(node));
    for (uint64_t i = BLOCKSIZE; i < inode_byte_boundary; i += INODE_SIZE_BOUNDARY)
    {
	sought_to = lseek(drive, i, SEEK_SET);
	//print("Seeking to %lx\n", sought_to);
        uint64_t bytes_read = read(drive, inode_ptr, sizeof(node)); // read ptr automatically shifts us up to next inode
        if (inode_ptr->mode == 0)
        { // cleared mode means not in use
	    //print("\n\nAllocating inode at address %lx. Read %lx bytes\n\n", i, bytes_read);
            free(inode_ptr);
            return i;
        }
    }
    free(inode_ptr);
    return 0;
}

/*
@brief free a given inode back to the fs
@param inode Pointer to the inode that is to be freed
@return 0 for proper return, -1 for error
*/
int free_inode(uint64_t cur_node)
{
    if (cur_node == 0)
        return -1;
    uint8_t buff[INODE_SIZE_BOUNDARY];
    bzero(buff, INODE_SIZE_BOUNDARY);
    lseek(drive, cur_node, SEEK_SET);
    write(drive, buff, INODE_SIZE_BOUNDARY);
    node cur;
    fetch_inode(cur_node, &cur);
    //print("FREED: Mode = %x\n", cur.mode);
    return 0;
}

/*
@brief read a given inode back to a buffer
@param inode pointer to the inode that is to be read
@param buff buffer we want to write inode information to
@return 0 for proper return, -1 for error
*/
int read_inode(uint64_t inode, void *buff)
{
    if (inode == 0 || buff == NULL)
        return -1;
    lseek(drive, inode, SEEK_SET);
    ssize_t valid = read(drive, buff, INODE_SIZE_BOUNDARY);
    // uint8_t *valid = memcpy(buff, inode, sizeof(node));
    if (valid > 0)
        return 0;
    return 0;
}

/*
@brief write over an inode
@param inode pointer to the inode that is to be written to
@param buff buffer containing new inode info
@return 0 for proper return, -1 for error
@note buff must contain a full inode.
The easiest way to write and edit is to read a copy, make changes, and write back
*/
int write_inode(uint64_t inode, void *buff)
{
    if (inode == 0 || buff == NULL)
        return -1;
    lseek(drive, inode, SEEK_SET);
    ssize_t valid = write(drive, buff, INODE_SIZE_BOUNDARY);
    if (valid > 0)
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
int read_block(void *buff, uint64_t block, off_t offset, uint64_t bytes)
{
    if (buff == NULL || block == 0)
        return 0;
    if (offset > BLOCKSIZE || offset < 0)
        return 0;
    uint64_t start = block + offset;
    uint64_t bytes_avail = BLOCKSIZE - offset;
    lseek(drive, start, SEEK_SET);

    if (bytes_avail < bytes)
    { // asking for more bytes than what is left so read what we can
        ssize_t read_bytes = read(drive, buff, bytes_avail);
        return read_bytes;
    }
    else
    { // can fill buffer with correct number of bytes
        ssize_t read_bytes = read(drive, buff, bytes);
        return read_bytes;
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
int write_block(void *buff, uint64_t block, off_t offset, uint64_t bytes)
{
    if (buff == NULL)
        return 0;
    if (offset > BLOCKSIZE || offset < 0)
        return 0;
    uint64_t start = block + offset;
    lseek(drive, start, SEEK_SET);
    uint64_t bytes_avail = BLOCKSIZE - offset;
    if (bytes_avail < bytes)
    { // want to write more bytes than what is left in block
        ssize_t written = write(drive, buff, bytes_avail);
        return written;
    }
    else
    { // write bytes to block
        ssize_t written = write(drive, buff, bytes);
        return written;
    }
    return 0;
}

/*
@brief find next free block and allocate it
@param fs_space pointer to disk/memory
@return pointer/location to where block is
*/
uint64_t allocate_block(int fd)
{
    uint64_t free_block = 0;
    uint64_t next_block = 0;
    uint8_t buff[ADDR_LENGTH];
    bzero(buff, ADDR_LENGTH); // zero array

    lseek(drive, 0, SEEK_SET);
    read(drive, &next_block, ADDR_LENGTH); // get next block address
    if (next_block == 0)
        return 0; // no more free blocks since next block is super block

    lseek(drive, next_block, SEEK_SET);
    for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH) // 8 byte addresses for large storage drives (32bit )
    {
        read(drive, &free_block, ADDR_LENGTH); // read ptr automatically shifts up by 8 bytes
        if (free_block != 0)
        {
            lseek(drive, -ADDR_LENGTH, SEEK_CUR); // zero out available block address
            write(drive, buff, ADDR_LENGTH);

            if (i == BLOCKSIZE - ADDR_LENGTH) // only 1 available address. Update super block
            {
                // lseek(drive, next_block, SEEK_SET);// Save address of next free list block
                // write(drive, free_block, ADDR_LENGTH);
                next_block = free_block;
                // memcpy(next_block, &free_block, ADDR_LENGTH);//old code

                lseek(drive, 0, SEEK_SET); // set free block as current block we are looking in
                read(drive, &free_block, ADDR_LENGTH);
                // memcpy(&free_block, fs_space, ADDR_LENGTH);//old code

                lseek(drive, 0, SEEK_SET); // Change super block to point to next free list block
                write(drive, &next_block, ADDR_LENGTH);
                // memcpy(fs_space, next_block, ADDR_LENGTH);//old code
            }
            return free_block;
        }
        next_block = next_block + ADDR_LENGTH; // advance 8 bytes
    }
    // The block was completely zeroed out which means it was the only block available
    lseek(drive, 0, SEEK_SET);
    read(drive, &free_block, ADDR_LENGTH);
    lseek(drive, -ADDR_LENGTH, SEEK_CUR);
    write(drive, buff, ADDR_LENGTH);
    return free_block; // we only get here if block is filled with zeros meaning it was the last free block
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
int free_block(int fd, uint64_t block)
{
    if (fd < 0 || block == 0)
        return -1;
    uint64_t free_block = 0;
    uint8_t buff[BLOCKSIZE];
    bzero(buff, BLOCKSIZE);
    // uint8_t *next_block = fs_space + *(uint64_t *)fs_space * BLOCKSIZE; // go to the block containing free address blocks
    lseek(drive, 0, SEEK_SET);
    uint64_t next_block = 0;
    read(drive, &next_block, ADDR_LENGTH); // get block to add address to

    if (next_block == 0) // Add as super block if there aren't any other free blocks
    {
        // *(uint64_t *)fs_space = block;
        lseek(drive, 0, SEEK_SET);         // go back to beginning
        write(drive, &block, ADDR_LENGTH); // write back new address
        lseek(drive, block, SEEK_SET);     // clear block
        write(drive, buff, BLOCKSIZE);
        return 0; // success
    }

    lseek(drive, next_block, SEEK_SET); // go to block containing addresses
    for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        read(drive, &free_block, ADDR_LENGTH); // read address. read ptr automatically moves 8 bytes
        if (free_block == 0)                   // a free spot to write in a new block
        {
            // *(uint64_t *)next_block = block; // write in block to free space
            lseek(drive, -ADDR_LENGTH, SEEK_CUR); // go back 8 bytes to go back where we just read
            write(drive, &block, ADDR_LENGTH);    // write in addr
            if (i == BLOCKSIZE - ADDR_LENGTH)     // first address at end
            {
                lseek(drive, block, SEEK_SET);
                write(drive, buff, BLOCKSIZE); // zero if we are set up to jump here
            }
            return 0;
        }
        next_block = next_block + ADDR_LENGTH; // advance 8 bytes
    }
    next_block -= BLOCKSIZE;
    // if the whole block is full, make block next super block with link to the full one
    lseek(drive, block, SEEK_SET);
    write(drive, buff, BLOCKSIZE); // wipe block
    // *(uint64_t *)(block+BLOCKSIZE-8) = *(uint64_t *)fs_space; //last address in freed block is current superblock that's full
    lseek(drive, block + BLOCKSIZE - ADDR_LENGTH, SEEK_SET);
    write(drive, &next_block, ADDR_LENGTH);
    // memcpy(block + BLOCKSIZE - ADDR_LENGTH, fs_space, ADDR_LENGTH);

    lseek(drive, 0, SEEK_SET);
    write(drive, &block, ADDR_LENGTH);
    // memcpy(fs_space, &block, sizeof(block));
    // *(uint64_t *)fs_space = block;//set new super block to be our freed block with addr of next block at end
    return 0;
}

/// @brief fetch a specified block belonging to an inode
/// @param my_node inode of interest
/// @param block_no the block number ranging from 0 to my_node->blocks - 1
/// @return return block pointer or NULL
void *fetch_block(uint64_t my_node, uint64_t block_no, void *block)
{
    uint64_t block_addr = 0;
    if (my_node == 0)
        return NULL;

    node *parent = malloc(sizeof(node));
    fetch_inode(my_node, parent);

    if (parent->blocks <= block_no)
    { // only accept block_no from 0 to blocks-1
        free(parent);
        return NULL;
    }

    if (block_no < 12)
    {
        read_block(block, parent->direct_blocks[block_no], 0, BLOCKSIZE);
        free(parent);
        return block;
    }
    else if (block_no < 12 + BLOCKSIZE / ADDR_LENGTH)
    {
        read_block(&block_addr, parent->indirect_blocks, (block_no - 12) * ADDR_LENGTH, ADDR_LENGTH);
        if (block_addr == 0)
        {
            free(parent);
            return NULL;
        }
        read_block(block, block_addr, 0, BLOCKSIZE);
        free(parent);
        return block;
    }
    else if (block_no < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2))
    {
        uint64_t indir_blk = 0;
        uint64_t dbl_offset = block_no - 12 - BLOCKSIZE / ADDR_LENGTH;
        for (uint64_t i = 0; i < pow(BLOCKSIZE / ADDR_LENGTH, 2); i += BLOCKSIZE / ADDR_LENGTH)
        {
            if (dbl_offset < i)
            {
                uint32_t valid_bytes = read_block(&indir_blk, parent->dbl_indirect, (i / (BLOCKSIZE / ADDR_LENGTH) - 1) * ADDR_LENGTH, ADDR_LENGTH); // get block addr from dbl indir
                if (indir_blk == 0 || valid_bytes == 0)
                {
                    free(parent);
                    return NULL;
                }
                dbl_offset = dbl_offset - (i - BLOCKSIZE / ADDR_LENGTH);
                read_block(&block_addr, indir_blk, dbl_offset * ADDR_LENGTH, ADDR_LENGTH);
                if (block_addr == 0)
                {
                    free(parent);
                    return NULL;
                }
                read_block(block, block_addr, 0, BLOCKSIZE);
                free(parent);
                return block;
            }
        }
    }
    else if (block_no < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2) + pow(BLOCKSIZE / ADDR_LENGTH, 3))
    {
        uint64_t dbl_blk = 0;
        uint64_t indir_blk = 0;
        uint64_t trpl_offset = block_no - (12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2));
        for (uint64_t i = 0; i < pow(BLOCKSIZE / ADDR_LENGTH, 3); i += pow(BLOCKSIZE / ADDR_LENGTH, 2))
        {
            if (trpl_offset < i)
            {
                read_block(&dbl_blk, parent->trpl_indirect, ((i / (pow(BLOCKSIZE / ADDR_LENGTH, 2))) - 1) * ADDR_LENGTH, ADDR_LENGTH); // get dbl indir blk address we want
                if (dbl_blk == 0)
                {
                    free(parent);
                    return NULL;
                }
                trpl_offset = trpl_offset - (i - pow(BLOCKSIZE / ADDR_LENGTH, 2));
                for (int j = 0; j < pow(BLOCKSIZE / ADDR_LENGTH, 2); j += BLOCKSIZE / ADDR_LENGTH)
                {
                    if (trpl_offset < j)
                    {
                        read_block(&indir_blk, dbl_blk, (j / (BLOCKSIZE / ADDR_LENGTH) - 1) * ADDR_LENGTH, ADDR_LENGTH);
                        if (indir_blk == 0)
                        {
                            free(parent);
                            return NULL;
                        }
                        trpl_offset = trpl_offset - (j - BLOCKSIZE / ADDR_LENGTH);
                        read_block(&block_addr, indir_blk, trpl_offset * ADDR_LENGTH, ADDR_LENGTH);
                        if (block_addr == 0)
                        {
                            free(parent);
                            return NULL;
                        }
                        read_block(block, block_addr, 0, BLOCKSIZE);
                        return block;
                    }
                }
            }
        }
    }
    return NULL;
}

/// @brief fetch the inode we want into a buffer
/// @param node offset to where node is located
/// @param buff buffer to place inode
/// @return returns buff
void *fetch_inode(uint64_t my_node, void *buff)
{
    if (my_node == 0)
        return NULL;
   lseek(drive, my_node, SEEK_SET);
   // //print("Fetched inode at offset %ld\n", sought_to);
   read(drive, buff, sizeof(node));
   // //print("Read %ld bytes to fetch inode\n", bytes);
   // //print("Size of node is %ld\n", sizeof(node));
    return buff;
}

int commit_inode(node *my_node, uint64_t node_loc)
{
   lseek(drive, node_loc, SEEK_SET);
   // //print("Sought to %ld for committing node\n", sought_to);
   uint64_t bytes = write(drive, my_node, sizeof(node));
   // //print("Wrote %ld bytes to commit node\n", bytes);
   // //print("Size of node is %ld\n", sizeof(node));
    return bytes;
    
}

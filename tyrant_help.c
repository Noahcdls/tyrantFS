#include "tyrant_help.h"

void *add_block_to_node(node *parent)
{
    if (parent == NULL)
    {
        printf("NULL PARENT\n\n");
        return NULL;
    }
    uint8_t *block = allocate_block(fs_space);
    if (block == NULL)
    {
        printf("NO MORE BLOCKS: ADD BLOCK TO NODE\n\n"); //
        return NULL;
    }
    if (parent->blocks == UINT64_MAX)
    {
        printf("HIT MAX BLOCKS\n\n");
        free_block(fs_space, block);
        return NULL;
    }
    printf("INSERTING BLOCK\n\n");
    uint64_t i = parent->blocks;

    if (i < 12) // direct
    {
        printf("INSERTING INTO DIRECT\n\n");
        parent->direct_blocks[i] = block;
        parent->blocks++;
        return block;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH) // indirect
    {
        printf("INSERTING INTO INDIRECT\n\n");
        uint8_t *indir_blk = parent->indirect_blocks;
        if (indir_blk == NULL)
        {
            indir_blk = allocate_block(fs_space);
            if (indir_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            parent->indirect_blocks = indir_blk;
        }

        uint64_t indir_blk_offset = (i - 12) * ADDR_LENGTH;
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        return block;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2)) // double indirect
    {
        printf("INSERTING INTO DBL INDIRECT\n\n");
        uint8_t *indir_blk = NULL;
        uint8_t *dbl_blk = parent->dbl_indirect;
        if (dbl_blk == NULL)
        {
            dbl_blk = allocate_block(fs_space);
            if (dbl_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            parent->dbl_indirect = dbl_blk;
        }

        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        if (indir_blk == NULL || indir_blk == 0)
        {
            indir_blk = allocate_block(fs_space);
            if (indir_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            write_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH); // write back new address
        }
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        return block;
    }
    else // triple indirect
    {
        printf("INSERTING INTO TRPL INDIRECT\n\n");
        uint8_t *indir_blk = NULL;
        uint8_t *dbl_blk = NULL;
        uint8_t *trpl_blk = parent->trpl_indirect;
        if (trpl_blk == NULL)
        {
            trpl_blk = allocate_block(fs_space);
            if (trpl_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            parent->trpl_indirect = trpl_blk;
        }

        uint64_t trpl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) * ADDR_LENGTH;
        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;

        read_block(&dbl_blk, dbl_blk, trpl_blk_offset, ADDR_LENGTH);
        if (dbl_blk == NULL || dbl_blk == 0)
        {
            dbl_blk = allocate_block(fs_space);
            if (dbl_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            write_block(&dbl_blk, dbl_blk, trpl_blk_offset, ADDR_LENGTH);
        }
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        if (indir_blk == NULL || indir_blk == 0)
        {
            indir_blk = allocate_block(fs_space);
            if (indir_blk == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
            write_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        }
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        return block;
    }
}

/// @brief remove link of a dir/nod from a directory inode
/// @param parent_node parent node
/// @param cur_node child node
/// @return 0 on success; -1 on failure
int remove_link_from_parent(void *fs_space, node *parent_node, node *cur_node)
{
    node *child = NULL;
    uint8_t *block = NULL;
    uint8_t entry_data[NAME_BOUNDARY];

    for (int i = 0; i < parent_node->blocks; i++)
    {
        block = get_i_block(parent_node, i);
        if (block == NULL)
            return -1;
        for (int j = 0; j < BLOCKSIZE && j + i * BLOCKSIZE < parent_node->size; j += NAME_BOUNDARY)
        {
            read_block(&child, block, j + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH);
            if (child == cur_node)
            {
                bzero(block + j, NAME_BOUNDARY);                                         // remove old entry
                uint8_t *last_block = get_i_block(parent_node, parent_node->blocks - 1); // get last block for last entry
                if (last_block == NULL)
                    return -1;
                if (j + i * BLOCKSIZE != parent_node->size - NAME_BOUNDARY)
                {                                                                                                     // not the last slot so we need to refill the space
                    read_block(entry_data, last_block, parent_node->size % BLOCKSIZE - NAME_BOUNDARY, NAME_BOUNDARY); // copy data over into buffer
                    write_block(entry_data, block, j, ADDR_LENGTH);                                                   // fill in empty slot
                }
                parent_node->size -= NAME_BOUNDARY;
                if (parent_node->size % BLOCKSIZE == 0)
                { // have cleared an entire block and need to deallocate
                    free_block(fs_space, last_block);
                    parent_node->blocks--;
                }
                return 0;
            }
        }
    }
    return -1;
}

int sub_unlink(void *fs_space, node *parent, node *child)
{
    // remove link from its parent
    if (parent == NULL || child == NULL)
        return -1;
    int status = remove_link_from_parent(fs_space, parent, child);
    if (status == -1)
    {
        // something is wrong, as we cannot find child from parent
        return -1;
    }

    // Update links count
    child->links -= 1;
    uint8_t *block = NULL;
    node *tmp = NULL;
    // if links count is 0, remove the file/directory
    if (child->links == 0)
    {
        // if it is a directory, unlink everything in it before freeing block
        if ((child->mode & S_IFMT) == S_IFDIR)
        {
            for (int i = 0; i < child->blocks; i++)
            {
                block = get_i_block(child, i);
                // unlink each entry (children dir or nod) in the block
                for (int j = 0; j < BLOCKSIZE && j + i * BLOCKSIZE < child->size; j += NAME_BOUNDARY)
                {
                    // get the address of the inode?
                    // char temp[NAME_BOUNDARY - ADDR_LENGTH];
                    read_block(&tmp, block, j + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH);
                    // read_block(temp, block, j, NAME_BOUNDARY - ADDR_LENGTH);

                    sub_unlink(fs_space, child, tmp);
                }

                // put the whole block back to free list
                free_block(fs_space, block);
            }
        }
        else
        {
            // free all blocks that belong to this inode
            for (int i = 0; i < child->blocks; i++)
            {
                block = get_i_block(child, i);
                free_block(fs_space, block);
            }
        }

        // free the inode
        free_inode(child);
    }
    return 0;
}

/// @brief Get the ith block of an inode
/// @param cur_node node for the operation
/// @param i index for the block in search
/// @return pointer to ith block success, NULL failure
void *get_i_block(node *cur_node, uint64_t i)
{
    // curnode cannot be NULL
    if (cur_node == NULL)
    {
        return NULL;
    }

    // cur_node should have at least one block
    if (cur_node->blocks == 0)
    {
        return NULL;
    }

    // the index cannot be larger than the number of blocks the cur_node has.
    if (cur_node->blocks <= i)
    {
        return NULL;
    }

    // cannot have negative index
    if (i < 0)
    {
        return NULL;
    }

    uint8_t *cur_blk;
    if (i < 12) // direct
    {
        cur_blk = cur_node->direct_blocks[i];
        return cur_blk;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH) // indirect
    {
        uint8_t *indir_blk = cur_node->indirect_blocks;

        uint64_t indir_blk_offset = (i - 12) * ADDR_LENGTH;
        if (indir_blk == NULL)
        {
            return NULL;
        }
        memcpy(&cur_blk, indir_blk + indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2)) // double indirect
    {
        uint8_t *indir_blk = NULL;
        uint8_t *dbl_blk = cur_node->dbl_indirect;

        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;

        if (dbl_blk == NULL)
        {
            return NULL;
        }
        memcpy(&indir_blk, dbl_blk + dbl_blk_offset, ADDR_LENGTH);
        memcpy(&cur_blk, indir_blk + indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
    else // triple indirect
    {
        uint8_t *indir_blk = NULL;
        uint8_t *dbl_blk = NULL;
        uint8_t *trpl_blk = cur_node->trpl_indirect;

        uint64_t trpl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) * ADDR_LENGTH;
        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        if (trpl_blk == NULL)
        {
            return NULL;
        }
        memcpy(&dbl_blk, dbl_blk + trpl_blk_offset, ADDR_LENGTH);
        memcpy(&indir_blk, dbl_blk + dbl_blk_offset, ADDR_LENGTH);
        memcpy(&cur_blk, indir_blk + indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
}

/// @brief Adds address to directory block
/// @param block block of where to store address
/// @param addr ptr to inode we are adding into the directory
/// @param name name of file/directory we are adding
/// @return 0 success, -1 failure
int add_addr(node *parent, uint8_t *block, node *addr, char *name)
{
    if (block == NULL)
        return -1;
    char temp[NAME_BOUNDARY - ADDR_LENGTH];
    strcpy(temp, name);
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        if (*(block + i) == 0)                                        // nothing written here
        {                                                             // no name so can write over
            write_block(temp, block, i, NAME_BOUNDARY - ADDR_LENGTH); // write name
            printf("Writing address %p at location %p\n", addr, block + i + NAME_BOUNDARY - ADDR_LENGTH);
            write_block(&addr, block, i + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH); // write address
            printf("%p added as address\n", addr);
            parent->size += NAME_BOUNDARY;
            addr->links++;
            return 0;
        }
    }
    return -1;
}

/*
@brief add a child inode to a parent directory
@param parent parent inode
@param child child inode
@return 0 for success, -1 failure
*/
int add_to_directory(void *fs_space, node *parent, node *child, char *name)
{
    if (parent == NULL || child == NULL)
        return -1;
    uint8_t *block = NULL;
    if (parent->size % BLOCKSIZE == 0) // only full blocks
    {                                  // we have filled up a block and need a new one
        block = add_block_to_node(fs_space, parent);
        if (block == NULL)
            return -1;
        bzero(block, BLOCKSIZE); // clean for directory
        return add_addr(parent, block, child, name);
    }
    block = get_i_block(parent, parent->blocks - 1);
    return add_addr(parent, block, child, name);

    return 0;
}

/*
@brief check directory block to see if it has a matching file/directory
and return address
@param block the block containing the directory table
@param name name of directory or file
@return inode ptr to
*/
uint64_t check_block(uint8_t *block, char *name)
{
    printf("Searching for %s\n", name);
    if (block == NULL)
        return NULL;
    char tmp_name[NAME_BOUNDARY - ADDR_LENGTH + 1];
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        memcpy(tmp_name, block + i, NAME_BOUNDARY - ADDR_LENGTH); // copy name
        // printf("%s is %d\n", tmp_name, strcmp(tmp_name, name));
        if (strcmp(tmp_name, name) == 0) // found a match in the name
        {
            uint64_t node_ptr = NULL;
            printf("Reading address at location %p\n", block + i + NAME_BOUNDARY - ADDR_LENGTH);
            read_block(&node_ptr, block, i + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH); // write address
            printf("Return with address %p\n", node_ptr);
            return node_ptr;
        }
    }
    return 0;
}

/*
@brief check indirect block for address
@param indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
uint64_t check_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
{
    if (block == NULL)
        return 0;
    uint64_t tmp_node = 0;
    uint8_t * data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0){
            free(data_block);
            return 0;
        }
        *block_count = *block_count - 1;
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH); // write address
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_block(data_block, name);
        if (tmp_node != 0){
            free(data_block);
            return tmp_node;
        }
    }
    free(data_block);
    return 0;
}

/*
@brief check double indirect block for address
@param double indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
uint64_t check_dbl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
{
    if (block == NULL)
        return 0;
    uint64_t tmp_node = 0;
    uint8_t * data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0){
            free(data_block);
            return 0;
        }
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_indirect_blk(data_block, name, block_count);
        if (tmp_node != 0){
            free(data_block);
            return tmp_node;
        }
    }
    free(data_block);
    return NULL;
}

/*
@brief check triple indirect block for address
@param triple indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
uint64_t check_trpl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
{
    if (block == NULL)
        return 0;
    uint64_t tmp_node = 0;
    uint8_t * data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0){
            free(data_block);
            return 0;
        }
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_dbl_indirect_blk(data_block, name, block_count);
        if (tmp_node != 0){
            free(data_block);
            return tmp_node;
        }
    }
    free(data_block);
    return 0;
}

/*
@brief Given a path, attempt to find the corresponding inode
@return pointer to inode, NULL upon failure
*/
void *find_path_node(char *path)
{
    char cpy_path[NAME_BOUNDARY], *node_name;

    if (root_node == 0)
    {
        printf("NULL ROOT\n");
        return NULL;
    }
    node *cur_node = malloc(sizeof(node));
    fetch_inode(root_node, cur_node);
    strcpy(cpy_path, path);
    node_name = strtok(cpy_path, "/");

    while (node_name != 0) // should break this loop if you find the full path
    {
        node *tmp_node = NULL;
        uint64_t block_cnt = cur_node->blocks; // check how many blocks inode uses to limit blocks checked
        for (int i = 0; i < 12; i++)
        {
            if (block_cnt == 0)
            { // no more blocks means you didnt find out
                free(cur_node);
                return NULL;
            }
            if (cur_node->direct_blocks[i] == NULL)
            { // bad case
                free(cur_node);
                return NULL;
            }
            tmp_node = check_block(cur_node->direct_blocks[i], node_name); // check block for addresses
            block_cnt--;
            if (tmp_node != NULL) // found the next inode
                break;
        }

        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            cur_node = tmp_node;
            node_name = strtok(NULL, "/");
            continue;
        }
        if (block_cnt <= 0)
            return NULL;

        tmp_node = check_indirect_blk(cur_node->indirect_blocks, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            cur_node = tmp_node;
            node_name = strtok(NULL, "/");
            continue;
        }

        if (block_cnt <= 0)
            return NULL;

        tmp_node = check_dbl_indirect_blk(cur_node->dbl_indirect, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            cur_node = tmp_node;
            node_name = strtok(NULL, "/");
            continue;
        }

        if (block_cnt <= 0)
            return NULL;

        tmp_node = check_trpl_indirect_blk(cur_node->indirect_blocks, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            cur_node = tmp_node;
            node_name = strtok(NULL, "/");
            continue;
        }

        return NULL; // nothing found
    }
    return cur_node;
}

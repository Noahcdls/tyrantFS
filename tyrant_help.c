#include "tyrant_help.h"

uint64_t add_block_to_node(node *parent, uint64_t parent_loc)
{
    if (parent == NULL || parent_loc == 0)
    {
        printf("NULL PARENT\n\n");
        return 0;
    }
    uint64_t block = allocate_block(drive);
    if (block == 0)
    {
        printf("NO MORE BLOCKS: ADD BLOCK TO NODE\n\n"); //
        return 0;
    }
    if (parent->blocks == UINT64_MAX)
    {
        printf("HIT MAX BLOCKS\n\n");
        free_block(drive, block);
        return 0;
    }
    printf("INSERTING BLOCK\n\n");
    uint64_t i = parent->blocks;

    if (i < 12) // direct
    {
        printf("INSERTING INTO DIRECT\n\n");
        parent->direct_blocks[i] = block;
        parent->blocks++;
        commit_inode(parent, parent_loc);
        return block;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH) // indirect
    {
        printf("INSERTING INTO INDIRECT\n\n");
        uint64_t indir_blk = parent->indirect_blocks;
        if (indir_blk == 0)
        {
            indir_blk = allocate_block(drive);
            if (indir_blk == 0)
            {
                free_block(drive, block);
                return 0;
            }
            parent->indirect_blocks = indir_blk;
            commit_inode(parent, parent_loc);
        }

        uint64_t indir_blk_offset = (i - 12) * ADDR_LENGTH;
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        commit_inode(parent, parent_loc);
        return block;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2)) // double indirect
    {
        printf("INSERTING INTO DBL INDIRECT\n\n");
        uint64_t indir_blk = 0;
        uint64_t dbl_blk = parent->dbl_indirect;
        if (dbl_blk == 0)
        {
            dbl_blk = allocate_block(drive);
            if (dbl_blk == 0)
            {
                free_block(drive, block);
                return 0;
            }
            parent->dbl_indirect = dbl_blk;
            commit_inode(parent, parent_loc);
        }

        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        if (indir_blk == 0)
        {
            indir_blk = allocate_block(drive);
            if (indir_blk == 0)
            {
                free_block(drive, block);
                return 0;
            }
            write_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH); // write back new address
            commit_inode(parent, parent_loc);
        }
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        commit_inode(parent, parent_loc);
        return block;
    }
    else // triple indirect
    {
        printf("INSERTING INTO TRPL INDIRECT\n\n");
        uint64_t indir_blk = 0;
        uint64_t dbl_blk = 0;
        uint64_t trpl_blk = parent->trpl_indirect;
        if (trpl_blk == 0)
        {
            trpl_blk = allocate_block(drive);
            if (trpl_blk == 0)
            {
                free_block(drive, block);
                return 0;
            }
            parent->trpl_indirect = trpl_blk;
            commit_inode(parent, parent_loc);
        }

        uint64_t trpl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) * ADDR_LENGTH;
        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;

        read_block(&dbl_blk, trpl_blk, trpl_blk_offset, ADDR_LENGTH);
        if (dbl_blk == 0)
        {
            dbl_blk = allocate_block(drive);
            if (dbl_blk == 0)
            {
                free_block(drive, block);
                return 0;
            }
            write_block(&dbl_blk, trpl_blk, trpl_blk_offset, ADDR_LENGTH);
            commit_inode(parent, parent_loc);
        }
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        if (indir_blk == 0)
        {
            indir_blk = allocate_block(drive);
            if (indir_blk == NULL)
            {
                free_block(drive, block);
                return NULL;
            }
            write_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
            commit_inode(parent, parent_loc);
        }
        write_block(&block, indir_blk, indir_blk_offset, ADDR_LENGTH);
        parent->blocks++;
        commit_inode(parent, parent_loc);
        return block;
    }
}

/// @brief remove link of a dir/nod from a directory inode
/// @param parent_node parent node
/// @param cur_node child node
/// @return 0 on success; -1 on failure
int remove_link_from_parent(uint64_t parent, uint64_t cur_node)
{
    if (parent == 0)
        return -1;
    if (cur_node == 0)
        return -1;
    uint64_t child = 0;
    uint64_t block = 0;
    uint8_t entry_data[NAME_BOUNDARY];
    node parent_node;
    fetch_inode(parent, &parent_node);

    for (int i = 0; i < parent_node.blocks; i++)
    {
        block = get_i_block(&parent_node, i);
        if (block == 0)
            return -1;
        for (int j = 0; j < BLOCKSIZE && j + i * BLOCKSIZE < parent_node.size; j += NAME_BOUNDARY)
        {
            read_block(&child, block, j + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH);
            if (child == cur_node)
            {
                uint8_t name_slot[NAME_BOUNDARY];
                bzero(name_slot, NAME_BOUNDARY);
                write_block(name_slot, block, j, NAME_BOUNDARY);                         // remove old entry
                uint64_t last_block = get_i_block(&parent_node, parent_node.blocks - 1); // get last block for last entry
                if (last_block == 0)
                    return -1;
                if (j + i * BLOCKSIZE != parent_node.size - NAME_BOUNDARY)
                {                                                                                                    // not the last slot so we need to refill the space
                    read_block(entry_data, last_block, parent_node.size % BLOCKSIZE - NAME_BOUNDARY, NAME_BOUNDARY); // copy data over into buffer
                    write_block(entry_data, block, j, ADDR_LENGTH);                                                  // fill in empty slot
                }
                parent_node.size -= NAME_BOUNDARY;
                if (parent_node.size % BLOCKSIZE == 0)
                { // have cleared an entire block and need to deallocate
                    free_block(drive, last_block);
                    parent_node.blocks--;
                }
                parent_node.data_time = get_current_time_in_nsec();
                parent_node.change_time = parent_node.data_time();
                commit_inode(&parent_node, parent);
                return 0;
            }
        }
    }
    return -1;
}

int sub_unlink(uint64_t parent, uint64_t child)
{
    // remove link from its parent
    if (parent == 0 || child == 0)
        return -1;
    int status = remove_link_from_parent(parent, child);
    if (status == -1)
    {
        // something is wrong, as we cannot find child from parent
        return -1;
    }
    node parent_node, child_node;
    fetch_inode(parent, &parent_node);
    fetch_inode(child, &child_node);
    // Update links count
    child_node.links -= 1;
    uint64_t block = 0;
    uint64_t tmp = 0;
    // if links count is 0, remove the file/directory
    if (child_node.links == 0)
    {
        // if it is a directory, unlink everything in it before freeing block
        if ((child_node.mode & S_IFMT) == S_IFDIR)
        {
            for (int i = 0; i < child_node.blocks; i++)
            {
                block = get_i_block(&child_node, i);
                // unlink each entry (children dir or nod) in the block
                for (int j = 0; j < BLOCKSIZE && j + i * BLOCKSIZE < child_node.size; j += NAME_BOUNDARY)
                {
                    // get the address of the inode?
                    // char temp[NAME_BOUNDARY - ADDR_LENGTH];
                    read_block(&tmp, block, j + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH);
                    // read_block(temp, block, j, NAME_BOUNDARY - ADDR_LENGTH);

                    sub_unlink(child, tmp);
                }

                // put the whole block back to free list
                free_block(drive, block);
            }
        }
        else
        {
            // free all blocks that belong to this inode
            for (int i = 0; i < child_node.blocks; i++)
            {
                block = get_i_block(&child_node, i);
                free_block(drive, block);
            }
        }

        // free the inode
        free_inode(child);
    }
    else
    {
        commit_inode(&child_node, child);
    }
    return 0;
}

/// @brief Get the ith block of an inode
/// @param cur_node node for the operation
/// @param i index for the block in search
/// @return pointer to ith block success, NULL failure
uint64_t get_i_block(node *cur_node, uint64_t i)
{
    // curnode cannot be NULL
    if (cur_node == NULL)
    {
        return 0;
    }

    // cur_node should have at least one block
    if (cur_node->blocks == 0)
    {
        return 0;
    }

    // the index cannot be larger than the number of blocks the cur_node has.
    if (cur_node->blocks <= i)
    {
        return 0;
    }

    // cannot have negative index
    if (i < 0)
    {
        return 0;
    }

    uint64_t cur_blk;
    if (i < 12) // direct
    {
        cur_blk = cur_node->direct_blocks[i];
        return cur_blk;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH) // indirect
    {
        uint64_t indir_blk = cur_node->indirect_blocks;

        uint64_t indir_blk_offset = (i - 12) * ADDR_LENGTH;
        if (indir_blk == 0)
        {
            return 0;
        }
        read_block(&cur_blk, indir_blk, indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
    else if (i < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2)) // double indirect
    {
        uint64_t indir_blk = 0;
        uint64_t dbl_blk = cur_node->dbl_indirect;

        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;

        if (dbl_blk == 0)
        {
            return 0;
        }
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        read_block(&cur_blk, indir_blk, indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
    else // triple indirect
    {
        uint64_t indir_blk = 0;
        uint64_t dbl_blk = 0;
        uint64_t trpl_blk = cur_node->trpl_indirect;

        uint64_t trpl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) * ADDR_LENGTH;
        uint64_t dbl_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) / (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        uint64_t indir_blk_offset = (i - 12 - BLOCKSIZE / ADDR_LENGTH - (BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % ((BLOCKSIZE / ADDR_LENGTH) * (BLOCKSIZE / ADDR_LENGTH)) % (BLOCKSIZE / ADDR_LENGTH) * ADDR_LENGTH;
        if (trpl_blk == 0)
        {
            return 0;
        }
        read_block(&dbl_blk, trpl_blk, trpl_blk_offset, ADDR_LENGTH);
        read_block(&indir_blk, dbl_blk, dbl_blk_offset, ADDR_LENGTH);
        read_block(&cur_blk, indir_blk, indir_blk_offset, ADDR_LENGTH);
        return cur_blk;
    }
}

/// @brief Adds address to directory block
/// @param block block of where to store address
/// @param addr ptr to inode we are adding into the directory
/// @param name name of file/directory we are adding
/// @return 0 success, -1 failure
int add_addr(uint64_t parent, uint64_t block, uint64_t addr, char *name)
{
    if (block == 0)
        return -1;
    char temp[NAME_BOUNDARY - ADDR_LENGTH];
    uint8_t empty_space = 0;
    node addr_node;
    node parent_node;
    fetch_inode(parent, &parent_node);
    fetch_inode(addr, &addr_node);

    strcpy(temp, name);
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        read_block(&empty_space, block, i, 1);                        // grab a single byte. If that byte is 0, that means the name slot is empty
        if (empty_space == 0)                                         // nothing written here
        {                                                             // no name so can write over
            write_block(temp, block, i, NAME_BOUNDARY - ADDR_LENGTH); // write name
            printf("Writing address %p at location %p\n", addr, block + i + NAME_BOUNDARY - ADDR_LENGTH);
            write_block(&addr, block, i + NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH); // write address
            printf("%p added as address\n", addr);
            parent_node.size += NAME_BOUNDARY;
            addr_node.links++;
            parent_node.data_time = get_current_time_in_nsec();
            parent_node.change_time = parent_node.data_time;
            addr_node.change_time = parent_node.data_time;
            commit_inode(&parent_node, parent);
            commit_inode(&addr_node, addr);
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
int add_to_directory(uint64_t parent, uint64_t child, char *name)
{
    if (parent == 0 || child == 0)
        return -1;
    node parent_node, child_node;
    fetch_inode(parent, &parent_node);
    fetch_inode(child, &child_node);
    uint64_t block = 0;
    uint8_t block_buffer[BLOCKSIZE];
    if (parent_node.size % BLOCKSIZE == 0) // only full blocks
    {                                      // we have filled up a block and need a new one
        block = add_block_to_node(&parent_node, parent);
        if (block == 0)
            return -1;

        bzero(block_buffer, BLOCKSIZE); // clean for directory
        write_block(block_buffer, block, 0, BLOCKSIZE);
        return add_addr(parent, block, child, name);
    }
    block = get_i_block(&parent_node, parent_node.blocks - 1);
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
    uint8_t *data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
        {
            free(data_block);
            return 0;
        }
        *block_count = *block_count - 1;
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH); // write address
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_block(data_block, name);
        if (tmp_node != 0)
        {
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
    uint8_t *data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
        {
            free(data_block);
            return 0;
        }
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_indirect_blk(data_block, name, block_count);
        if (tmp_node != 0)
        {
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
    uint8_t *data_block = malloc(BLOCKSIZE);
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
        {
            free(data_block);
            return 0;
        }
        uint64_t block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        lseek(drive, block_addr, SEEK_SET);
        read(drive, data_block, BLOCKSIZE);
        tmp_node = check_dbl_indirect_blk(data_block, name, block_count);
        if (tmp_node != 0)
        {
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
uint64_t find_path_node(char *path)
{
    char cpy_path[NAME_BOUNDARY], *node_name;

    if (root_node == 0)
    {
        printf("NULL ROOT\n");
        return NULL;
    }
    node *cur_node = malloc(sizeof(node));
    uint8_t *tmp_block = malloc(BLOCKSIZE);
    uint64_t tmp_node = 0;
    if (fetch_inode(root_node, cur_node) == NULL)
        return NULL;
    strcpy(cpy_path, path);
    node_name = strtok(cpy_path, "/");
    while (node_name != 0) // should break this loop if you find the full path
    {
        tmp_node = 0;
        uint64_t block_cnt = cur_node->blocks; // check how many blocks inode uses to limit blocks checked
        for (int i = 0; i < 12; i++)
        {
            if (block_cnt == 0)
            { // no more blocks means you didnt find out
                free(cur_node);
                return NULL;
            }
            if (cur_node->direct_blocks[i] == 0)
            { // bad case
                free(cur_node);
                return NULL;
            }

            lseek(drive, cur_node->direct_blocks[i], SEEK_SET);
            write(drive, tmp_block, BLOCKSIZE);
            tmp_node = check_block(tmp_block, node_name); // check block for addresses
            block_cnt--;
            if (tmp_node != NULL) // found the next inode
                break;
        }

        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            fetch_inode(tmp_node, cur_node);
            node_name = strtok(NULL, "/");
            continue;
        }
        if (block_cnt <= 0)
        {
            free(tmp_block);
            free(cur_node);
            return NULL;
        }

        lseek(drive, cur_node->indirect_blocks, SEEK_SET);
        write(drive, tmp_block, BLOCKSIZE);
        tmp_node = check_indirect_blk(tmp_block, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            fetch_inode(tmp_node, cur_node);
            node_name = strtok(NULL, "/");
            continue;
        }

        if (block_cnt <= 0)
        {
            free(tmp_block);
            free(cur_node);
            return NULL;
        }

        lseek(drive, cur_node->dbl_indirect, SEEK_SET);
        write(drive, tmp_block, BLOCKSIZE);
        tmp_node = check_dbl_indirect_blk(tmp_block, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            fetch_inode(tmp_node, cur_node);
            node_name = strtok(NULL, "/");
            continue;
        }

        if (block_cnt <= 0)
        {
            free(tmp_block);
            free(cur_node);
            return NULL;
        }

        lseek(drive, cur_node->trpl_indirect, SEEK_SET);
        write(drive, tmp_block, BLOCKSIZE);
        tmp_node = check_trpl_indirect_blk(tmp_block, node_name, &block_cnt);
        if (tmp_node != NULL) // broke out of for loop and have a valid inode
        {
            fetch_inode(tmp_node, cur_node);
            node_name = strtok(NULL, "/");
            continue;
        }

        free(tmp_block);
        free(cur_node);
        return NULL; // nothing found
    }
    free(tmp_block);
    free(cur_node);
    return tmp_node;
}


uint64_t get_current_time_in_nsec()
{
    time_t current_time;
    time(&current_time);
    return (uint64_t) current_time;
}
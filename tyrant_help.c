#include "disk.h"
#include "tyrant_help.h"

void *add_block_to_node(void *fs_space, node *parent)
{
    if (parent == NULL)
        return NULL;
    uint8_t *block = allocate_block(fs_space);
    if (block == NULL)
        return;
    if (parent->blocks == UINT64_MAX)
    {
        free_block(fs_space, block);
        return NULL;
    }
    if (parent->blocks < 12)
    {
        parent->direct_blocks[parent->blocks] = block;
        parent->blocks += 1;
        return block;
    }
    else if (parent->blocks < 12 + BLOCKSIZE / ADDR_LENGTH)
    {
        if (parent->indirect_blocks == NULL)
        { // add block where necessary
            parent->indirect_blocks = allocate_block(fs_space);
            if (parent->indirect_blocks == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
        }
        uint32_t bytes = write_block(&block, parent->indirect_blocks, (parent->blocks - 12) * ADDR_LENGTH - ADDR_LENGTH, ADDR_LENGTH);
        if (bytes == 0)
        {
            free_block(fs_space, block);
            return NULL;
        }
        parent->blocks += 1;
        return block;
    }
    else if (parent->blocks < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2))
    {
        if (parent->dbl_indirect == NULL)
        { // add in double indirect block if it doesnt exist yet
            parent->dbl_indirect = allocate_block(fs_space);
            if (parent->dbl_indirect == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
        }
        uint8_t *indir_node = NULL;
        uint64_t base = 12 + BLOCKSIZE / ADDR_LENGTH;
        for (uint64_t i = BLOCKSIZE / ADDR_LENGTH; i <= pow(BLOCKSIZE / ADDR_LENGTH, 2); i += BLOCKSIZE / ADDR_LENGTH)
        { // find which indir block this belongs to
            if (parent->blocks < base + i)
            {
                read_block(&indir_node, parent->dbl_indirect, ((i / (BLOCKSIZE / ADDR_LENGTH)) - 1) * ADDR_LENGTH, ADDR_LENGTH); // read indirect block address
                if (indir_node == 0)
                { // this indirect block does not exist
                    indir_node = allocate_block(fs_space);
                    if (indir_node == NULL)
                    {
                        free_block(fs_space, block);
                        return NULL;
                    }
                    write_block(&indir_node, parent->dbl_indirect, ((i / (BLOCKSIZE / ADDR_LENGTH)) - 1) * ADDR_LENGTH, ADDR_LENGTH); // write indirect block address in dbl block
                }
                uint64_t x = parent->blocks - (base + i - BLOCKSIZE / ADDR_LENGTH); // Which block is it in the indirect block
                write_block(&block, indir_node, x * ADDR_LENGTH, ADDR_LENGTH);      // write block address in indirect block
                parent->blocks += 1;
                return block;
            }
        }
        return NULL;
    }
    else // the case of triple indirect
    {
        if (parent->trpl_indirect == NULL)
        {
            parent->trpl_indirect = allocate_block(fs_space);
            if (parent->trpl_indirect == NULL)
            {
                free_block(fs_space, block);
                return NULL;
            }
        }
        uint64_t base = 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2); // starting address
        for (uint64_t i = pow(BLOCKSIZE / ADDR_LENGTH, 2); i < pow(BLOCKSIZE / ADDR_LENGTH, 3); i += pow(BLOCKSIZE / ADDR_LENGTH, 2))
        {
            if (parent->blocks < base + i)
            {
                uint8_t *dbl_blk = NULL;
                read_block(&dbl_blk, parent->trpl_indirect, (i / (pow(BLOCKSIZE / ADDR_LENGTH, 2)) - 1) * ADDR_LENGTH, ADDR_LENGTH); // read addr of dbl indirect block
                if (dbl_blk == 0)
                {
                    dbl_blk = allocate_block(fs_space);
                    if (dbl_blk == NULL)
                    {
                        free_block(fs_space, block);
                        return NULL;
                    }
                    write_block(&dbl_blk, parent->trpl_indirect, (i / (pow(BLOCKSIZE / ADDR_LENGTH, 2)) - 1) * ADDR_LENGTH, ADDR_LENGTH); // write address on dbl indir block
                }

                // Find the indirect block
                uint8_t *indir_blk = NULL;
                for (uint64_t j = BLOCKSIZE / ADDR_LENGTH; j < pow(BLOCKSIZE / ADDR_LENGTH, 2); j += BLOCKSIZE / ADDR_LENGTH)
                {
                    if (parent->blocks < base + i - pow(BLOCKSIZE / ADDR_LENGTH) + j)
                    {
                        read_block(&indir_blk, dbl_blk, (j / (BLOCKSIZE / ADDR_LENGTH) - 1), ADDR_LENGTH);
                        if (indir_blk == 0)
                        { // indir blk needs to be added
                            indir_blk = allocate_block(fs_space);
                            if (indir_blk == NULL)
                            {
                                free_block(fs_space, block);
                                return NULL;
                            }
                            write_block(&indir_blk, dbl_blk, (j / (BLOCKSIZE / ADDR_LENGTH) - 1), ADDR_LENGTH);
                        }
                        uint64_t blk_loc = parent->blocks - (base + i + j - pow((BLOCKSIZE / ADDR_LENGTH), 2) - BLOCKSIZE / ADDR_LENGTH);
                        write_block(&block, indir_blk, blk_loc, ADDR_LENGTH); // write block address
                        parent->blocks += 1;
                        return block;
                    }
                }
            }
        }
        return NULL;
    }
    return NULL;
}

/// @brief Adds address to directory block
/// @param block block of where to store address
/// @param addr ptr to inode we are adding into the directory
/// @param name name of file/directory we are adding
/// @return 0 success, -1 failure
int add_addr(uint8_t *block, node *addr, char *name)
{
    if (block == NULL)
        return -1;
    char temp[NAME_BOUNDARY - ADDR_LENGTH];
    strcpy(temp, name);
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        if (*(block + i) == 0)                                                   // nothing written here
        {                                                                        // no name so can write over
            write_block(temp, block, i, NAME_BOUNDARY - ADDR_LENGTH);            // write name
            write_block(&addr, block, NAME_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH); // write address
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
    uint64_t num_blocks = parent->blocks;
    if (num_blocks == 0) // an inode should at least have 1 block allocated
        return -1;
    if (num_blocks <= 12)//direct
    {
        int result = add_addr(parent->direct_blocks[parent->blocks - 1], child, name);
        if (result == -1) // block is full
        {
            uint8_t *blk_result = add_block_to_node(fs_space, parent);
            if (blk_result == NULL)
                return -1;
            result = add_addr(blk_result, child, name);
        }
        parent->size += result < 0 ? 0 : NAME_BOUNDARY;
        return result;
    }
    else if (num_blocks <= 12 + BLOCKSIZE / ADDR_LENGTH)//indirect
    {
        uint64_t loc = parent->blocks - 12;
        uint8_t *blk_from_indir = NULL;
        read_block(&blk_from_indir, parent->indirect_blocks, (parent->blocks - 12 - 1) * ADDR_LENGTH, ADDR_LENGTH);
        if (blk_from_indir == 0)
            blk_from_indir = add_block_to_node(fs_space, parent);
        int result = add_addr(blk_from_indir, child, name);
        if (result == -1) // block is full
        {
            uint8_t *blk_result = add_block_to_node(fs_space, parent);
            if (blk_result == NULL) // no more blocks to add to parent
                return -1;
            result = add_addr(blk_result, child, name);
        }
        parent->size += result < 0 ? 0 : NAME_BOUNDARY;
        return result;
    }
    else if (num_blocks <= 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2))//double indirect
    {
        uint64_t base = 12 + BLOCKSIZE / ADDR_LENGTH;
        for (uint64_t i = 0; i <= pow(BLOCKSIZE / ADDR_LENGTH, 2); i += BLOCKSIZE / ADDR_LENGTH)
        {
            if (parent->blocks <= base + i)
            {
                uint8_t *indir_blk = NULL;
                read_block(&indir_blk, parent->dbl_indirect, (i / (BLOCKSIZE / ADDR_LENGTH) - 1) * ADDR_LENGTH, ADDR_LENGTH);
                uint64_t loc = parent->blocks - (base + i - BLOCKSIZE / ADDR_LENGTH) - 1;
                uint8_t *blk_result = NULL;
                read_block(&blk_result, indir_blk, loc * ADDR_LENGTH, ADDR_LENGTH);
                int result = add_addr(blk_result, child, name);
                if (result == -1)
                {
                    blk_result = add_block_to_node(fs_space, parent);
                    result = add_addr(blk_result, child, name);
                }
                parent->size += result < 0 ? 0 : NAME_BOUNDARY;
                return result;
            }
        }
    }
    else//triple indirect
    {
        uint64_t base = 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE/ADDR_LENGTH, 2);
        for(uint64_t i = 0; i <= pow(BLOCKSIZE/ADDR_LENGTH, 3), i+=pow(BLOCKSIZE/ADDR_LENGTH,2)){
            if(parent->blocks <= base + i){
                for(uint64_t j = 0; j < pow(BLOCKSIZE/ADDR_LENGTH, 2); j+=BLOCKSIZE/ADDR_LENGTH){
                    if(parent->blocks <= base+i+j-pow(BLOCKSIZE/ADDR_LENGTH,2)){
                        uint8_t * dbl_dir, * indir_blk, * blk_result;
                        read_block(&dbl_dir, parent->trpl_indirect, (i/(pow(BLOCKSIZE/ADDR_LENGTH,2))-1)*ADDR_LENGTH, ADDR_LENGTH);
                        read_block(&indir_blk, dbl_dir, (j/(BLOCKSIZE/ADDR_LENGTH)-1)*ADDR_LENGTH, ADDR_LENGTH);
                        uint64_t loc = parent->blocks - (base+i+j-pow(BLOCKSIZE/ADDR_LENGTH, 2)-BLOCKSIZE/ADDR_LENGTH) - 1;
                        read_block(&blk_result, indir_blk, loc, ADDR_LENGTH);
                        int result = add_addr(blk_result, child, name);
                        if(result == -1){
                            blk_result = add_block_to_node(fs_space, parent);
                            result = add_addr(blk_result, child, name);
                        }
                        parent->size += result < 0 ? 0 : NAME_BOUNDARY;
                        return result;
                    }
                }
            }
        }
    }
    return -1;
}

/*
@brief check directory block to see if it has a matching file/directory
and return address
@param block the block containing the directory table
@param name name of directory or file
@return inode ptr to
*/
void *check_block(uint8_t *block, char *name)
{
    if (block == NULL)
        return NULL;
    char tmp_name[NAME_BOUNDARY - ADDR_LENGTH + 1];
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        memcpy(tmp_name, block + i, NAME_BOUNDARY - ADDR_LENGTH); // copy name

        if (strcmp(tmp_name, name) == 0) // found a match in the name
        {
            node *node_ptr;
            read_block(&node_ptr, block, i + NAME_BOUNDARY - ADDR_LENGTH, sizeof(node_ptr)); // write address
            return node_ptr;
        }
    }
    return NULL;
}

/*
@brief check indirect block for address
@param indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
void *check_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
{
    if (block == NULL)
        return NULL;
    node *tmp_node = NULL;
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
            return NULL;
        *block_count = *block_count - 1;
        uint8_t *block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH); // write address
        tmp_node = check_block(block_addr, name);
        if (tmp_node != NULL)
            return tmp_node;
    }
    return NULL;
}

/*
@brief check double indirect block for address
@param double indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
void *check_dbl_indirect_blk(uint8_t *block, char *name, int64_t *block_count)
{
    if (block == NULL)
        return NULL;
    node *tmp_node = NULL;
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
            return NULL;
        uint8_t *block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        tmp_node = check_indirect_block(block_addr, name, block_count);
        if (tmp_node != NULL)
            return tmp_node;
    }
    return NULL;
}

/*
@brief check triple indirect block for address
@param triple indirect block
@param name name of directory or file
@param block_count number of blocks left to check based on original node we are checking
@return inode ptr to
*/
void *check_trpl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
{
    if (block == NULL)
        return NULL;
    node *tmp_node = NULL;
    for (int i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
    {
        if (*block_count == 0)
            return NULL;
        uint8_t *block_addr;
        memcpy(&block_addr, block + i, ADDR_LENGTH);
        tmp_node = check_dbl_indirect_blk(block_addr, name, block_count);
        if (tmp_node != NULL)
            return tmp_node;
    }
    return NULL;
}

/*
@brief Given a path, attempt to find the corresponding inode
@return pointer to inode, NULL upon failure
*/
void *find_path_node(char *path)
{
    char cpy_path[NAME_BOUNDARY], *node_name;
    node *cur_node = root;
    uint8_t *block;

    strcpy(cpy_path, path);
    node_name = strtok(cpy_path, "/");

    while (node_name != 0) // should break this loop if you find the full path
    {
        node *tmp_node = NULL;
        uint64_t block_cnt = cur_node->blocks; // check how many blocks inode uses to limit blocks checked
        for (int i = 0; i < 12; i++)
        {
            if (block_cnt == 0) // no more blocks means you didnt find out
                return NULL;
            if (cur_node->direct_blocks[i] == NULL) // bad case
                return NULL;
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

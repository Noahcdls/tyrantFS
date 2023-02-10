#include "disk.h"
#include "tyrant_help.h"

int add_block_to_node(void *fs_space, node *parent)
{
    if (parent == NULL)
        return -1;
    uint8_t *block = allocate_block(fs_space);
    if (block == NULL)
        return;
    if (parent->blocks == UINT64_MAX)
    {
        free_block(fs_space, block);
        return -1;
    }
    if (parent->blocks < 12)
    {
        parent->direct_blocks[parent->blocks - 1] = block;
        parent->blocks += 1;
        return 0;
    }
    else if (parent->blocks < 12 + BLOCKSIZE / ADDR_LENGTH)
    {
        if (parent->indirect_blocks == NULL)
        { // add block where necessary
            parent->indirect_blocks = allocate_block(fs_space);
            if (parent->indirect_blocks == NULL)
            {
                free_block(fs_space, block);
                return -1;
            }
        }
        uint32_t bytes = write_block(&block, parent->indirect_blocks, (parent->blocks - 12) * ADDR_LENGTH - ADDR_LENGTH, ADDR_LENGTH);
        if (bytes == 0)
        {
            free_block(fs_space, block);
            return -1;
        }
        parent->blocks += 1;
        return 0;
    }
    else if (parent->blocks < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2))
    {
        if (parent->dbl_indirect == NULL)
        {
            parent->dbl_indirect = allocate_block(fs_space);
            if (parent->dbl_indirect == NULL)
            {
                free_block(fs_space, block);
                return -1;
            }
        }
        node * indir_node = NULL;
        uint64_t base = 12 + BLOCKSIZE / ADDR_LENGTH;
        for(int i = BLOCKSIZE/ADDR_LENGTH; i <= pow(BLOCKSIZE/ADDR_LENGTH, 2); i += BLOCKSIZE/ADDR_LENGTH){//find which indir block this belongs to
            if(parent->blocks < base + i){
                read_block(&indir_node, parent->dbl_indirect, ((i/(BLOCKSIZE/ADDR_LENGTH))-1)*ADDR_LENGTH, ADDR_LENGTH);//read indirect block address
                if(indir_node == 0){//this indirect block does not exist
                    indir_node = allocate_block(fs_space);
                    if(indir_node == NULL){
                        free_block(fs_space, indir_node);
                        free_block(fs_space, block);
                        return -1;
                    }
                    write_block(&indir_node, parent->dbl_indirect, (parent->blocks -(base+i-BLOCKSIZE/ADDR_LENGTH))*ADDR_LENGTH, ADDR_LENGTH);//write indirect block address in dbl block
                }
                int x = parent->blocks -(base+i-BLOCKSIZE/ADDR_LENGTH);//STUB
                write_block(&block, indir_node, x*ADDR_LENGTH, ADDR_LENGTH);//write block address in indirect block
                return 0;
            }

        }
        return -1;
    }
    else
    {
        return 0; // STUB
    }

    return 0;
}

int add_addr(uint8_t *block, node *addr, char *name)
{
    if (block == NULL)
        return;
    char temp[NAME_BOUNDARY - ADDR_LENGTH];
    strcpy(temp, name);
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        if (*(block + i) == 0)
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
int add_child(void *fs_space, node *parent, node *child, char *name)
{
    uint64_t num_blocks = parent->blocks;
    if (num_blocks == 0) // an inode should at least have 1 block allocated
        return -1;
    if (num_blocks <= 12)
    {
        int result = add_addr(parent->direct_blocks[parent->blocks - 1], child, name);
        add_block_to_node(fs_space, parent);
    }
    if (num_blocks <= 12 + BLOCKSIZE / ADDR_LENGTH)
    {
        return 0;
    }
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

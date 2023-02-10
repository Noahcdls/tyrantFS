#include "disk.h"
#include "tyrant_help.h"

/*
@brief add a child inode to a parent directory
@param parent parent inode
@param child child inode
@return 0 for success, -1 failure
*/
int add_child(node *parent, node *child)
{
    uint64_t num_blocks = parent->blocks;
    if (num_blocks < 13)
    {
        for (int i = 0; i < 12; i++)
        {
            if (parent->direct_blocks[i] != NULL)
            {
                return -1;
            }
        }
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
    char tmp_name[NAME_BOUNDARY - 8 + 1];
    for (int i = 0; i < BLOCKSIZE; i += NAME_BOUNDARY)
    {
        strcpy(tmp_name, block + i);
        if (strcmp(tmp_name, name) == 0)
        {
            node *node_ptr;
            read_block(node_ptr, block, NAME_BOUNDARY - 8, sizeof(node_ptr)); // write address
            return node_ptr;
        }
    }
    return NULL;
}

void *find_path_node(char *path)
{
    char cpy_path[NAME_BOUNDARY], *node_name;
    node *cur_node = root;
    uint8_t *block;

    strcpy(cpy_path, path);
    node_name = strtok(cpy_path, "/");
    node *tmp_node;
    while (node_name != 0)//should break this loop if you find the full path
    {
        uint64_t block_cnt = cur_node->blocks;//check how many blocks inode uses to limit blocks checked
        for (int i = 0; i < 12; i++)
        {
            if (block_cnt == 0)//no more blocks means you didnt find out
                return NULL;
            if (cur_node->direct_blocks[i] == NULL)//bad case
                return NULL;
            tmp_node = check_block(cur_node->direct_blocks[i], node_name);//check block for addresses
            block_cnt--;
            if (tmp_node != NULL)//found the next inode
                break;
        }
        if (tmp_node != NULL)//broke out of for loop and have a valid inode
        {
            cur_node = tmp_node;
            node_name = strtok(NULL, "/");
            continue;
        }
        if(block_cnt == 0)
            return NULL;
        




    }
}

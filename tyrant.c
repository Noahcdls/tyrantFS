#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "include/fuse.h"
#include "tyrant.h"
#include "disk.h"
#include "tyrant_help.h"

uint8_t *memspace;
/*
@brief make a directory path by allocating an inode and adding it to the directory list
@param path pathname of new directory
@param m Permissions passed by system, 32-bit
@note TODO: Find parent path to determine where to add this file

*/
int tfs_mkdir(const char *path, mode_t m)
{

    node *parent_node = NULL; // TODO: FIND PARENT NODE
    char *temp = malloc(sizeof(char) * (strlen(path) + 1));

    int i;
    strcpy(temp, path);
    for (i = strlen(path) - 1; temp[i] != '/' && i >= 0; i--)
        ; // check for last / to differentiate parent path from new directory
    if (i == -1)
    {
        free(temp);
        return -1; // invalid path
    }

    temp[i] = 0; // terminate at / for full path

    if (i != 0) // '/' or root is the first byte
        parent_node = find_path_node(temp);
    else
        parent_node = root;
    if (parent_node == NULL)
    {
        free(temp);
        return -1;
    }

    ///////////////////////////////////////PARENT SEARCH END

    node *dir_node = allocate_inode(memspace); // get inode
    if (dir_node == NULL)
    {
        free(temp);
        return -1;
    }
    uint8_t *block = allocate_block(memspace); // get block
    if (block == NULL)
    {
        free_inode(dir_node); // free node if we cant make a full directory
        free(temp);
        return -1;
    }
    // https://jameshfisher.com/2017/02/24/what-is-mode_t/
    // what mode_t means
    dir_node->mode = m; // definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    dir_node->mode |= S_IFDIR;
    dir_node->direct_blocks[0] = block;
    dir_node->blocks = 1;
    write_block(".", block, 0, sizeof("."));
    write_block(&dir_node, block, MAX_NAME_LENGTH, sizeof(dir_node));

    write_block("..", block, PATH_BOUNDARY, sizeof(".."));
    write_block(&parent_node, block, 2 * PATH_BOUNDARY - sizeof(uint8_t *), sizeof(parent_node)); // write address of parent node

    int result = add_child(memspace, parent_node, dir_node, (temp+i+1));
    if (!result)
    {
        free_block(memspace, block); // need memspace since we need to write on disk/working memory too
        free_inode(dir_node);
    }
    free(temp);
    return result;
}
void tfs_getattr()
{
}

static struct fuse_operations operations = {
    .mkdir = tfs_mkdir,
    // .getattr = tfs_getattr,
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

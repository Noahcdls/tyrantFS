#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "include/fuse.h"
#include "tyrant.h"

uint8_t *memspace;

/*
@brief make a directory path by allocating an inode and adding it to the directory list
@param path pathname of new directory
@param m Permissions passed by system, 32-bit
@note TODO: Find parent path to determine where to add this file

*/
int tfs_mkdir(const char * path, mode_t m)
{



    node * dir_node = allocate_inode(memspace);
    node * parent_node = NULL;//TODO: FIND PARENT NODE
    if(dir_node == NULL)
        return -1;
    uint8_t * block = allocate_block(memspace);
    if(block == NULL){
        free_inode(dir_node);//free node if we cant make a full directory
        return -1;
    }

    //https://jameshfisher.com/2017/02/24/what-is-mode_t/
    //what mode_t means
    dir_node->mode = m;//definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    dir_node->mode |= S_IFDIR;
    write_block(".", block, 0, sizeof("."));
    write_block(&dir_node, block, MAX_NAME_LENGTH, sizeof(dir_node));

    write_block("..", block, PATH_BOUNDARY, sizeof(".."));
    write_block(&parent_node, )





    return 0;
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

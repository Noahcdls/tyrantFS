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

*/
int tfs_mkdir(const char * path, mode_t m)
{
    node * dir_node = allocate_inode(memspace);
    if(dir_node == NULL)
        return -1;
    //https://jameshfisher.com/2017/02/24/what-is-mode_t/
    //what mode_t means
    dir_node->mode = m;//definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    dir_node->mode |= S_IFDIR;



    
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

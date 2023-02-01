#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "include/fuse.h"
#include "tyrant.h"

uint8_t * memspace;


void tfs_mkdir(){

}
void tfs_getattr(){

}

static struct fuse_operations operations = {
    .mkdir = tfs_mkdir,
    .getattr = tfs_getattr,
    .readdir = tfs_readdir,
    .rmdir = tfs_rmdir,
    .open = tfs_open,
    .read = tfs_read,
    .write = tfs_write,
    .create = tfs_create,
    .rename = tfs_rename
    .unlink = tfs_unlink

};


int main(int argc, char * argv[]){
    memspace = malloc(MEMSIZE);



}






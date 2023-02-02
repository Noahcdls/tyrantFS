#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "include/fuse.h"
#include "tyrant.h"

uint8_t * memspace;
/*
@brief mkfs for tyrant DEFAULT
@param fs_space pointer to where beginning of memory space to write over
*/
void tfs_mkfs(void * fs_space){
    for(int i = 1; i < END_OF_INODE; i++){
        for(int j = 0; j < BLOCKSIZE; j+=128){
            node * inode_init = fs_space + 4096*i + j;//set up a basic node
            bzero(inode_init, 128);//clear out inode info
        }
    }
    *((uint32_t *)fs_space) = END_OF_INODE;//first superblock
    uint32_t* free_blockptr = fs_space + END_OF_INODE*BLOCKSIZE; //shift pointer 
    uint32_t block_counter = END_OF_INODE;
    for(int i = 0; i < 3; i++){//fill three blocks with addresses for fun
        for(int j = 0; j < BLOCKSIZE/4; j++){//fill entire block
            *free_blockptr = block_counter+1;//write in next free block
            block_counter++;//increment next available block
            if(j == BLOCKSIZE/4-1){//at end before reiterate
                free_blockptr = block_counter*BLOCKSIZE;//jump to the next block
                block_counter++;//additional increment since we used block counter to hold next free block
            }
            else{
                free_blockptr++;//go to next 4 bytes in block
            }
        }
    }


}



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






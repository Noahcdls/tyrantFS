#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "disk.h"


uint8_t *memspace;

int has_no_overlap(void *first_node, void *second_node){
    if (first_node==NULL || second_node==NULL){
        return 0;
    }
    return (first_node<=second_node && first_node + INODE_SIZE_BOUNDARY <= second_node) || 
    (first_node>second_node && second_node + INODE_SIZE_BOUNDARY <= first_node);
}

int stays_in_block(void *inode){
    if (inode==NULL){
        return 0;
    }
    for (int i = 1; i< END_OF_INODE;i+=1){
        uint8_t *start_block = memspace + i*BLOCKSIZE;
        uint8_t *next_block = memspace + (i+1)*BLOCKSIZE;
        if (inode>=start_block && inode<next_block){
            uint8_t *last_byte_of_node = (inode + INODE_SIZE_BOUNDARY-1);
            return last_byte_of_node>start_block && last_byte_of_node<next_block;
        }
    }
    return 0;
}

void test_tfs_mkfs()
{
    int result = tfs_mkfs(NULL);
    assert(result == -1);

    int result2 = tfs_mkfs(memspace);
    assert(result2 == 0);

    //check that all inodes are free
    for (int i = BLOCKSIZE; i< END_OF_INODE*BLOCKSIZE; i+=INODE_SIZE_BOUNDARY){
        node *inode_ptr = memspace+i;
        assert (inode_ptr->mode == 0);
    }
    printf("Past mkfs test");

}

void test_allocate_and_free_inode()
{
    //check that inodes can be allocated and they do not cross blocks
    node *inode_ptr = allocate_inode(memspace);
    node *first_inode = inode_ptr;
    node *last_inode;
    assert(inode_ptr!=NULL);
    //check that the inode stays in one block
    assert(stays_in_block(inode_ptr));
    //set mode to nonzero, so that new inode will be allocated.
    inode_ptr->mode = 1;

    while (inode_ptr!=NULL)
    {
        node *temp_node = allocate_inode(memspace);
        if (temp_node != NULL){
            temp_node->mode = 1;
            //assert that there's no overlap between the current inode and last inode
            assert(has_no_overlap(inode_ptr,temp_node));
            //assert that the inode does not cross block boundary
            assert(stays_in_block(temp_node));
            last_inode = temp_node;
        }
        inode_ptr = temp_node;
    }
    // test that we can free the first and last inode, and reallocate them back
    int status = free_inode(first_inode);
    assert(status == 0);
    assert(first_inode->mode == 0);

    int status2 = free_inode(last_inode);
    assert(status2 == 0);
    assert(last_inode->mode == 0);

    first_inode = allocate_inode(memspace);
    assert(first_inode != NULL);
    first_inode->mode = 1;

    last_inode = allocate_inode(memspace);
    assert(last_inode != NULL);
    last_inode->mode = 1;

    // after this, there shouldn't be any more free nodes.

    node *null_ptr = allocate_inode(memspace);
    assert(null_ptr == NULL);

    printf("Passed allocate/free inode test.");

    //Free some inodes for future tests
    free_inode(first_inode);
    free_inode(last_inode);
}

void test_read_and_write_inode()
{
    //alocate an inode and set some attributes
    node *inode_ptr = allocate_inode(memspace);
    inode_ptr->mode = 1;
    inode_ptr->links = 1;

    //create a buffer, read the inode, and check the attributes
    uint8_t *buffer = malloc(INODE_SIZE_BOUNDARY);
    int status = read_inode(inode_ptr, buffer);
    assert(status == 0);
    node *new_node = buffer;
    assert(new_node->mode == 1);
    assert(new_node->links == 1);

    new_node->mode = 2;
    new_node->links = 2;

    int write_status = write_inode(inode_ptr, buffer);
    assert(write_status == 0);

    status = read_inode(inode_ptr, buffer);
    new_node = buffer;
    assert(new_node->mode == 2);
    assert(new_node->links == 2);

    printf("Past read and write inode test.");
}

void test_allocate_block()
{

}

void test_free_block()
{

}

void test_read_block()
{

}

void test_write_block()
{

}


int main(int argc, char *argv[])
{
    memspace = malloc(MEMSIZE);

    test_tfs_mkfs();
    test_allocate_and_free_inode();
    test_read_and_write_inode();

    printf("All tests passed.");
}
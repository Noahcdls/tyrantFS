#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "disk.h"


uint8_t *memspace;

int has_no_overlap(void *first_node, void *second_node){
    node *node1 = first_node;
    node *node2 = second_node;
    if (node1==NULL || node2==NULL){
        return 0;
    }
    return (node1<=node2 && node1->checksum + sizeof(node1->checksum) <= node2) || 
    (node1>node2 && node2->checksum + sizeof(node2->checksum) <= node1);
}

int stays_in_block(void *inode){
    node *node_ptr = inode;
    if (inode==NULL){
        return 0;
    }
    for (int i = 1; i< END_OF_INODE;i+=1){
        uint8_t *start_block = memspace + i*BLOCKSIZE;
        uint8_t *next_block = memspace + (i+1)*BLOCKSIZE;
        if (node_ptr>start_block && node_ptr<next_block){
            uint8_t *last_byte_of_node = (node_ptr->checksum + sizeof(node_ptr->checksum)-1);
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
    assert(result == 0);

    //check that all inodes are free
    for (int i = BLOCKSIZE; i< END_OF_INODE*BLOCKSIZE; i+=INODE_SIZE_BOUNDARY){
        node *inode_ptr = memspace+i;
        assert (inode_ptr->mode == 0);
    }


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

}

void test_read_inode()
{

}

void test_write_inode()
{

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
    test_allocate_inode();

    printf("All tests passed.");
}
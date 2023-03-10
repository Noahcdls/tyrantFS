#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "disk.h"

uint8_t *memspace;

void test_allocate_and_free_block()
{
    uint8_t* block1 = allocate_block(memspace);
    assert(block1 != NULL); //Verify that the function returned something

    int free_result = free_block(memspace, block1);
    assert(free_result == 0); //Verify that free operation completed successfully

    uint8_t* block2 = allocate_block(memspace);
    assert(block2 == block1); //Verify that the initial block was added back to the head of the free list, then allocated again

    int free_result2 = free_block(memspace, block2);
    assert(free_result2 == 0); //Free block again

    uint8_t* blocks[1024] = { 0 };
    for(int i = 0; i < NUM_FREE_BLOCKS; i++) //Allocate all blocks in filesystem, one block is already in use by root directory
    {
        blocks[i] = allocate_block(memspace);
    }
    assert(blocks[1023] != 0); //Verify that all possible blocks were allocated

    uint8_t* test_block = allocate_block(memspace);
    assert(test_block == NULL); //Verify that after all blocks are allocated NULL is returned

    for(int i = 0; i < NUM_FREE_BLOCKS; i++) //Free all blocks
    {
        int free_result_loop = free_block(memspace, blocks[i]);
        assert(free_result_loop == 0);
    }

    for(int i = 0; i < NUM_FREE_BLOCKS; i++) //Allocate all the blocks again, to verify the freelist can grow
    {
        blocks[i] = allocate_block(memspace);
    }
    assert(blocks[1023] != 0);

    test_block = allocate_block(memspace);
    assert(test_block == NULL); //Verify that after all blocks are allocated again NULL is returned

    for(int i = 0; i < NUM_FREE_BLOCKS; i++) //Free all blocks again
    {
        int free_result_loop = free_block(memspace, blocks[i]);
        assert(free_result_loop == 0);
    }
}

void test_read_and_write_block()
{
    uint8_t* test_block = allocate_block(memspace);

    char test[7] = "Testing";
    int result1 = write_block(test, test_block, 3, 7);
    assert(result1 = 7);

    char read_buf[7];
    int result2 = read_block(read_buf, test_block, 3, 7);
    assert(result2 == 7);
    assert(strcmp(read_buf, test));

    int result3 = write_block(test, test_block, 4090, 7);
    assert(result3 = 6);

    int result4 = read_block(read_buf, test_block, 4090, 7);
    assert(result4 == 6);
    assert(strcmp(read_buf, "Testin"));

    free_block(memspace, test_block);
}

int main(int argc, char *argv[])
{
    memspace = malloc(MEMSIZE);
    int result = tfs_mkfs(memspace);
    assert(result == 0); //Assert that mkfs executed successfully
    test_allocate_and_free_block();
    test_read_and_write_block();

    printf("Tests executed successfully");
}
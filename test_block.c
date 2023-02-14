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

    uint8_t* blocks[64];
    for(int i = 0; i < NUM_FREE_BLOCKS; i++)
    {
        blocks[i] = allocate_block(memspace);
    }
    assert(blocks[63] != 0); //Verify that all 64 possible blocks were allocated

    uint8_t* test_block = allocate_block(memspace);
    assert(test_block == NULL); //Verify that after all blocks are allocated NULL is returned

    for(int i = 0; i < NUM_FREE_BLOCKS; i++)
    {
        int free_result_loop = free_block(memspace, blocks[i]);
        assert(free_result_loop == 0);
    }
}

int main(int argc, char *argv[])
{
    memspace = malloc(MEMSIZE);
    int result = tfs_mkfs(memspace);
    assert(result == 0); //Assert that mkfs executed successfully
    test_allocate_and_free_block();


    printf("Tests executed successfully");
}
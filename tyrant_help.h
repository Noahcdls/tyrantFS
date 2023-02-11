#include "disk.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>


int add_to_directory(void * fs_space, node * parent, node * child, char * name);
void * find_path_node(char * path);
void *  add_block_to_node(void *fs_space, node *parent);
int add_addr(uint8_t *block, node *addr, char *name);
void *check_block(uint8_t *block, char *name);
void *check_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)
void *check_dbl_indirect_blk(uint8_t *block, char *name, int64_t *block_count)
void *check_trpl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count)




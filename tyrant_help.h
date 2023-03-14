#include "disk.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>


int add_to_directory(uint64_t parent, uint64_t child, char *name);
uint64_t find_path_node(char *path);
uint64_t add_block_to_node(node *parent, uint64_t parent_loc);
int add_addr(uint64_t parent, uint64_t block, uint64_t addr, char *name);
uint64_t check_block(uint8_t *block, char *name);
uint64_t check_indirect_blk(uint8_t *block, char *name, uint64_t *block_count);
uint64_t check_dbl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count);
uint64_t check_trpl_indirect_blk(uint8_t *block, char *name, uint64_t *block_count);
uint64_t get_i_block(node *cur_node, uint64_t i);
int remove_link_from_parent(uint64_t parent, uint64_t cur_node);
int sub_unlink(uint64_t parent, uint64_t child);
uint64_t get_current_time_in_nsec();
int rename_from_parent(uint64_t parent, uint64_t cur_node, char* new_name);


#include "disk.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>


int add_child(void * fs_space, node * parent, node * child, char * name);
void * find_path_node(char * path);



#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "include/fuse.h"
#include "tyrant.h"
#include "disk.h"
#include "tyrant_help.h"

uint8_t *memspace;
/*
@brief make a directory path by allocating an inode and adding it to the directory list
@param path pathname of new directory
@param m Permissions passed by system, 32-bit
@note TODO: Find parent path to determine where to add this file
*/
int tfs_mkdir(const char *path, mode_t m)
{

    node *parent_node = NULL;
    char *temp = malloc(sizeof(char) * (strlen(path) + 1));

    int i;
    strcpy(temp, path);
    for (i = strlen(path) - 1; temp[i] != '/' && i >= 0; i--)
        ; // check for last / to differentiate parent path from new directory
    if (i == -1)
    {
        free(temp);
        return -1; // invalid path
    }

    temp[i] = 0; // terminate at / for full path

    if (i != 0) // '/' or root is the first byte
        parent_node = find_path_node(temp);
    else
        parent_node = root;
    if (parent_node == NULL)
    {
        free(temp);
        return -1;
    }

    ///////////////////////////////////////PARENT SEARCH END

    node *dir_node = allocate_inode(memspace); // get inode
    if (dir_node == NULL)
    {
        free(temp);
        return -1;
    }
    uint8_t *block = allocate_block(memspace); // get block
    if (block == NULL)
    {
        free_inode(dir_node); // free node if we cant make a full directory
        free(temp);
        return -1;
    }
    // https://jameshfisher.com/2017/02/24/what-is-mode_t/
    // what mode_t means
    dir_node->creation_time = time(NULL);
    dir_node->mode = m; // definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    dir_node->mode |= S_IFDIR;
    dir_node->direct_blocks[0] = block;
    dir_node->blocks = 1;
    dir_node->links = 1;
    write_block(".", block, 0, sizeof("."));
    write_block(&dir_node, block, MAX_NAME_LENGTH, sizeof(dir_node));

    write_block("..", block, PATH_BOUNDARY, sizeof(".."));
    write_block(&parent_node, block, 2 * PATH_BOUNDARY - sizeof(uint8_t *), sizeof(parent_node)); // write address of parent node
    dir_node->size = PATH_BOUNDARY * 2;
    int result = add_to_directory(memspace, parent_node, dir_node, (temp + i + 1));
    if (!result)
    {
        free_block(memspace, block); // need memspace since we need to write on disk/working memory too
        free_inode(dir_node);
    }
    free(temp);
    return result;
}

int tfs_mknod(const char *path, mode_t m, dev_t d)
{
    node *parent_node = NULL;
    char *temp = malloc(sizeof(char) * (strlen(path) + 1)); // for us to hold path

    int i;
    strcpy(temp, path);
    for (i = strlen(path) - 1; temp[i] != '/' && i >= 0; i--)
        ; // check for last / to differentiate parent path from new directory
    if (i == -1)
    {
        free(temp);
        return -1; // invalid path
    }

    temp[i] = 0; // terminate at / for full path

    if (i != 0) // '/' or root is the first byte
        parent_node = find_path_node(temp);
    else
        parent_node = root;
    if (parent_node == NULL)
    {
        free(temp);
        return -1;
    }

    ///////////////////////////////////////PARENT SEARCH END

    node *data_node = allocate_inode(memspace); // get inode
    if (data_node == NULL)
    {
        free(temp);
        return -1;
    }
    // https://jameshfisher.com/2017/02/24/what-is-mode_t/
    // what mode_t means
    // dev_t is device ID
    data_node->mode = m; // definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    data_node->links = 1;
    data_node->creation_time = time(NULL);
    int result = add_to_directory(memspace, parent_node, data_node, (temp + i + 1));
    if (!result)
    {
        free_inode(data_node);
    }
    free(temp);
    return result;
}

/// @brief Put path names into buffer using filler
/// @param path file path
/// @param buffer buffer that will be used to add path to DIRENT for readdir
/// @param filler function that fills buffer appropriately
/// @param offset not relevant. Read readdir in fuse.h to see what it does
/// @param fi not relevant
/// @param fuse_flag extra features in current fuse release we are ignoring
/// @return 0 success, -1 failure
int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info *fi, enum fuse_readdir_flags fuse_flag)
{
    node *dir = find_path_node(path);
    if (dir == NULL)
        return -1;

    uint64_t block_count = 0;
    uint64_t byte_count = 0;
    uint64_t total_blocks = dir->blocks;
    uint64_t total_bytes = dir->size;
    uint8_t *cur_blk;
    char name[MAX_NAME_LENGTH];
    while (block_count < total_blocks && byte_count < total_bytes)
    {
        if (block_count < 12)
        {
            cur_blk = dir->direct_blocks[block_count];
            if (cur_blk == NULL)
                return -1;
            for (uint64_t i = 0; i < BLOCKSIZE; i += PATH_BOUNDARY)
            {
                if (total_bytes < byte_count)
                    return 0;
                read_block(name, cur_blk, i, MAX_NAME_LENGTH);
                if (name[0] != '\0')
                    filler(buffer, name, NULL, 0, 0);
                byte_count += PATH_BOUNDARY;
            }
            block_count++;
        }

        else if (block_count < 12 + BLOCKSIZE / ADDR_LENGTH)
        {
            uint8_t *indir_blk = dir->indirect_blocks;
            if (indir_blk == NULL)
                return -1;
            for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
            {
                if (block_count > total_blocks || byte_count > total_bytes) // exit
                    return 0;

                memcpy(&cur_blk, indir_blk + i, ADDR_LENGTH); // get next block
                if (cur_blk == 0 || cur_blk == NULL)          // skip if invalid address
                {
                    block_count++;
                    byte_count += BLOCKSIZE;
                    continue;
                }

                for (uint64_t j = 0; j < BLOCKSIZE; j += PATH_BOUNDARY) // read block addresses
                {
                    if (total_bytes < byte_count)
                        return 0;
                    read_block(name, cur_blk, j, MAX_NAME_LENGTH);
                    if (name[0] != '\0')
                        filler(buffer, name, NULL, 0, 0);
                    byte_count += PATH_BOUNDARY;
                }
                block_count++;
            }
        }

        else if (block_count < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2))
        {
            uint8_t *indir_blk = NULL;
            uint8_t *dbl_blk = dir->dbl_indirect;
            if (dbl_blk == NULL)
                return -1;
            for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
            {
                if (block_count > total_blocks || byte_count > total_bytes) // exit
                    return 0;
                memcpy(&indir_blk, dbl_blk + i, ADDR_LENGTH);
                if (indir_blk == 0 || indir_blk == NULL)
                {
                    byte_count += BLOCKSIZE / ADDR_LENGTH * BLOCKSIZE;
                    block_count += BLOCKSIZE / ADDR_LENGTH;
                    continue;
                }

                for (int j = 0; j < BLOCKSIZE; j += ADDR_LENGTH)
                {
                    if (block_count > total_blocks || byte_count > total_bytes) // exit
                        return 0;

                    memcpy(&cur_blk, indir_blk + j, ADDR_LENGTH); // get next block
                    if (cur_blk == 0 || cur_blk == NULL)          // skip if invalid address
                    {
                        block_count++;
                        byte_count += BLOCKSIZE;
                        continue;
                    }

                    for (uint64_t k = 0; k < BLOCKSIZE; k += PATH_BOUNDARY) // read block addresses
                    {
                        if (total_bytes < byte_count)
                            return 0;
                        read_block(name, cur_blk, k, MAX_NAME_LENGTH);
                        if (name[0] != '\0')
                            filler(buffer, name, NULL, 0, 0);
                        byte_count += PATH_BOUNDARY;
                    }
                    block_count++;
                }
            }
        }

        else if (block_count < 12 + BLOCKSIZE / ADDR_LENGTH + pow(BLOCKSIZE / ADDR_LENGTH, 2) + pow(BLOCKSIZE / ADDR_LENGTH, 3))
        {
            uint8_t *indir_blk = NULL;
            uint8_t *dbl_blk = NULL;
            uint8_t *trpl_blk = dir->trpl_indirect;
            if (trpl_blk == NULL)
                return -1;
            for (uint64_t trpl = 0; trpl < BLOCKSIZE; trpl += ADDR_LENGTH)
            {
                if (block_count > total_blocks || byte_count > total_bytes) // exit
                    return 0;
                memcpy(&dbl_blk, trpl_blk + trpl, ADDR_LENGTH);
                if (dbl_blk == 0 || dbl_blk == NULL)
                {
                    byte_count += pow(BLOCKSIZE / ADDR_LENGTH, 2) * BLOCKSIZE;
                    block_count += pow(BLOCKSIZE / ADDR_LENGTH, 2);
                    continue;
                }

                for (uint64_t i = 0; i < BLOCKSIZE; i += ADDR_LENGTH)
                {
                    if (block_count > total_blocks || byte_count > total_bytes) // exit
                        return 0;
                    memcpy(&indir_blk, dbl_blk + i, ADDR_LENGTH);
                    if (indir_blk == 0 || indir_blk == NULL)
                    {
                        byte_count += BLOCKSIZE / ADDR_LENGTH * BLOCKSIZE;
                        block_count += BLOCKSIZE / ADDR_LENGTH;
                        continue;
                    }

                    for (int j = 0; j < BLOCKSIZE; j += ADDR_LENGTH)
                    {
                        if (block_count > total_blocks || byte_count > total_bytes) // exit
                            return 0;

                        memcpy(&cur_blk, indir_blk + j, ADDR_LENGTH); // get next block
                        if (cur_blk == 0 || cur_blk == NULL)          // skip if invalid address
                        {
                            block_count++;
                            byte_count += BLOCKSIZE;
                            continue;
                        }

                        for (uint64_t k = 0; k < BLOCKSIZE; k += PATH_BOUNDARY) // read block addresses
                        {
                            if (total_bytes < byte_count)
                                return 0;
                            read_block(name, cur_blk, k, MAX_NAME_LENGTH);
                            if (name[0] != '\0')
                                filler(buffer, name, NULL, 0, 0);
                            byte_count += PATH_BOUNDARY;
                        }
                        block_count++;
                    }
                }
            }
        }
    } // end while
    return 0;
}

int tfs_open(const char *path, struct fuse_file_info *fi){
    node * cur_node = find_path_node(path);
    if(cur_node == NULL)
        return -1;
    uint32_t flags = 0;
    switch(fi->flags & O_ACCMODE){
        case O_RDWR:
            flags = flags | 0666;
            break;
        case O_RDONLY:
            flags = flags | 0444;
            break;
        case O_WRONLY:
            flags = flags | 0222;
            break;
    }
    if(flags & cur_node->mode){
        fi->fh = &cur_node;//fh is a uint64_t that can be used to store data during open or release
        //fh gets called when reading and writing so very useful to store inode address here
        return 0;
    }
    return -1;
}



static struct fuse_operations operations = {
    .mkdir = tfs_mkdir,
    .mknod = tfs_mknod,
    // .getattr = tfs_getattr,
    .readdir = tfs_readdir,
    // .rmdir = tfs_rmdir,
    .open = tfs_open,
    .flush = tfs_flush, // close() operation stuff
    .read = tfs_read,
    .write = tfs_write,
    // .create = tfs_create,
    // .rename = tfs_rename
    .unlink = tfs_unlink

};

int main(int argc, char *argv[])
{
    memspace = malloc(MEMSIZE);
}

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
    bzero(block, BLOCKSIZE);
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
    uint8_t *block = NULL;
    uint64_t block_count = 0;
    uint64_t byte_count = 0;
    uint64_t total_blocks = dir->blocks;
    uint64_t total_bytes = dir->size;
    char name[MAX_NAME_LENGTH];
    while (block_count < total_blocks && byte_count < total_bytes)
    {
        block = fetch_block(dir, block_count);
        if (block == NULL)
        {
            block++;
            byte_count += BLOCKSIZE;
        }
        for (uint64_t i = 0; i < BLOCKSIZE && byte_count < total_bytes; i += PATH_BOUNDARY)
        {
            read_block(name, block, i, MAX_NAME_LENGTH);
            byte_count += PATH_BOUNDARY;
            if (name[0] == '\0')
                continue;
            filler(buffer, name, NULL, 0, 0);
        }
        block_count++;
    }
    return 0;
}

int tfs_open(const char *path, struct fuse_file_info *fi)
{
    node *cur_node = find_path_node(path);
    if (cur_node == NULL)
        return -1;
    uint32_t flags = 0;
    switch (fi->flags & O_ACCMODE)
    {
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
    if (flags & cur_node->mode)
    {
        fi->fh = &cur_node; // fh is a uint64_t that can be used to store data during open or release
        // fh gets called when reading and writing so very useful to store inode address here
        return 0;
    }
    return -1;
}

/// @brief Removes a file or a directory
/// @param pathname file path
/// @return 0 success, -1 failure
int tfs_unlink(const char *path)
{
    // first check if the pathname is valid
    node *cur_node = find_path_node(path);
    if (cur_node == NULL)
    {
        return -1;
    }

    // cannot remove root
    if (strcmp(path, "/") == 0)
    { // if root, return -1 (operation not permitted)
        return -1;
    }

    /////////////////////// start parent search
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

    // remove link from its parent
    int status = remove_link_from_parent(memspace, parent_node, cur_node);
    if (status == -1)
    {
        // something is wrong, as we cannot find child from parent
        return -1;
    }

    // Update links count
    cur_node->links -= 1;
    node * tmp = NULL;
    // if links count is 0, remove the file/directory
    if (cur_node->links == 0)
    {
        // if it is a directory, unlink everything in it before freeing block
        if ((cur_node->mode & S_IFMT) == S_IFDIR)
        {
            for (int i = 0; i < cur_node->blocks; i++)
            {
                uint8_t *block = get_i_block(cur_node, i);
                // unlink each entry (children dir or nod) in the block
                for (int j = 0; j < BLOCKSIZE && j+i*BLOCKSIZE < cur_node->size; j += NAME_BOUNDARY)
                {
                    // get the address of the inode?
                    // char temp[NAME_BOUNDARY - ADDR_LENGTH];
                    // read_block(temp, block, j, NAME_BOUNDARY - ADDR_LENGTH);
                    // tfs_unlink(temp);
                    read_block(&tmp, block, PATH_BOUNDARY - ADDR_LENGTH, ADDR_LENGTH);
                    sub_unlink(memspace, cur_node, tmp);//dont need to call tfs_unlink since we have the parent and child here. Saves time on traversal too
                }

                // put the whole block back to free list
                free_block(memspace, block);
            }
        }
        else
        {
            // free all blocks that belong to this inode
            for (int i = 0; i < cur_node->blocks; i++)
            {
                uint8_t *block = get_i_block(cur_node, i);
                free_block(memspace, block);
            }
        }

        // free the inode
        free_inode(cur_node);
    }
    return 0;
}

/// @brief read a file's data with given size and offset
/// @param path path of file
/// @param buff buffer to put data into
/// @param size amount of data requested to be read
/// @param offset offset to read from
/// @param fi fuse file info we wont use
/// @return amount of actually read data
int tfs_read(const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi)
{
    node *cur_node = find_path_node(path);
    if (cur_node == NULL)
        return -1;

    uint64_t total_bytes = cur_node->size;
    uint64_t total_blocks = cur_node->blocks;
    uint64_t bytes = 0;
    if (offset >= total_bytes)
        return -1;
    uint64_t blocks = offset / BLOCKSIZE;
    uint64_t loc = offset % BLOCKSIZE;
    uint64_t byte_counter = offset + size > total_bytes ? total_bytes - offset : size;
    uint8_t *block = NULL;
    uint64_t tmp = 0;
    while (byte_counter != 0 && blocks < total_blocks)
    { // strong condition with total blocks in case something is wrong with read block
        block = fetch_block(cur_node, blocks);
        tmp = read_block(buff + bytes, block, loc, byte_counter);
        bytes += tmp;
        byte_counter -= tmp;
        loc = 0;
        blocks++;
    }
    return bytes;
}

/// @brief write file data with given size and offset
/// @param path path of file
/// @param buff buffer that has write data
/// @param size amount of data requested to be written
/// @param offset offset to read from
/// @param fi fuse file info we wont use
/// @return amount of actually written data
int tfs_write(const char *path, const char *buff, size_t size, off_t offset, struct fuse_file_info *fi)
{
    node *cur_node = find_path_node(path);
    if (cur_node == NULL)
        return -1;

    uint64_t total_bytes = cur_node->size;
    uint64_t total_blocks = cur_node->blocks;
    uint64_t bytes = 0;
    uint64_t blocks = offset / BLOCKSIZE;
    uint64_t loc = offset % BLOCKSIZE;
    uint64_t byte_counter = size;
    uint8_t *block = NULL;
    uint64_t tmp = 0;
    uint8_t skip_fetch = 0;
    while (byte_counter != 0 && blocks < total_blocks)
    {
        if (skip_fetch == 0) // use skip fetch to avoid fetch when allocating more blocks for performance
            block = fetch_block(cur_node, blocks);
        else
            skip_fetch == 0;
        tmp = write_block(buff + bytes, block, loc, byte_counter); // see how many bytes we were able to write
        if (tmp == 0 || blocks == total_blocks - 1)
        {                                                  // full block or bad offset for tmp == 0; otherwise need to allocate a new block
            block = add_block_to_node(memspace, cur_node); // automatically increases cur_node->blocks by 1
            if (block == NULL)
            {
                cur_node->size = total_bytes > (uint64_t)offset + bytes ? total_bytes : (uint64_t)offset + bytes;
                return bytes;
            }
            skip_fetch = 1; // skip fetch in next cycle so we use new block
            total_blocks++;
        }
        bytes += tmp;        // increase bytes written
        byte_counter -= tmp; // decrease bytes left to write
        loc = 0;             // loc = 0 virtually every time except the first time of the loop where offset can start not at block beginning
        blocks++;            // increment to next block we plan to fetch
    }
    cur_node->size = total_bytes > (uint64_t)offset + bytes ? total_bytes : (uint64_t)offset + bytes;
    return bytes;
}

int tfs_flush(const char *path, struct fuse_file_info *fi){
    return 0;//we already write back data on write so we are good to leave flush alone. Flush should be used if we want to log our data or send it somewhere else too
}

static struct fuse_operations operations = {
    .mkdir = tfs_mkdir,
    .mknod = tfs_mknod,
    // .getattr = tfs_getattr,
    .readdir = tfs_readdir,
    // .rmdir = tfs_rmdir,
    .open = tfs_open,
    .flush = tfs_flush, // close() operation stuff. Pretty much finish writing data if you havent yet and have it stored somewhere
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

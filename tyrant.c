#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
// #include "include/fuse.h"
#include <fuse.h>
#include "tyrant.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <ctype.h>

uint8_t *memspace = NULL;
/*
@brief make a directory path by allocating an inode and adding it to the directory list
@param path pathname of new directory
@param m Permissions passed by system, 32-bit
@note TODO: Find parent path to determine where to add this file
*/
int tfs_mkdir(const char *path, mode_t m)
{
	//printf("Starting mkdir\n");
    //printf("CALLING MKDIR with path %s\n\n", path);
    node parent_node;
    uint64_t parent = 0;
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
    //if(isalnum(*(temp + i + 1)) == 0)
//	    return -EBADF;
    if (i != 0) // '/' or root is the first byte
        parent = find_path_node(temp);
    else
        parent = root_node;
    if (parent == 0)
    {
        free(temp);
        return -ENOENT;
    }
    fetch_inode(parent, &parent_node);
    ///////////////////////////////////////PARENT SEARCH END

    uint64_t dir = allocate_inode(drive); // get inode
    node dir_node;
    if (dir == 0)
    {
        free(temp);
        return -1;
    }
    fetch_inode(dir, &dir_node);

    uint64_t block = allocate_block(drive); // get block
    if (block == 0)
    {
        free_inode(dir); // free node if we cant make a full directory
        free(temp);
        return -1;
    }
    uint8_t block_data[BLOCKSIZE];
    bzero(block_data, BLOCKSIZE);
    write_block(block_data, block, 0, BLOCKSIZE); // clear directory block
    // https://jameshfisher.com/2017/02/24/what-is-mode_t/
    // what mode_t means
    dir_node.creation_time = get_current_time_in_nsec();
    dir_node.access_time = dir_node.creation_time;
    dir_node.change_time = dir_node.creation_time;
    dir_node.data_time = dir_node.creation_time;
    dir_node.user_id = geteuid();
    dir_node.group_id = getegid();
    dir_node.mode = m; // definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    dir_node.mode |= S_IFDIR;
    dir_node.direct_blocks[0] = block;
    dir_node.blocks = 1;
    dir_node.links = 0;
    write_block(".", block, 0, sizeof("."));
    write_block(&dir, block, MAX_NAME_LENGTH, ADDR_LENGTH);

    write_block("..", block, PATH_BOUNDARY, sizeof(".."));
    write_block(&parent, block, 2 * PATH_BOUNDARY - BLOCKSIZE, ADDR_LENGTH); // write address of parent node
    dir_node.size = PATH_BOUNDARY * 2;
    commit_inode(&dir_node, dir);
    int result = add_to_directory(parent, dir, (temp + i + 1));
    if (result != 0)
    {
        free_block(drive, block); // need memspace since we need to write on disk/working memory too
        free_inode(dir);
        free(temp);
        return -1;
    }
    free(temp);
    //printf("FINISHED MKDIR\n");
    return result;
}

int tfs_mknod(const char *path, mode_t m, dev_t d)
{
	//printf("Startind mknod\n");
    uint64_t parent = 0;
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
  //  if(isalnum(*(temp + i +1)) == 0)
//	    return -EBADF;
    if (i != 0) // '/' or root is the first byte
        parent = find_path_node(temp);
    else
        parent = root_node;
    if (parent == 0)
    {
        free(temp);
        return -ENOENT;
    }

    ///////////////////////////////////////PARENT SEARCH END

    uint64_t data = allocate_inode(drive); // get inode
    if (data == 0)
    {
        free(temp);
        return -1;
    }
    node data_node;
    fetch_inode(data, &data_node);
    // https://jameshfisher.com/2017/02/24/what-is-mode_t/
    // what mode_t means
    // dev_t is device ID
    data_node.mode = m; // definitions for mode can be found in sys/stat.h and bits/stat.h (bits for actual numerical value)
    //printf("\n\nStarting data node with mode %x\n\n", data_node.mode);
    data_node.links = 0;
    data_node.blocks = 0;
    data_node.size = 0;
    data_node.user_id = geteuid();
    data_node.group_id = getegid();

    data_node.creation_time = get_current_time_in_nsec();
    data_node.access_time = data_node.creation_time;
    data_node.change_time = data_node.creation_time;
    data_node.data_time = data_node.creation_time;
    commit_inode(&data_node, data);
    //printf("Adding %s as part of path %s\n", temp+i+1, (char*)path);
    int result = add_to_directory(parent, data, (temp + i + 1));
    if (result != 0)
    {
        free_inode(data);
        free(temp);
        return result;
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
int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	//printf("Starting readdir\n");

    uint64_t dir_loc = find_path_node((char *)path);
    if (dir_loc == 0)
        return -ENOENT;
    node dir;
    fetch_inode(dir_loc, &dir);
    uint64_t block = 0;
    uint64_t block_count = 0;
    uint64_t byte_count = 0;
    uint64_t total_blocks = dir.blocks;
    uint64_t total_bytes = dir.size;
    dir.access_time = get_current_time_in_nsec();
    commit_inode(&dir, dir_loc); // commit access time
    char name[MAX_NAME_LENGTH];
    while (block_count < total_blocks && byte_count < total_bytes)
    {
        block = get_i_block(&dir, block_count);
        if (block == 0)
        {
            block_count++;
            byte_count += BLOCKSIZE;
            continue;
        }
        for (uint64_t i = 0; i < BLOCKSIZE && byte_count < total_bytes; i += PATH_BOUNDARY)
        {
            read_block(name, block, i, MAX_NAME_LENGTH);
            //printf("%s\n", name);
            byte_count += PATH_BOUNDARY;
            if (name[0] == '\0'){
                //printf("empty name\n");
                continue;
                }
            filler(buffer, name, NULL, 0);
            
        }
        block_count++;
    }
    return 0;
}

int tfs_open(const char *path, struct fuse_file_info *fi)
{
	//printf("Starting open\n");
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)
        return -ENOENT;
    node cur_node;
    fetch_inode(cur, &cur_node);
    cur_node.access_time = get_current_time_in_nsec();
    commit_inode(&cur_node, cur); // commit access time
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
    if (flags & cur_node.mode)
    {
	//printf("Permissions of open: %x\n", flags & cur_node.mode);
        // fi->fh = &cur_node; // fh is a uint64_t that can be used to store data during open or release
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
	//printf("starting unlink\n");
    // first check if the pathname is valid
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)
    {
        return -ENOENT;
    }
    node cur_node;
    fetch_inode(cur, &cur_node);

    // cannot remove root
    if (strcmp(path, "/") == 0)
    { // if root, return -1 (operation not permitted)
        return -1;
    }

    /////////////////////// start parent search
    uint64_t parent = 0;
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
        parent = find_path_node(temp);
    else
        parent = root_node;
    if (parent == 0)
    {
        free(temp);
        return -1;
    }
    node parent_node;

    ///////////////////////////////////////PARENT SEARCH END

    // remove link from its parent
    int status = remove_link_from_parent(parent, cur);
    if (status == -1)
    {
        // something is wrong, as we cannot find child from parent
        return -1;
    }
    fetch_inode(cur, &cur_node);
    fetch_inode(parent, &parent_node);
    // Update links count
    cur_node.links -= 1;
    cur_node.change_time = get_current_time_in_nsec();
    commit_inode(&cur_node, cur);
    uint64_t block = 0;
    uint64_t tmp = 0;
    uint64_t total_blocks = cur_node.blocks;
    // if links count is 0, remove the file/directory
    //printf("Current links: %d\n", cur_node.links);
    if (cur_node.links == 0)
    {
        // if it is a directory, unlink everything in it before freeing block
        if ((cur_node.mode & S_IFDIR) == S_IFDIR)
        {
            for (uint64_t i = total_blocks; i > 0; i--)
            {
                block = get_i_block(&cur_node, i-1);
                // unlink each entry (children dir or nod) in the block
                for (int j = (i == total_blocks) ? cur_node.size % BLOCKSIZE : BLOCKSIZE; j > 0; j -= NAME_BOUNDARY)
                {
                    // get the address of the inode?
                    // char temp[NAME_BOUNDARY - ADDR_LENGTH];
                    if((i-1)*BLOCKSIZE+j-NAME_BOUNDARY == NAME_BOUNDARY || (i-1)*BLOCKSIZE+j-NAME_BOUNDARY == 0)
                        continue;//no need to unlink . or ..
                    read_block(&tmp, block, j - ADDR_LENGTH, ADDR_LENGTH);
                    // read_block(temp, block, j, NAME_BOUNDARY - ADDR_LENGTH);

                    sub_unlink(cur, tmp);
                }

                // put the whole block back to free list
                free_block(drive, block-1);
            }
        }
        else
        {
            // free all blocks that belong to this inode
            for (int i = 0; i < cur_node.blocks; i++)
            {
                uint64_t block = get_i_block(&cur_node, i);
                free_block(drive, block);
            }
        }

        // free the inode
	//printf("Freeing child inode");
        free_inode(cur);
    }
    else
    {
        commit_inode(&cur_node, cur);
    }
    //printf("Done unlinking\n");
    return 0;
}

int tfs_rmdir(const char * path){
    return tfs_unlink(path);
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
	//printf("starting read\n");
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)//
        return -ENOENT;
    node cur_node;
    fetch_inode(cur, &cur_node);
    //printf("READING\n\n");
    uint64_t total_bytes = cur_node.size;
    uint64_t total_blocks = cur_node.blocks;
    uint64_t bytes = 0;
    if (offset >= total_bytes)
        return -1;
    uint64_t blocks = offset / BLOCKSIZE;
    uint64_t loc = offset % BLOCKSIZE;
    uint64_t byte_counter = offset + size > total_bytes ? total_bytes - offset : size;
    uint64_t block = 0;
    uint64_t tmp = 0;
    cur_node.access_time = get_current_time_in_nsec();
    commit_inode(&cur_node, cur);
    while (byte_counter != 0 && blocks < total_blocks && tmp >= 0)
    { // strong condition with total blocks in case something is wrong with read block
        block = get_i_block(&cur_node, blocks);
        tmp = read_block(buff + bytes, block, loc, byte_counter);
        bytes += tmp;
        byte_counter -= tmp;
        loc = 0;
        blocks++;
    }
    //printf("READ %lu BYTES\n\n", bytes);
    return bytes;
}

/// @brief write file data with given size and offset
/// @param path path of file
/// @param buff buffer that has write data
/// @param size amount of data requested to be written
/// @param offset offset to write from
/// @param fi fuse file info we wont use
/// @return amount of actually written data
int tfs_write(const char *path, const char *buff, size_t size, off_t offset, struct fuse_file_info *fi)
{
	//printf("Starting write\n");
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)
        return -ENOENT;
    node cur_node;
    fetch_inode(cur, &cur_node);
    //printf("WRITING. CURRENTLY HAS %lu blocks. REQUESTING %lu BYTES\n\n", cur_node.blocks, size);
    uint64_t total_bytes = cur_node.size;
    uint64_t total_blocks = cur_node.blocks;
    uint64_t bytes = 0;
    uint64_t blocks = offset / BLOCKSIZE;
    uint64_t loc = offset % BLOCKSIZE;
    uint64_t byte_counter = size;
    uint64_t block = 0;
    uint64_t tmp = 0;
    cur_node.data_time = get_current_time_in_nsec();
    cur_node.change_time = cur_node.data_time;
    commit_inode(&cur_node, cur); // commit timestamps
    if (size + offset > total_bytes)
    { // add new blocks if we dont have enough for write
        uint64_t final_blocks = (size + offset) / BLOCKSIZE + (((size + offset) % BLOCKSIZE) == 0 ? 0 : 1);
        for (uint64_t i = total_blocks; i < final_blocks; i++)
        {
            block = add_block_to_node(&cur_node, cur);
            if (block == 0)
                break;
        }
    }
    total_blocks = cur_node.blocks;

    while (byte_counter != 0 && blocks < total_blocks && tmp >= 0)
    {
        block = get_i_block(&cur_node, blocks);
        tmp = write_block((uint8_t *)(buff + bytes), block, loc, byte_counter); // see how many bytes we were able to write
        bytes += tmp;                                                           // increase bytes written
        //printf("BYTES LEFT %lu\n\n", bytes);
        byte_counter -= tmp; // decrease bytes left to write
        loc = 0;             // loc = 0 virtually every time except the first time of the loop where offset can start not at block beginning
        blocks++;            // increment to next block we plan to fetch
    }
    cur_node.size = total_bytes > (uint64_t)offset + bytes ? total_bytes : (uint64_t)offset + bytes;
    commit_inode(&cur_node, cur);
    //printf("FINISHED WRITING %lu BYTES\n\n", bytes);
    return bytes;
}

int tfs_truncate(const char *path, off_t length)
{
	//printf("Starting truncate\n");
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)
        return -ENOENT;
    node cur_node;
    fetch_inode(cur, &cur_node);
    if (cur_node.size <= length)
        return 0;
    uint64_t block_no = length / BLOCKSIZE;
    uint64_t block = get_i_block(&cur_node, block_no);
    uint64_t total_blocks = cur_node.blocks;
    for (uint32_t i = block_no+1; i < total_blocks; i++)
    {
        block = get_i_block(&cur_node, i);
        free_block(drive, block);
	cur_node.blocks--;
    }
    cur_node.change_time = get_current_time_in_nsec();
    cur_node.data_time = cur_node.change_time;
    cur_node.size = length;
    commit_inode(&cur_node, cur);
    return 0;
}

int tfs_getattr(const char * path, struct stat * st){
    //printf("getting attribute\n");
    uint64_t cur = find_path_node((char *)path);
    //printf("Cur node location is %ld\n", cur);
    if(cur == 0){
        //printf("%s NOT FOUND\n\n", path);
        return -ENOENT;
    }
    node cur_node;
   // memset(&cur_node, 0, sizeof(node));
    fetch_inode(cur, &cur_node);
    memset(st, 0, sizeof(struct stat));

    st->st_ino = (uint64_t)(cur - BLOCKSIZE)/INODE_SIZE_BOUNDARY;
    st->st_mode = cur_node.mode;
    ////printf("cur node is DIR? %x %x\n", (cur_node.mode), S_IFDIR);
    ////printf("IS DIR? %x\n", (st->st_mode & S_IFDIR)); 
    st->st_nlink = cur_node.links;
    st->st_size = cur_node.size;
    st->st_blocks = cur_node.blocks;

    st->st_uid = cur_node.user_id;
    st->st_gid = cur_node.group_id;
    st->st_blksize = BLOCKSIZE;
    st->st_atime = (time_t)cur_node.access_time;
    st->st_mtime = (time_t)cur_node.data_time;
    st->st_ctime = (time_t)cur_node.change_time;
    //printf("Returning stat for %s\n", (char*)path);
    return 0;
}

int tfs_utime(const char *path, struct utimbuf *tv){
    uint64_t cur = find_path_node((char *)path);
    if(cur == 0){
        //printf("%s NOT FOUND\n\n", path);
        return -ENOENT;
    }

    node cur_node;
    fetch_inode(cur, &cur_node);
    cur_node.access_time = tv->actime;
    cur_node.change_time = tv->modtime;
    commit_inode(&cur_node, cur);
    return 0;   
}

int tfs_flush(const char *path, struct fuse_file_info *fi)
{
   // return syncfs(drive);
    return 0; // we already write back data on write so we are good to leave flush alone. Flush should be used if we want to log our data or send it somewhere else too
}

int tfs_chmod(const char * path, mode_t m){
    uint64_t cur = find_path_node((char *)path);
    if(cur == 0){
        //printf("%s NOT FOUND\n\n", path);
        return -ENOENT;
    }

    node cur_node;
    fetch_inode(cur, &cur_node);
    cur_node.mode = m;
    commit_inode(&cur_node, cur);
    return 0;
}


int tfs_chown(const char * path, uid_t owner, gid_t group){
    uint64_t cur = find_path_node((char *)path);
    if(cur == 0){
        //printf("%s NOT FOUND\n\n", path);
        return -ENOENT;
    }

    node cur_node;
    fetch_inode(cur, &cur_node);
    cur_node.user_id = owner;
    cur_node.group_id = group;
    commit_inode(&cur_node, cur);
    return 0;
}	

int tfs_rename(const char * path, const char * new_name){
    uint64_t cur = find_path_node((char *)path);
    if (cur == 0)
    {
        return -ENOENT;
    }
    node cur_node;
    fetch_inode(cur, &cur_node);

    // cannot remove root
    if (strcmp(path, "/") == 0)
    { // if root, return -1 (operation not permitted)
        return -1;
    }

    /////////////////////// start parent search
    uint64_t parent = 0;
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
        parent = find_path_node(temp);
    else
        parent = root_node;
    if (parent == 0)
    {
        free(temp);
        return -1;
    }
    free(temp);

    char *temp2 = malloc(sizeof(char) * (strlen(new_name) + 1));
    strcpy(temp2, new_name);
    for (i = strlen(new_name) - 1; temp2[i] != '/' && i >= 0; i--)
        ; // check for last / to differentiate parent path from new directory
    if (i == -1)
    {
        free(temp2);
        return -1; // invalid path
    }

    temp2[i] = 0; // terminate at / for full path




    return rename_from_parent(parent, cur, (char *) new_name+i+1);



}

static const struct fuse_operations operations = {
    .mkdir = &tfs_mkdir,
    .mknod = &tfs_mknod,
    .getattr = &tfs_getattr,
    .readdir = &tfs_readdir,
    .rmdir = &tfs_rmdir,
    .utime = &tfs_utime,
    // .rmdir = tfs_rmdir,
    .open = &tfs_open,
    .flush = &tfs_flush, // close() operation stuff. Pretty much finish writing data if you havent yet and have it stored somewhere
    .read = &tfs_read,
    .write = &tfs_write,
    .truncate = &tfs_truncate,
    .chmod = &tfs_chmod,
    .chown = &tfs_chown,
    .rename = &tfs_rename,
    // .create = tfs_create,
    // .rename = tfs_rename
    .unlink = &tfs_unlink

};

int main(int argc, char **argv)
{
    if (argc < 3){
	//printf("Not enough args. Add a directory path\n");
        return -1;
    }
    char * path = argv[argc-1];
    int fd = open(path, O_RDWR, 0666);
    if (fd < 0){
        //printf("\n\nFailed to open %s\n\n", path);
        return fd;
        }
    tfs_disk_info(fd);
    //printf("Starting up FUSE in %s\n", path);
    return fuse_main(argc -1, argv, &operations);
}

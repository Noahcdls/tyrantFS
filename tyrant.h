#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "tyrant_help.h"
#define MAX_NAME_LENGTH 256-8 // maximum name length of a file or directory
#define PATH_BOUNDARY 256   // name + address of inode

int tfs_mknod(const char *path, mode_t m, dev_t d);
int tfs_mkdir(const char *path, mode_t m);
int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info *fi);
int tfs_open(const char *path, struct fuse_file_info *fi);
int tfs_flush(const char *path, struct fuse_file_info *fi);
int tfs_read(const char *path, char * buff, size_t size, off_t offset,
             struct fuse_file_info *fi);
int tfs_write(const char *path, const char * buff, size_t size, off_t offset,
              struct fuse_file_info *fi);
int tfs_truncate(const char * path, off_t length);
int tfs_unlink(const char *path);
int tfs_rmdir(const char * path);
int tfs_utime(const char *path, struct utimbuf *tv);
int tfs_chmod(const char * path, mode_t m);
int tfs_chown(const char * path, uid_t owner, gid_t group);
int tfs_rename(const char * path, const char * new_name);
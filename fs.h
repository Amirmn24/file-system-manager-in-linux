#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>
#include <time.h> // For stress test timing

#define MAGIC 0xDEADBEEF
#define MAX_FILENAME 32
#define MAX_USERNAME 32
#define MAX_GROUPNAME 32
#define MAX_USER_GROUPS 8 

// New Configurations based on assignment
#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 32768
#define DISK_SIZE (TOTAL_BLOCKS * BLOCK_SIZE) // ~128 MB

// Permission Macros
#define R_OK 4
#define W_OK 2
#define X_OK 1

// User Structure
typedef struct {
    int32_t uid;
    char username[MAX_USERNAME];
    int32_t gids[MAX_USER_GROUPS];
    int32_t next; 
} User;

// Group Structure
typedef struct {
    int32_t gid;
    char groupname[MAX_GROUPNAME];
    int32_t next;
} Group;

// SuperBlock
typedef struct {
    int32_t magic;
    int32_t version;
    // first_free_block REMOVED (Replaced by Bitmap in Block 1)
    int32_t file_count;
    int32_t first_file;

    int32_t first_user;
    int32_t first_group;
    int32_t next_uid;
    int32_t next_gid;
} SuperBlock;

// FileEntry
typedef struct {
    char name[MAX_FILENAME];
    int32_t size;
    int32_t permission;
    int32_t uid;
    int32_t gid;
    int32_t data_block;
    int32_t next;
} FileEntry;

// --- FUNCTION DECLARATIONS ---

void fs_open_disk();
void fs_save_superblock();

// Core File Operations
int32_t fs_find_file(const char *filename);
int fs_open(const char *name, int flags);
int fs_read(int pos, int n_bytes, char *buffer);
int fs_write(int pos, int n_bytes, const char *buffer);
void fs_rm(const char *name);
void fs_shrink(int new_size);

// User & Group Management
void fs_useradd(const char *username);
void fs_userdel(const char *username);
void fs_groupadd(const char *groupname);
void fs_groupdel(const char *groupname);
void fs_usermod(const char *username, const char *groupname);
void fs_login(const char *username);
int32_t fs_get_current_uid();
void fs_print_users();

// Permission Management
void fs_chmod(const char *path, int mode);
void fs_chown(const char *path, const char *owner_user, const char *owner_group);
void fs_chgrp(const char *path, const char *groupname);
void fs_getfacl(const char *path);

// System
void fs_close();
void fs_stats();
void fs_stress_test(); // New function

#endif

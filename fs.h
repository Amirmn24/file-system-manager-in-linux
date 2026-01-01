#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>

#define MAGIC 0xDEADBEEF
#define MAX_FILENAME 32
#define MAX_USERNAME 32
#define MAX_GROUPNAME 32
#define MAX_USER_GROUPS 8 

// --- UPDATED FOR BITMAP FS ---
#define BLOCK_SIZE 4096         // 4KB Block
#define TOTAL_BLOCKS 32768      // 32K Blocks
#define DISK_SIZE (TOTAL_BLOCKS * BLOCK_SIZE) // 128 MB

// Reserved Blocks
#define SUPERBLOCK_IDX 0
#define BITMAP_BLOCK_IDX 1
#define ROOT_DIR_BLOCK_IDX 2 // Where data starts allocation

// Permission Macros
#define R_OK 4
#define W_OK 2
#define X_OK 1 

// User Structure
typedef struct {
    int32_t uid;
    char username[MAX_USERNAME];
    int32_t gids[MAX_USER_GROUPS]; 
    int32_t next; // Offset to next user block
} User;

// Group Structure
typedef struct {
    int32_t gid;
    char groupname[MAX_GROUPNAME];
    int32_t next; // Offset to next group block
} Group;

// Modified SuperBlock
typedef struct {
    int32_t magic;
    int32_t version;
    // first_free_block REMOVED - Using Bitmap now
    int32_t file_count;
    int32_t first_file;

    // Metadata Headers
    int32_t first_user;
    int32_t first_group;
    int32_t next_uid; 
    int32_t next_gid; 
    
    // Padding to ensure SB is smaller than BLOCK_SIZE (not strictly necessary but good practice)
    char padding[BLOCK_SIZE - 64]; 
} SuperBlock;

// Modified FileEntry
typedef struct {
    char name[MAX_FILENAME];
    int32_t size;
    int32_t permission; 
    int32_t uid;        
    int32_t gid;        
    int32_t data_block; // Byte offset to data
    int32_t next;
} FileEntry;

// --- FUNCTION DECLARATIONS ---

void fs_open_disk();
void fs_save_metadata(); // Saves SB and Bitmap

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

// Permission Management
void fs_chmod(const char *path, int mode);
void fs_chown(const char *path, const char *owner_user, const char *owner_group);
void fs_chgrp(const char *path, const char *groupname);
void fs_getfacl(const char *path);

// System
void fs_close();
void fs_stats();
void fs_visualize_bitmap(); // Changed from free list
void fs_stress_test(); // NEW: For step 1

#endif

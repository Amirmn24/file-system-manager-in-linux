#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>

#define MAGIC 0xDEADBEEF
#define MAX_FILENAME 32
#define MAX_USERNAME 32
#define MAX_GROUPNAME 32
#define MAX_USER_GROUPS 8 // Max groups a user can belong to
#define BLOCK_SIZE 4096
#define DISK_SIZE (1024*1024) // 1 MB

// Permission Macros
#define R_OK 4
#define W_OK 2
#define X_OK 1 // Not used for files in this simple FS, but kept for standard

// --- NEW STRUCTURES ---

typedef struct {
    int32_t start;
    int32_t size;
    int32_t next;  
} FreeBlock;

// User Structure
typedef struct {
    int32_t uid;
    char username[MAX_USERNAME];
    int32_t gids[MAX_USER_GROUPS]; // List of Group IDs this user belongs to
    int32_t next; // Offset to next user block
} User;

// Group Structure
typedef struct {
    int32_t gid;
    char groupname[MAX_GROUPNAME];
    int32_t next; // Offset to next group block
} Group;

//SuperBlock
typedef struct {
    int32_t magic;
    int32_t version;
    int32_t first_free_block;
    int32_t file_count;
    int32_t first_file;
    
    // New Metadata Headers
    int32_t first_user;
    int32_t first_group;
    int32_t next_uid; // Auto-increment counter
    int32_t next_gid; // Auto-increment counter
} SuperBlock;

//FileEntry
typedef struct {
    char name[MAX_FILENAME];
    int32_t size;
    int32_t permission; // Octal representation e.g., 0755 stored as int (decimal 493)
    int32_t uid;        // Owner UID
    int32_t gid;        // Group GID
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
void fs_usermod(const char *username, const char *groupname); // Add user to group
void fs_login(const char *username); // Switch current user context
int32_t fs_get_current_uid();
void fs_print_users(); // Helper to see users

// Permission Management
void fs_chmod(const char *path, int mode);
void fs_chown(const char *path, const char *owner_user, const char *owner_group);
void fs_chgrp(const char *path, const char *groupname);
void fs_getfacl(const char *path);

// System
void fs_close();
void fs_stats();
void fs_visualize_free_list();

#endif

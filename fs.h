#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdio.h>

#define MAGIC 0xDEADBEEF
#define MAX_FILENAME 32
#define BLOCK_SIZE 4096
#define DISK_SIZE (1024*1024) // 1 MB

//new struct
typedef struct {
    int32_t start; 
    int32_t size;  
    int32_t next;  // offset of next block
} FreeBlock;

//superBlock structure(modified)
typedef struct {
    int32_t magic;
    int32_t version;
    int32_t first_free_block; 
    int32_t file_count;
    int32_t first_file; 
} SuperBlock;

// fileEntry structure
typedef struct {
    char name[MAX_FILENAME];
    int32_t size;
    int32_t type;
    int32_t permission;
    int32_t data_block; 
    int32_t next;      
} FileEntry;

// function declarations
void fs_open_disk();
void fs_save_superblock();

int32_t fs_find_file(const char *filename);

int fs_open(const char *name, int flags);
int fs_read(int pos, int n_bytes, char *buffer);
int fs_write(int pos, int n_bytes, const char *buffer);

void fs_shrink(int new_size);
void fs_rm(const char *name);
void fs_close();
void fs_stats();
void fs_visualize_free_list(); 

#endif

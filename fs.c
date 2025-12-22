#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // Ensure stdio is included

FILE *disk = NULL;
SuperBlock sb;

FileEntry current_file;
int32_t current_file_pos = -1;

// --- FORWARD DECLARATIONS for alloc/free ---
int32_t alloc_block(int32_t size);
void free_block(int32_t start, int32_t size);

// Load or initialize filesystem
void fs_open_disk() {
    disk = fopen("filesys.db", "r+b");
    if (!disk) {
        printf("Cannot open disk file.\n");
        exit(1);
    }

    fread(&sb, sizeof(SuperBlock), 1, disk);

    if (sb.magic != MAGIC) {
        printf("Initializing new filesystem...\n");

        sb.magic = MAGIC;
        sb.version = 1;
        sb.file_count = 0;
        sb.first_file = -1;
        
        // Initialize free list
        sb.first_free_block = sizeof(SuperBlock);

        fseek(disk, 0, SEEK_SET);
        fwrite(&sb, sizeof(SuperBlock), 1, disk);

        // Create the first big free block
        FreeBlock initial_block;
        initial_block.size = DISK_SIZE - sizeof(SuperBlock);
        initial_block.next = -1;
        
        fseek(disk, sb.first_free_block, SEEK_SET);
        fwrite(&initial_block, sizeof(FreeBlock), 1, disk);

        fflush(disk);
    }
}

void fs_save_superblock() {
    fseek(disk, 0, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, disk);
    fflush(disk);
}

// ---- NEW MEMORY MANAGEMENT FUNCTIONS ----

// Allocate a block of memory of at least `size` bytes.
// This function now correctly implements block splitting.
int32_t alloc_block(int32_t size) {
    int32_t current_pos = sb.first_free_block;
    int32_t prev_pos = -1;

    while (current_pos != -1) {
        FreeBlock fb;
        fseek(disk, current_pos, SEEK_SET);
        fread(&fb, sizeof(FreeBlock), 1, disk);

        if (fb.size >= size) {
            // We found a block that is large enough.
            int32_t remaining_size = fb.size - size;
            
            // Check if the remaining space is too small to be useful.
            // A useful block must be able to at least hold a FreeBlock header.
            if (remaining_size < sizeof(FreeBlock)) {
                // Allocate the entire block.
                // Remove it from the free list.
                if (prev_pos == -1) { // It's the first block in the list
                    sb.first_free_block = fb.next;
                } else {
                    FreeBlock prev_fb;
                    fseek(disk, prev_pos, SEEK_SET);
                    fread(&prev_fb, sizeof(FreeBlock), 1, disk);
                    prev_fb.next = fb.next;
                    fseek(disk, prev_pos, SEEK_SET);
                    fwrite(&prev_fb, sizeof(FreeBlock), 1, disk);
                }
                fs_save_superblock();
                return current_pos;
            } else {
                // ** THIS IS THE CRITICAL BLOCK SPLITTING LOGIC **
                // The block is large enough to be split.
                
                // 1. Create a new header for the remaining free space.
                int32_t new_header_pos = current_pos + size;
                FreeBlock new_fb;
                new_fb.size = remaining_size;
                new_fb.next = fb.next;

                // 2. Write the new header to disk.
                fseek(disk, new_header_pos, SEEK_SET);
                fwrite(&new_fb, sizeof(FreeBlock), 1, disk);

                // 3. Update the free list to point to the new, smaller block.
                if (prev_pos == -1) { // It's the first block
                    sb.first_free_block = new_header_pos;
                } else {
                    FreeBlock prev_fb;
                    fseek(disk, prev_pos, SEEK_SET);
                    fread(&prev_fb, sizeof(FreeBlock), 1, disk);
                    prev_fb.next = new_header_pos;
                    fseek(disk, prev_pos, SEEK_SET);
                    fwrite(&prev_fb, sizeof(FreeBlock), 1, disk);
                }
                fs_save_superblock();
                
                // 4. Return the address of the allocated chunk.
                return current_pos;
            }
        }
        prev_pos = current_pos;
        current_pos = fb.next;
    }
    return -1; // No suitable block found
}


// Free a block of memory. This function should handle coalescing.
void free_block(int32_t start, int32_t size) {
    // For now, we just add it to the front of the list. Coalescing is a future step.
    FreeBlock new_fb;
    new_fb.size = size;
    new_fb.next = sb.first_free_block;

    fseek(disk, start, SEEK_SET);
    fwrite(&new_fb, sizeof(FreeBlock), 1, disk);

    sb.first_free_block = start;
    fs_save_superblock();
    // NOTE: A full implementation would search the list and merge adjacent blocks.
}


// ---- MODIFIED FILESYSTEM FUNCTIONS ----

int fs_open(const char *name, int flags) {
    int32_t pos = fs_find_file(name);

    if (pos != -1) { // File exists
        fseek(disk, pos, SEEK_SET);
        fread(&current_file, sizeof(FileEntry), 1, disk);
        current_file_pos = pos;
        return 0;
    }

    if (!(flags & 1)) { // File does not exist, and O_CREAT is not set
        printf("File not found and create flag not set.\n");
        return -1;
    }

    // File does not exist, create it.
    int32_t fe_pos = alloc_block(sizeof(FileEntry));
    if (fe_pos == -1) {
        printf("Error: No space left on disk for file entry.\n");
        return -1;
    }

    FileEntry fe;
    memset(&fe, 0, sizeof(fe));
    strncpy(fe.name, name, MAX_FILENAME - 1);
    fe.size = 0;
    fe.type = 0;
    fe.permission = 0;
    fe.data_block = -1; // No data block allocated yet
    fe.next = sb.first_file;

    fseek(disk, fe_pos, SEEK_SET);
    fwrite(&fe, sizeof(FileEntry), 1, disk);

    sb.first_file = fe_pos;
    sb.file_count++;
    fs_save_superblock();

    current_file = fe;
    current_file_pos = fe_pos;

    return 0;
}

int fs_write(int pos, int n_bytes, const char *buffer) {
    if (current_file_pos == -1) {
        printf("No file is open.\n");
        return -1;
    }

    if (current_file.data_block == -1) {
        // Allocate data block on first write
        current_file.data_block = alloc_block(BLOCK_SIZE);
        if (current_file.data_block == -1) {
            printf("Error: No space left for data block.\n");
            return -1;
        }
    }
    
    // Check boundaries
    if (pos + n_bytes > BLOCK_SIZE) {
        n_bytes = BLOCK_SIZE - pos;
        if (n_bytes <= 0) {
            printf("Write position out of block boundary.\n");
            return 0;
        }
        printf("Warning: Write truncated to fit in a single block.\n");
    }

    fseek(disk, current_file.data_block + pos, SEEK_SET);
    fwrite(buffer, 1, n_bytes, disk);

    if (pos + n_bytes > current_file.size) {
        current_file.size = pos + n_bytes;
    }

    // Save updated FileEntry
    fseek(disk, current_file_pos, SEEK_SET);
    fwrite(&current_file, sizeof(FileEntry), 1, disk);

    return n_bytes;
}

void fs_rm(const char *name) {
    int32_t prev_fe_pos = -1;
    int32_t current_fe_pos = sb.first_file;

    while (current_fe_pos != -1) {
        FileEntry fe;
        fseek(disk, current_fe_pos, SEEK_SET);
        fread(&fe, sizeof(FileEntry), 1, disk);

        if (strcmp(fe.name, name) == 0) {
            // Found the file, now remove it.
            // 1. Unlink from the file list.
            if (prev_fe_pos == -1) {
                sb.first_file = fe.next;
            } else {
                FileEntry prev_fe;
                fseek(disk, prev_fe_pos, SEEK_SET);
                fread(&prev_fe, sizeof(FileEntry), 1, disk);
                prev_fe.next = fe.next;
                fseek(disk, prev_fe_pos, SEEK_SET);
                fwrite(&prev_fe, sizeof(FileEntry), 1, disk);
            }

            // 2. Free the data block if it exists.
            if (fe.data_block != -1) {
                free_block(fe.data_block, BLOCK_SIZE);
            }

            // 3. Free the file entry block itself.
            free_block(current_fe_pos, sizeof(FileEntry));

            sb.file_count--;
            fs_save_superblock();
            printf("File '%s' removed.\n", name);
            
            if (current_file_pos == current_fe_pos) {
                current_file_pos = -1; // Invalidate current file if it's the one being removed
            }
            return;
        }
        prev_fe_pos = current_fe_pos;
        current_fe_pos = fe.next;
    }
    printf("File '%s' not found.\n", name);
}


// --- OTHER FUNCTIONS (mostly unchanged) ---

// Find file in linked list
int32_t fs_find_file(const char *filename) {
    int32_t pos = sb.first_file;
    while (pos != -1) {
        FileEntry fe;
        fseek(disk, pos, SEEK_SET);
        fread(&fe, sizeof(FileEntry), 1, disk);
        if (strcmp(fe.name, filename) == 0) return pos;
        pos = fe.next;
    }
    return -1;
}

// Read file
int fs_read(int pos, int n_bytes, char *buffer) {
    if (current_file_pos == -1) {
        printf("No file is open.\n");
        return -1;
    }
    if (current_file.data_block == -1) return 0; // No data to read
    if (pos >= current_file.size) return 0; // Read past end of file

    int available = current_file.size - pos;
    if (n_bytes > available) n_bytes = available;

    fseek(disk, current_file.data_block + pos, SEEK_SET);
    fread(buffer, 1, n_bytes, disk);
    buffer[n_bytes] = '\0';
    return n_bytes;
}

void fs_shrink(int new_size) {
    if (current_file_pos == -1) {
        printf("No file is open.\n");
        return;
    }
    if (new_size < 0) new_size = 0;
    if (new_size >= current_file.size) return; // Cannot grow with shrink

    current_file.size = new_size;
    fseek(disk, current_file_pos, SEEK_SET);
    fwrite(&current_file, sizeof(FileEntry), 1, disk);
    printf("File size shrunk to %d bytes.\n", new_size);
}

void fs_close() {
    current_file_pos = -1;
}

void fs_stats() {
    int64_t total_free_space = 0;
    int32_t pos = sb.first_free_block;
    int block_count = 0;
    while (pos != -1) {
        FreeBlock fb;
        fseek(disk, pos, SEEK_SET);
        fread(&fb, sizeof(FreeBlock), 1, disk);
        total_free_space += fb.size;
        pos = fb.next;
        block_count++;
    }
    printf("--- Filesystem Stats ---\n");
    printf(" File count: %d\n", sb.file_count);
    printf(" Free space: %lld bytes\n", (long long)total_free_space);
    printf("  Used space: %lld bytes\n", (long long)DISK_SIZE - total_free_space);
    printf("  Free blocks: %d\n", block_count);
    printf("------------------------\n");
}

void fs_visualize_free_list() {
    printf("--- Free Space Visualizer ---\n");
    int32_t pos = sb.first_free_block;
    int i = 0;
    if (pos == -1) {
        printf("No free space available.\n");
    }
    while (pos != -1) {
        FreeBlock fb;
        fseek(disk, pos, SEEK_SET);
        fread(&fb, sizeof(FreeBlock), 1, disk);
        printf("Block %d: [Header at %d] -> Size = %lld bytes, Next Header = %d\n",
               i, pos, (long long)fb.size, fb.next);
        pos = fb.next;
        i++;
    }
    printf("---------------------------\n");
}

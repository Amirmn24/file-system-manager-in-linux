#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

FILE *disk = NULL;
SuperBlock sb;

FileEntry current_file;
int32_t current_file_pos = -1;

// Global Context for currently loggedIn user
int32_t current_uid = 0; 
int32_t current_gid = 0; 
int32_t current_user_groups[MAX_USER_GROUPS]; 

int32_t alloc_block(int32_t size);
void free_block(int32_t start, int32_t size);
int fs_check_permission(FileEntry *fe, int mode);
int32_t find_user_by_name(const char* name, User* out_user);
int32_t find_group_by_name(const char* name, Group* out_group);
void reload_current_user_groups();

//Memory management
void fs_save_superblock() {
    fseek(disk, 0, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, disk);
    fflush(disk);
}

#include <time.h>

void fs_stress_test() {
    printf("Starting CORRCTED Stress Test on LINKED LIST FileSystem...\n");
    printf("Disk Size: %d bytes\n", DISK_SIZE);

    // شروع زمان‌گیری
    clock_t start = clock();

    int FILE_COUNT = 1000; // تعداد فایل‌ها
    char filename[32];
    char data[] = "This is a test data for benchmarking...";

    // مرحله ۱: ایجاد فایل‌ها و تخصیص فضا
    printf("Step 1: Creating %d files with 4KB data blocks...\n", FILE_COUNT);
    for (int i = 0; i < FILE_COUNT; i++) {
        sprintf(filename, "file_%d", i);
        
        // 1. ایجاد فایل (تخصیص FileEntry)
        int ret = fs_open(filename, 1); 
        if (ret == -1) {
            printf("Error: Could not create file %d (Disk Full or Limit Reached)\n", i);
            break; 
        }

        // 2. تخصیص واقعی بلوک داده (این بخش گلوگاه اصلی است)
        // ما 4096 بایت درخواست می‌کنیم تا alloc_block مجبور به جستجو و تقسیم بلوک شود
        int32_t data_block_pos = alloc_block(4096);
        
        if (data_block_pos == -1) {
            printf("Error: Disk Full during data allocation for file %d\n", i);
            break;
        }

        // 3. نوشتن در بلوک تخصیص داده شده (نه روی هدر لیست آزاد!)
        fs_write(data_block_pos, sizeof(data), data);
        
        // نمایش پیشرفت هر 100 فایل
        if (i % 100 == 0) printf(".");
    }
    printf("\nAllocation Done.\n");

    // مرحله ۲: حذف نیمی از فایل‌ها (برای ایجاد فرگمنت و تکه‌تکه شدن دیسک)
    printf("Step 2: Deleting half of the files...\n");
    for (int i = 0; i < FILE_COUNT; i += 2) {
        sprintf(filename, "file_%d", i);
        fs_rm(filename);
        // توجه: در سیستم واقعی باید بلوک داده هم free شود، اما fs_rm فعلی شما
        // فقط FileEntry را حذف می‌کند. برای تست تخصیص همین کافیست.
    }

    // مرحله ۳: ایجاد فایل‌های جدید (مجبور کردن سیستم به جستجو در فضاهای خالی شده)
    printf("Step 3: Creating 200 new files in fragmented space...\n");
    for (int i = 0; i < 200; i++) {
        sprintf(filename, "new_file_%d", i);
        fs_open(filename, 1);
        int32_t data_pos = alloc_block(4096); // تلاش برای پر کردن جاهای خالی
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("\n------------------------------------------------\n");
    printf("Stress Test Completed in %.4f seconds.\n", time_taken);
    printf("------------------------------------------------\n");
}



int32_t alloc_block(int32_t size) {
    int32_t current_pos = sb.first_free_block;
    int32_t prev_pos = -1;

    while (current_pos != -1) {
        FreeBlock fb;
        fseek(disk, current_pos, SEEK_SET);
        fread(&fb, sizeof(FreeBlock), 1, disk);

        if (fb.size >= size) {
            int32_t remaining_size = fb.size - size;
            if (remaining_size < sizeof(FreeBlock)) {
                if (prev_pos == -1) sb.first_free_block = fb.next;
                else {
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
                int32_t new_header_pos = current_pos + size;
                FreeBlock new_fb;
                new_fb.size = remaining_size;
                new_fb.next = fb.next;
                fseek(disk, new_header_pos, SEEK_SET);
                fwrite(&new_fb, sizeof(FreeBlock), 1, disk);
                if (prev_pos == -1) sb.first_free_block = new_header_pos;
                else {
                    FreeBlock prev_fb;
                    fseek(disk, prev_pos, SEEK_SET);
                    fread(&prev_fb, sizeof(FreeBlock), 1, disk);
                    prev_fb.next = new_header_pos;
                    fseek(disk, prev_pos, SEEK_SET);
                    fwrite(&prev_fb, sizeof(FreeBlock), 1, disk);
                }
                fs_save_superblock();
                return current_pos;
            }
        }
        prev_pos = current_pos;
        current_pos = fb.next;
    }
    return -1;
}

void free_block(int32_t start, int32_t size) {
    FreeBlock new_fb;
    new_fb.size = size;
    new_fb.next = sb.first_free_block;
    fseek(disk, start, SEEK_SET);
    fwrite(&new_fb, sizeof(FreeBlock), 1, disk);
    sb.first_free_block = start;
    fs_save_superblock();
}

//Initialization

void fs_create_root_user() {
    // Create Root Group
    int32_t g_pos = alloc_block(sizeof(Group));
    Group root_group;
    root_group.gid = 0;
    strcpy(root_group.groupname, "root");
    root_group.next = -1;
    fseek(disk, g_pos, SEEK_SET);
    fwrite(&root_group, sizeof(Group), 1, disk);
    
    sb.first_group = g_pos;
    sb.next_gid = 1;

    // Create Root User
    int32_t u_pos = alloc_block(sizeof(User));
    User root_user;
    root_user.uid = 0;
    strcpy(root_user.username, "root");
    // Init groups
    for(int i=0; i<MAX_USER_GROUPS; i++) root_user.gids[i] = -1;
    root_user.gids[0] = 0; // Member of root group
    root_user.next = -1;
    
    fseek(disk, u_pos, SEEK_SET);
    fwrite(&root_user, sizeof(User), 1, disk);

    sb.first_user = u_pos;
    sb.next_uid = 1;
    
    fs_save_superblock();
    
    // Set context
    current_uid = 0;
    current_gid = 0;
    memset(current_user_groups, -1, sizeof(current_user_groups));
    current_user_groups[0] = 0;
}

void fs_open_disk() {
    disk = fopen("filesys.db", "r+b");
    if (!disk) {
        printf("Formatting new filesystem...\n");
        disk = fopen("filesys.db", "w+b");
        if (!disk) { perror("Error creating disk"); exit(1); }

        sb.magic = MAGIC;
        sb.version = 2; // Updated version
        sb.file_count = 0;
        sb.first_file = -1;
        sb.first_free_block = sizeof(SuperBlock);
        sb.first_user = -1;
        sb.first_group = -1;

        fwrite(&sb, sizeof(SuperBlock), 1, disk);

        FreeBlock initial_block;
        initial_block.size = DISK_SIZE - sizeof(SuperBlock);
        initial_block.next = -1;
        fseek(disk, sb.first_free_block, SEEK_SET);
        fwrite(&initial_block, sizeof(FreeBlock), 1, disk);
        
        fs_create_root_user();
        printf("Filesystem initialized with root user.\n");
    } else {
        fread(&sb, sizeof(SuperBlock), 1, disk);
        if (sb.magic != MAGIC) {
            printf("Invalid filesystem magic number. Please delete filesys.db\n");
            exit(1);
        }
        // Load root context
        current_uid = 0;
        current_gid = 0;
        reload_current_user_groups();
    }
}

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

// Returns 1 if allowed, 0 otherwise
int fs_check_permission(FileEntry *fe, int required_mode) {
    // 1. Root allows everything
    if (current_uid == 0) return 1;

    int file_mode = fe->permission; // e.g., 0755
    int allowed = 0;

    //Owner (bits 6-8), Group (bits 3-5), Others (bits 0-2)
    int owner_perm = (file_mode >> 6) & 0x7;
    int group_perm = (file_mode >> 3) & 0x7;
    int other_perm = file_mode & 0x7;

    if (current_uid == fe->uid) {
        // Check Owner Permissions
        if ((owner_perm & required_mode) == required_mode) allowed = 1;
    } else {
        // Check Group Permissions
        int in_group = 0;
        if (current_gid == fe->gid) in_group = 1;
        else {
            for (int i=0; i < MAX_USER_GROUPS; i++) {
                if (current_user_groups[i] == fe->gid) {
                    in_group = 1;
                    break;
                }
            }
        }

        if (in_group) {
             if ((group_perm & required_mode) == required_mode) allowed = 1;
        } else {
            // Check Other Permissions
             if ((other_perm & required_mode) == required_mode) allowed = 1;
        }
    }

    if (!allowed) {
        printf("Permission denied: UID %d requesting access %d on file owned by %d:%d mode %o\n", 
               current_uid, required_mode, fe->uid, fe->gid, fe->permission);
    }
    return allowed;
}

int32_t find_user_by_name(const char* name, User* out_user) {
    int32_t pos = sb.first_user;
    while(pos != -1) {
        fseek(disk, pos, SEEK_SET);
        fread(out_user, sizeof(User), 1, disk);
        if(strcmp(out_user->username, name) == 0) return pos;
        pos = out_user->next;
    }
    return -1;
}

int32_t find_group_by_name(const char* name, Group* out_group) {
    int32_t pos = sb.first_group;
    while(pos != -1) {
        fseek(disk, pos, SEEK_SET);
        fread(out_group, sizeof(Group), 1, disk);
        if(strcmp(out_group->groupname, name) == 0) return pos;
        pos = out_group->next;
    }
    return -1;
}

void reload_current_user_groups() {
    int32_t pos = sb.first_user;
    while(pos != -1) {
        User u;
        fseek(disk, pos, SEEK_SET);
        fread(&u, sizeof(User), 1, disk);
        if(u.uid == current_uid) {
            memcpy(current_user_groups, u.gids, sizeof(current_user_groups));
            // Primary group is first one
            current_gid = u.gids[0]; 
            return;
        }
        pos = u.next;
    }
}

//User and group

void fs_useradd(const char *username) {
    if (current_uid != 0) { printf("Permission denied: Only root can add users.\n"); return; }
    
    User dummy;
    if (find_user_by_name(username, &dummy) != -1) {
        printf("Error: User '%s' already exists.\n", username);
        return;
    }

    int32_t pos = alloc_block(sizeof(User));
    if (pos == -1) { printf("Disk full.\n"); return; }

    User u;
    u.uid = sb.next_uid++;
    strcpy(u.username, username);
    for(int i=0; i<MAX_USER_GROUPS; i++) u.gids[i] = -1;
    
    u.next = sb.first_user;
    
    fseek(disk, pos, SEEK_SET);
    fwrite(&u, sizeof(User), 1, disk);
    
    sb.first_user = pos;
    fs_save_superblock();
    printf("User '%s' added with UID %d.\n", username, u.uid);
}

void fs_userdel(const char *username) {
    if (current_uid != 0) { printf("Permission denied.\n"); return; }
    if (strcmp(username, "root") == 0) { printf("Cannot delete root.\n"); return; }

    int32_t prev_pos = -1;
    int32_t curr_pos = sb.first_user;
    
    while(curr_pos != -1) {
        User u;
        fseek(disk, curr_pos, SEEK_SET);
        fread(&u, sizeof(User), 1, disk);
        
        if (strcmp(u.username, username) == 0) {
            if (prev_pos == -1) sb.first_user = u.next;
            else {
                User prev;
                fseek(disk, prev_pos, SEEK_SET);
                fread(&prev, sizeof(User), 1, disk);
                prev.next = u.next;
                fseek(disk, prev_pos, SEEK_SET);
                fwrite(&prev, sizeof(User), 1, disk);
            }
            free_block(curr_pos, sizeof(User));
            fs_save_superblock();
            printf("User '%s' deleted.\n", username);
            return;
        }
        prev_pos = curr_pos;
        curr_pos = u.next;
    }
    printf("User not found.\n");
}

void fs_groupadd(const char *groupname) {
    if (current_uid != 0) { printf("Permission denied.\n"); return; }
    
    Group dummy;
    if (find_group_by_name(groupname, &dummy) != -1) {
        printf("Error: Group '%s' exists.\n", groupname);
        return;
    }

    int32_t pos = alloc_block(sizeof(Group));
    Group g;
    g.gid = sb.next_gid++;
    strcpy(g.groupname, groupname);
    g.next = sb.first_group;
    
    fseek(disk, pos, SEEK_SET);
    fwrite(&g, sizeof(Group), 1, disk);
    
    sb.first_group = pos;
    fs_save_superblock();
    printf("Group '%s' added with GID %d.\n", groupname, g.gid);
}

void fs_groupdel(const char *groupname) {
    if (current_uid != 0) { printf("Permission denied.\n"); return; }
    if (strcmp(groupname, "root") == 0) { printf("Cannot delete root group.\n"); return; }

    int32_t prev = -1;
    int32_t curr = sb.first_group;
    
    while(curr != -1) {
        Group g;
        fseek(disk, curr, SEEK_SET);
        fread(&g, sizeof(Group), 1, disk);
        if (strcmp(g.groupname, groupname) == 0) {
            if (prev == -1) sb.first_group = g.next;
            else {
                Group p;
                fseek(disk, prev, SEEK_SET);
                fread(&p, sizeof(Group), 1, disk);
                p.next = g.next;
                fseek(disk, prev, SEEK_SET);
                fwrite(&p, sizeof(Group), 1, disk);
            }
            free_block(curr, sizeof(Group));
            fs_save_superblock();
            printf("Group '%s' deleted.\n", groupname);
            return;
        }
        prev = curr;
        curr = g.next;
    }
    printf("Group not found.\n");
}

void fs_usermod(const char *username, const char *groupname) {
    if (current_uid != 0) { printf("Permission denied.\n"); return; }

    User u; 
    int32_t u_pos = find_user_by_name(username, &u);
    if (u_pos == -1) { printf("User not found.\n"); return; }

    Group g;
    if (find_group_by_name(groupname, &g) == -1) { printf("Group not found.\n"); return; }

    // Check if already member
    for(int i=0; i<MAX_USER_GROUPS; i++) {
        if(u.gids[i] == g.gid) {
            printf("User already in group.\n");
            return;
        }
    }

    // Add to first empty slot
    int added = 0;
    for(int i=0; i<MAX_USER_GROUPS; i++) {
        if(u.gids[i] == -1) {
            u.gids[i] = g.gid;
            added = 1;
            break;
        }
    }

    if(added) {
        fseek(disk, u_pos, SEEK_SET);
        fwrite(&u, sizeof(User), 1, disk);
        printf("User '%s' added to group '%s'.\n", username, groupname);
    } else {
        printf("User group limit reached.\n");
    }
}

void fs_login(const char *username) {
    User u;
    if (find_user_by_name(username, &u) != -1) {
        current_uid = u.uid;
        reload_current_user_groups();
        printf("Logged in as %s (UID: %d, GID: %d)\n", username, current_uid, current_gid);
        fs_close(); // Close open file on user switch
    } else {
        printf("User not found.\n");
    }
}

int32_t fs_get_current_uid() {
    return current_uid;
}

// --- FILE OPERATIONS (MODIFIED) ---

int fs_open(const char *name, int flags) {
    int32_t pos = fs_find_file(name);

    if (pos != -1) { 
        // File exists, check permissions?
        // Open usually doesn't imply R/W yet, but let's assume if you open,
        // you want to read. If flags has 1 (create), and file exists, standard open.
        
        fseek(disk, pos, SEEK_SET);
        fread(&current_file, sizeof(FileEntry), 1, disk);
        
        // Simple check: do we have read permission to "open" it?
        if (!fs_check_permission(&current_file, R_OK)) return -1;

        current_file_pos = pos;
        return 0;
    }

    if (!(flags & 1)) {
        printf("File not found.\n");
        return -1;
    }

    // Create File
    int32_t fe_pos = alloc_block(sizeof(FileEntry));
    if (fe_pos == -1) { printf("Disk full.\n"); return -1; }

    FileEntry fe;
    memset(&fe, 0, sizeof(fe));
    strncpy(fe.name, name, MAX_FILENAME - 1);
    fe.size = 0;
    fe.permission = 0644; // Default: rw-r--r-- (octal)
    fe.uid = current_uid;
    fe.gid = current_gid;
    fe.data_block = -1;
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
    if (current_file_pos == -1) { printf("No file open.\n"); return -1; }
    
    // Permission Check
    if (!fs_check_permission(&current_file, W_OK)) return -1;

    if (current_file.data_block == -1) {
        current_file.data_block = alloc_block(BLOCK_SIZE);
        if (current_file.data_block == -1) return -1;
    }

    if (pos + n_bytes > BLOCK_SIZE) n_bytes = BLOCK_SIZE - pos;
    if (n_bytes <= 0) return 0;

    fseek(disk, current_file.data_block + pos, SEEK_SET);
    fwrite(buffer, 1, n_bytes, disk);

    if (pos + n_bytes > current_file.size) current_file.size = pos + n_bytes;

    fseek(disk, current_file_pos, SEEK_SET);
    fwrite(&current_file, sizeof(FileEntry), 1, disk);

    return n_bytes;
}

int fs_read(int pos, int n_bytes, char *buffer) {
    if (current_file_pos == -1) { printf("No file open.\n"); return -1; }

    // Permission Check
    if (!fs_check_permission(&current_file, R_OK)) return -1;

    if (current_file.data_block == -1) return 0;
    if (pos >= current_file.size) return 0;

    int available = current_file.size - pos;
    if (n_bytes > available) n_bytes = available;

    fseek(disk, current_file.data_block + pos, SEEK_SET);
    fread(buffer, 1, n_bytes, disk);
    buffer[n_bytes] = '\0';
    return n_bytes;
}

void fs_rm(const char *name) {
    int32_t prev_pos = -1;
    int32_t curr_pos = sb.first_file;

    while (curr_pos != -1) {
        FileEntry fe;
        fseek(disk, curr_pos, SEEK_SET);
        fread(&fe, sizeof(FileEntry), 1, disk);

        if (strcmp(fe.name, name) == 0) {
            // Check Permission: Need Write permission on "directory".
            // Since we have no directories, we require W access on the file itself to delete it?
            // Or only Owner/Root can delete. Let's say Owner or Root.
            if (current_uid != 0 && current_uid != fe.uid) {
                printf("Permission denied: Only owner or root can delete.\n");
                return;
            }

            if (prev_pos == -1) sb.first_file = fe.next;
            else {
                FileEntry prev;
                fseek(disk, prev_pos, SEEK_SET);
                fread(&prev, sizeof(FileEntry), 1, disk);
                prev.next = fe.next;
                fseek(disk, prev_pos, SEEK_SET);
                fwrite(&prev, sizeof(FileEntry), 1, disk);
            }

            if (fe.data_block != -1) free_block(fe.data_block, BLOCK_SIZE);
            free_block(curr_pos, sizeof(FileEntry));

            sb.file_count--;
            fs_save_superblock();
            if (current_file_pos == curr_pos) current_file_pos = -1;
            printf("File deleted.\n");
            return;
        }
        prev_pos = curr_pos;
        curr_pos = fe.next;
    }
    printf("File not found.\n");
}

void fs_shrink(int new_size) {
    if (current_file_pos == -1) return;
    if (!fs_check_permission(&current_file, W_OK)) return; // Needs write perm
    
    if (new_size < 0) new_size = 0;
    if (new_size >= current_file.size) return;

    current_file.size = new_size;
    fseek(disk, current_file_pos, SEEK_SET);
    fwrite(&current_file, sizeof(FileEntry), 1, disk);
    printf("Shrunk to %d.\n", new_size);
}

// --- PERMISSION COMMANDS ---

void fs_chmod(const char *path, int mode) {
    int32_t pos = fs_find_file(path);
    if (pos == -1) { printf("File not found.\n"); return; }
    
    FileEntry fe;
    fseek(disk, pos, SEEK_SET);
    fread(&fe, sizeof(FileEntry), 1, disk);

    if (current_uid != 0 && current_uid != fe.uid) {
        printf("Permission denied: Only owner/root can chmod.\n");
        return;
    }

    fe.permission = mode;
    fseek(disk, pos, SEEK_SET);
    fwrite(&fe, sizeof(FileEntry), 1, disk);
    
    // If current file is this one, update memory copy
    if (current_file_pos == pos) current_file = fe;
    
    printf("Changed mode of '%s' to %o\n", path, mode);
}

void fs_chown(const char *path, const char *owner_user, const char *owner_group) {
    if (current_uid != 0) { printf("Permission denied: Only root can chown.\n"); return; }

    int32_t pos = fs_find_file(path);
    if (pos == -1) { printf("File not found.\n"); return; }

    User u; 
    Group g;
    int32_t new_uid = -1, new_gid = -1;

    if (find_user_by_name(owner_user, &u) != -1) new_uid = u.uid;
    else { printf("User '%s' not found.\n", owner_user); return; }

    if (find_group_by_name(owner_group, &g) != -1) new_gid = g.gid;
    else { printf("Group '%s' not found.\n", owner_group); return; }

    FileEntry fe;
    fseek(disk, pos, SEEK_SET);
    fread(&fe, sizeof(FileEntry), 1, disk);

    fe.uid = new_uid;
    fe.gid = new_gid;
    
    fseek(disk, pos, SEEK_SET);
    fwrite(&fe, sizeof(FileEntry), 1, disk);
    if (current_file_pos == pos) current_file = fe;

    printf("Changed ownership of '%s'.\n", path);
}

void fs_chgrp(const char *path, const char *groupname) {
    int32_t pos = fs_find_file(path);
    if (pos == -1) { printf("File not found.\n"); return; }

    FileEntry fe;
    fseek(disk, pos, SEEK_SET);
    fread(&fe, sizeof(FileEntry), 1, disk);

    // Only Owner or Root can change group
    if (current_uid != 0 && current_uid != fe.uid) {
        printf("Permission denied.\n");
        return;
    }

    Group g;
    if (find_group_by_name(groupname, &g) == -1) { printf("Group not found.\n"); return; }

    fe.gid = g.gid;
    fseek(disk, pos, SEEK_SET);
    fwrite(&fe, sizeof(FileEntry), 1, disk);
    if (current_file_pos == pos) current_file = fe;
    printf("Changed group of '%s'.\n", path);
}

void fs_getfacl(const char *path) {
    int32_t pos = fs_find_file(path);
    if (pos == -1) { printf("File not found.\n"); return; }

    FileEntry fe;
    fseek(disk, pos, SEEK_SET);
    fread(&fe, sizeof(FileEntry), 1, disk);

    printf("# file: %s\n", fe.name);
    printf("# owner: %d\n", fe.uid);
    printf("# group: %d\n", fe.gid);
    printf("user::%s\n", (fe.permission & 0400) ? "r" : "-"); 
    // Simplified display...
    printf("permissions: %o\n", fe.permission);
}

void fs_close() {
    current_file_pos = -1;
}

void fs_stats() {
    // ... existing stats ...
    printf("--- Extended Stats ---\n");
    printf("Current User UID: %d\n", current_uid);
    printf("Next UID: %d, Next GID: %d\n", sb.next_uid, sb.next_gid);
    printf("----------------------\n");
}

void fs_visualize_free_list() {
    // Existing visualization...
     printf("--- Free Space Visualizer ---\n");
    int32_t pos = sb.first_free_block;
    int i = 0;
    while (pos != -1) {
        FreeBlock fb;
        fseek(disk, pos, SEEK_SET);
        fread(&fb, sizeof(FreeBlock), 1, disk);
        printf("Block %d: [At %d] Size=%d\n", i++, pos, fb.size);
        pos = fb.next;
    }
}

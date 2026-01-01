#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"

int main() {
    fs_open_disk();

    printf("Extended FS CLI (Bitmap Version).\n");
    printf("Commands: useradd, userdel, groupadd, usermod, login\n");
    printf("File Ops: open, read, write, shrink, rm, chmod, chown, getfacl\n");
    printf("System: stats, bitmap, stressTest, exit\n");

    while (1) {
        char cmd[32];
        char line[512];

        printf("[%d]> ", fs_get_current_uid());
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) break;
        int items = sscanf(line, "%s", cmd);
        if (items < 1) continue;

        if (strcmp(cmd, "useradd") == 0) {
            char u[32];
            if (sscanf(line, "%*s %s", u) == 1) fs_useradd(u);
        }
        else if (strcmp(cmd, "userdel") == 0) {
            char u[32];
            if (sscanf(line, "%*s %s", u) == 1) fs_userdel(u);
        }
        else if (strcmp(cmd, "groupadd") == 0) {
            char g[32];
            if (sscanf(line, "%*s %s", g) == 1) fs_groupadd(g);
        }
        else if (strcmp(cmd, "usermod") == 0) {
            char arg1[32], arg2[32]; // Simplification: usermod user group
            if(sscanf(line, "%*s %s %s", arg1, arg2) == 2) fs_usermod(arg1, arg2);
            else printf("Usage: usermod <user> <group>\n");
        }
        else if (strcmp(cmd, "login") == 0) {
             char u[32];
             if (sscanf(line, "%*s %s", u) == 1) fs_login(u);
        }
        else if (strcmp(cmd, "chmod") == 0) {
            char path[32];
            int mode;
            if (sscanf(line, "%*s %s %o", path, &mode) == 2) fs_chmod(path, mode);
        }
        else if (strcmp(cmd, "chown") == 0) {
             char path[32], ug[64];
             if (sscanf(line, "%*s %s %s", path, ug) == 2) {
                 char *colon = strchr(ug, ':');
                 if (colon) { *colon = '\0'; fs_chown(path, ug, colon + 1); }
             }
        }
        else if (strcmp(cmd, "getfacl") == 0) {
            char path[32];
            if (sscanf(line, "%*s %s", path) == 1) fs_getfacl(path);
        }
        else if (strcmp(cmd, "open") == 0) {
            char name[32];
            int flag;
            if(sscanf(line, "%*s %s %d", name, &flag) == 2) fs_open(name, flag);
        }
        else if (strcmp(cmd, "write") == 0) {
             int pos;
             char data[512] = {0};
             char* first_space = strchr(line, ' ');
             if (first_space) {
                 char* second_space = strchr(first_space + 1, ' ');
                 if (second_space) {
                     pos = atoi(first_space + 1);
                     strncpy(data, second_space + 1, sizeof(data)-1);
                     data[strcspn(data, "\n")] = 0;
                     fs_write(pos, strlen(data), data);
                 }
             }
        }
        else if (strcmp(cmd, "read") == 0) {
            int pos, n;
            if(sscanf(line, "%*s %d %d", &pos, &n) == 2) {
                char buf[1024];
                int r = fs_read(pos, n, buf);
                if (r >= 0) printf("Read: [%s]\n", buf);
            }
        }
        else if (strcmp(cmd, "rm") == 0) {
             char name[32];
             if(sscanf(line, "%*s %s", name) == 1) fs_rm(name);
        }
        else if (strcmp(cmd, "shrink") == 0) {
             int sz;
             if(sscanf(line, "%*s %d", &sz) == 1) fs_shrink(sz);
        }
        else if (strcmp(cmd, "stats") == 0) fs_stats();
        else if (strcmp(cmd, "bitmap") == 0) fs_visualize_bitmap();
        // --- NEW COMMAND ---
        else if (strcmp(cmd, "stressTest") == 0) fs_stress_test();
        else if (strcmp(cmd, "exit") == 0) break;
        else printf("Unknown command.\n");
    }
    return 0;
}

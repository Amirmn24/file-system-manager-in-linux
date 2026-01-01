#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"

int main() {
    fs_open_disk();

    while (1) {
        char cmd[32];
        char line[512];

        printf("[%d]> ", fs_get_current_uid()); // Prompt shows current UID
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) break;
        int items = sscanf(line, "%s", cmd);
        if (items < 1) continue;

        //New commands
        if (strcmp(cmd, "useradd") == 0) {
            char u[32];
            if (sscanf(line, "%*s %s", u) == 1) fs_useradd(u);
            else printf("Usage: useradd <username>\n");
        }
        else if (strcmp(cmd, "userdel") == 0) {
            char u[32];
            if (sscanf(line, "%*s %s", u) == 1) fs_userdel(u);
            else printf("Usage: userdel <username>\n");
        }
        else if (strcmp(cmd, "groupadd") == 0) {
            char g[32];
            if (sscanf(line, "%*s %s", g) == 1) fs_groupadd(g);
            else printf("Usage: groupadd <groupname>\n");
        }
        else if (strcmp(cmd, "groupdel") == 0) {
            char g[32];
            if (sscanf(line, "%*s %s", g) == 1) fs_groupdel(g);
            else printf("Usage: groupdel <groupname>\n");
        }
        else if (strcmp(cmd, "usermod") == 0) {

            char u[32], g[32], flag[8];
            char arg1[32], arg2[32], arg3[32];
            int n = sscanf(line, "%*s %s %s %s", arg1, arg2, arg3);

            if (n == 3 && strcmp(arg1, "-aG") == 0) {
                 fs_usermod(arg2, arg3);
            } else {
                 printf("Usage: usermod -aG <user> <group>\n");
            }
        }
        else if (strcmp(cmd, "login") == 0) {
             char u[32];
             if (sscanf(line, "%*s %s", u) == 1) fs_login(u);
             else printf("Usage: login <username>\n");
        }
        else if (strcmp(cmd, "chmod") == 0) {
            char path[32];
            int mode; // Octal input needs careful parsing
            if (sscanf(line, "%*s %s %o", path, &mode) == 2) fs_chmod(path, mode);
            else printf("Usage: chmod <file> <octal_mode> (e.g. 755)\n");
        }
        else if (strcmp(cmd, "chown") == 0) {
             // chown path user:group
             char path[32], ug[64];
             if (sscanf(line, "%*s %s %s", path, ug) == 2) {
                 char *colon = strchr(ug, ':');
                 if (colon) {
                     *colon = '\0';
                     fs_chown(path, ug, colon + 1);
                 } else {
                     printf("Usage: chown <file> <user>:<group>\n");
                 }
             } else printf("Usage: chown <file> <user>:<group>\n");
        }
        else if (strcmp(cmd, "chgrp") == 0) {
            char path[32], g[32];
            if (sscanf(line, "%*s %s %s", path, g) == 2) fs_chgrp(path, g);
            else printf("Usage: chgrp <file> <group>\n");
        }
        else if (strcmp(cmd, "getfacl") == 0) {
            char path[32];
            if (sscanf(line, "%*s %s", path) == 1) fs_getfacl(path);
            else printf("Usage: getfacl <file>\n");
        }

        // --- OLD COMMANDS (Mostly same) ---
        else if (strcmp(cmd, "open") == 0) {
            char name[32];
            int flag;
            if(sscanf(line, "%*s %s %d", name, &flag) == 2) fs_open(name, flag);
            else printf("Usage: open <filename> <flag 1=create>\n");
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
                else if (strcmp(cmd, "stressTest") == 0) {
            fs_stress_test();
        }
        else if (strcmp(cmd, "stats") == 0) fs_stats();
        else if (strcmp(cmd, "exit") == 0) break;
        else printf("Unknown command.\n");
    }
    return 0;
}

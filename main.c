#include <stdio.h>
#include <string.h>
#include "fs.h"

int main() {
    fs_open_disk();

    printf("Filesystem CLI. Available commands: open, read, write, shrink, rm, stats, viz, close, exit\n");

    while (1) {
        char cmd[32];
        char line[512];

        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) break;
        
        int items = sscanf(line, "%s", cmd);
        if (items < 1) continue;

        if (strcmp(cmd, "open") == 0) {
            char name[32];
            int flag;
            if(sscanf(line, "%*s %s %d", name, &flag) == 2){
                fs_open(name, flag);
            } else {
                printf("Usage: open <filename> <flag>\n");
            }
        }
        else if (strcmp(cmd, "write") == 0) {
            int pos;
            char data[512] = {0};
            // Manually parse to handle spaces in data
            char* first_space = strchr(line, ' ');
            if (first_space) {
                char* second_space = strchr(first_space + 1, ' ');
                if (second_space) {
                    pos = atoi(first_space + 1);
                    strncpy(data, second_space + 1, sizeof(data) - 1);
                    // Remove trailing newline
                    data[strcspn(data, "\n")] = 0;
                    fs_write(pos, strlen(data), data);
                } else {
                     printf("Usage: write <pos> <data to write>\n");
                }
            } else {
                 printf("Usage: write <pos> <data to write>\n");
            }
        }
        else if (strcmp(cmd, "read") == 0) {
            int pos, n;
            if(sscanf(line, "%*s %d %d", &pos, &n) == 2) {
                char buf[1024];
                int r = fs_read(pos, n, buf);
                if (r >= 0) printf("Read %d bytes: [%s]\n", r, buf);
            } else {
                printf("Usage: read <pos> <n_bytes>\n");
            }
        }
        else if (strcmp(cmd, "shrink") == 0) {
            int s;
            if(sscanf(line, "%*s %d", &s) == 1){
                fs_shrink(s);
            } else {
                 printf("Usage: shrink <new_size>\n");
            }
        }
        else if (strcmp(cmd, "rm") == 0) {
            char name[32];
            if(sscanf(line, "%*s %s", name) == 1){
                fs_rm(name);
            } else {
                 printf("Usage: rm <filename>\n");
            }
        }
        else if (strcmp(cmd, "stats") == 0) {
            fs_stats();
        }
        else if (strcmp(cmd, "viz") == 0) {
            fs_visualize_free_list();
        }
        else if (strcmp(cmd, "close") == 0) {
            fs_close();
        }
        else if (strcmp(cmd, "exit") == 0) {
            break;
        }
        else {
            printf("Unknown command: %s\n", cmd);
        }
    }

    return 0;
}

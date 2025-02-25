#include "../include/utils.h"
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

int valid_directory_path(const char* path) {
    //old but didnt support /home/.git - char* pattern = "^\\/([a-zA-Z0-9._-]+\\/)*[a-zA-Z0-9_-]+\\/?$";
    char* pattern = "^\\/([a-zA-Z0-9._-]+\\/)*([a-zA-Z0-9._-]+|\\.[a-zA-Z0-9_-]+)\\/?$";

    regex_t reg;
    int ret;

    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if(ret) {
        perror("Could not compile regex.\n");
        return 0;
    }

    ret = regexec(&reg, path, 0, NULL, 0);
    regfree(&reg);

    DIR* dir = opendir(path);
    if (!dir) { return 0; }

    closedir(dir);
    return 1;
}

const char* format_priority(int priority) {
    if(priority == 0) return "*";
    if(priority == 1) return "**";
    if(priority == 2) return "***";
    return "";
}

const char* format_progress(float progress) {
    char* temp = (char*)malloc(4096 * sizeof(char));
    if(temp == NULL) {perror("Error during malloc."); return "";}
    snprintf(temp, 4096, "%d%% in progress", (int)progress);

    return temp;
}

char* format_details(const char* path) {
    char* temp = (char*)malloc(4096 * sizeof(char));
    if(temp == NULL) {perror("Error during malloc."); return "";}
    snprintf(temp, 4096, "%d files, %d dirs", get_total_files(path), get_total_dirs(path));

    return temp;
}


char* format_hashtag(int n_hashtags) {
    char* temp = (char*)malloc(n_hashtags + 1);

    for(int i = 0; i < n_hashtags; ++i)
        temp[i] = '#';
    temp[n_hashtags] = 0;

    return temp;
}

char* format_bytes(long long bytes) {
    const long long KB = 1024;
    const long long MB = KB * 1024;
    const long long GB = MB * 1024;
    
    char* temp = (char*)malloc(4096);

    if(bytes < KB) {
        snprintf(temp, 4096, "%lld  B", bytes);
    }

    if(KB <= bytes && bytes < MB) {
        snprintf(temp, 4096, "%.2f KB", (float)bytes / KB);
    }

    if(MB <= bytes && bytes <= GB) {
        snprintf(temp, 4096, "%.2f MB", (float)bytes / MB);
    }

    if(GB <= bytes) {
        snprintf(temp, 4096, "%.2f GB", (float)bytes / GB);
    }

    return temp;
}

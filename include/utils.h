#ifndef UTILS_H
#define UTILS_H

int valid_directory_path(const char* path);
const char* format_priority(int priority);
const char* format_progress(float progress);
char* format_details(const char* path);
char* format_hashtag(int n_hashtags);
char* format_bytes(long long bytes);

#endif
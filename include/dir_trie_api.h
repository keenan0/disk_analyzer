#ifndef DIR_TRIE_API_H
#define DIR_TRIE_API_H

#define MAX_SUBDIRS 1000
#define _DEBUG 1

struct trie_node;
typedef struct trie_node {
    int is_analysed;

    const char* dir_name;
    int n_files;
    int n_directories;

    long long size;

    struct trie_node* subdirectories[MAX_SUBDIRS];
} trie_node;



trie_node* create_node(const char* name);
int arrcmp(const char** arr, const char* comp_string);
trie_node* insert_directory_rec(trie_node* root, const char* path, FILE* fp);
trie_node* insert_node(trie_node* root, const char* path, FILE* fp);
void print(trie_node* root, int indent, FILE* fp);

#endif
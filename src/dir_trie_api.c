#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "../include/dir_trie_api.h"

trie_node* create_node(const char* name) {
    trie_node* node = (trie_node*)malloc(sizeof(trie_node));
    
    node->dir_name = strdup(name);
    node->is_analysed = 0;
    node->n_directories = 0;
    node->n_files = 0;
    node->size = 0;
    for(int i = 0; i < MAX_SUBDIRS; ++i) node->subdirectories[i] = NULL;
    
    if(_DEBUG) 
        printf("\tCreated node '%s'.\n", name);

    return node;
}

int arrcmp(const char** arr, const char* comp_string) {
    int index = 0;

    while(arr[index] != NULL) {
        if(strcmp(arr[index], comp_string) == 0)
            return 1;

        index++;
    }

    return 0;
}

trie_node* insert_directory_rec(trie_node* root, const char* path, FILE* fp) {
    /*
        @return trie_node* 

        Function computes the size of the tree that starts at root. Root has to exist. Any other node that will be traversed recursively will be created using the create_node() method. 

        This function is called by the insert_node() method once it passes the validation process.

        Caching is also supported: 
            if /home/user/test was computed
            and we want to now compute /home/user then the node from the trie will have all the necessary information in there and will return  
    */

    if(root == NULL) return NULL;
    if(root->is_analysed == 1) return root;
    
    const char* exclude[] = {
        ".",
        "..",
        // "sys",
        // "dev",
        // "mnt",
        // "proc",
        // "snap",
        // "boot",
        NULL
    };
    
    DIR* dir = opendir(path);
    if(dir == NULL) {perror("Error opening directory.\n"); return NULL;}

    fprintf(fp, "Added %s to the trie.\n", path);

    struct dirent* entry;
    char full_path[PATH_MAX];
    while(entry = readdir(dir)) {
        if(arrcmp(exclude, entry->d_name) == 1) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat file_metadata;
        if(stat(full_path, &file_metadata) != 0) {perror("Error getting file or directory info"); return NULL;}

        if(S_ISDIR(file_metadata.st_mode)) {
            // The found entry is a directory

            // Search if it exists already
            int found = -1;
            for(int i = 0; i < root->n_directories; ++i)
                if(root->subdirectories[i]->is_analysed && strcmp(root->subdirectories[i]->dir_name, entry->d_name) == 0) {
                    found = i;
                    break;
                }

            trie_node* new_node;
            if(found != -1) {
                new_node = root->subdirectories[found];
            } else {
                new_node = create_node(entry->d_name);
                root->subdirectories[root->n_directories++] = new_node;
            }
            
            trie_node* cached = insert_directory_rec(new_node, full_path, fp);
            root->n_files += cached->n_files;
            root->size += cached->size;
        } else if(S_ISREG(file_metadata.st_mode)) {
            // The found entry is a file
            root->n_files++;
            root->size += file_metadata.st_size;
        }
    }

    closedir(dir);

    root->is_analysed = 1;
    if(_DEBUG) 
        printf("\t\t\tFinished analysing '%s'.\n", root->dir_name);
    
    return root;
}

trie_node* insert_node(trie_node* root, const char* path, FILE* fp) {
    /*
        @return trie_node*

        Given a path of type /dir1/dir2/..../dirn <- validation is done by the parser in this program

        , the function will first check if the path was already analysed:
            take /home/users/test for example

            and now we want to compute /home/users/test/first, this will return early because the value is already computed

            if any of the files change size, the trie won't update unless there is a funciton update_node or something (i haven't thought about it right now)
    */

    const char* delim = "/";
    char* temp_path = strdup(path);

    trie_node* currentNode = root;

    char* token = strtok(temp_path, delim);
    while(token != NULL) {
        int found = 0;
        for(int i = 0; i < currentNode->n_directories; ++i) {
            if(strcmp(currentNode->subdirectories[i]->dir_name, token) == 0) {
                currentNode = currentNode->subdirectories[i];
                found = 1;
                break;
            }
        }

        if(!found) {
            trie_node* new_node = create_node(token);
            currentNode->subdirectories[currentNode->n_directories++] = new_node;
            currentNode = new_node;
        } else if(currentNode->is_analysed == 1) {
            printf("\tPath is already being analysed.\n");
            return currentNode;
        }

        token = strtok(NULL, delim);
    }
    
    free(temp_path);

    trie_node* computed_root = insert_directory_rec(currentNode, path, fp);

    return computed_root;
}

void print(trie_node* root, int indent, FILE* fp) {
    for(int i = 0; i < indent; ++i)
        fprintf(fp, "    ");
    fprintf(fp, "%d. /%s  : %lld bytes %d files %d directories\n",
        indent, 
        root->dir_name,
        root->size,
        root->n_files,
        root->n_directories
    );

    for(int i = 0; i < root->n_directories; ++i)
        print(root->subdirectories[i], indent + 1, fp);
}

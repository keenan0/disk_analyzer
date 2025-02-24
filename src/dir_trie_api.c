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

trie_node* insert_directory_rec(trie_node* root, const char* path, FILE* fp, da_task* task_data) {
    /*
        @return trie_node* 

        Function computes the size of the tree that starts at root. Root has to exist. Any other node that will be traversed recursively will be created using the create_node() method. 

        This function is called by the insert_node() method once it passes the validation process.

        Caching is also supported: 
            if /home/user/test was computed
            and we want to now compute /home/user then the node from the trie will have all the necessary information in there and will return  
    */

    pthread_mutex_lock(&task_data->lock);

    if(task_data->is_deleted) {
        pthread_mutex_unlock(&task_data->lock);
        if(_DEBUG) 
            printf("Removed analysis task with ID '%d', status '%s' for '%s'’\n", task_data->id, task_data->status, path);
        pthread_exit(NULL);
    }

    while (task_data->paused) {
        if(_DEBUG) 
            printf("Task with ID '%d' is paused at %f%%.\n", task_data->id, task_data->progress);
        pthread_cond_wait(&task_data->cond, &task_data->lock);
    }

    pthread_mutex_unlock(&task_data->lock);

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

    //fprintf(fp, "Task %d: Added %s to the trie.\n", task_data->id, path);

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
            
            trie_node* cached = insert_directory_rec(new_node, full_path, fp, task_data);
            root->n_files += cached->n_files;
            root->size += cached->size;

            // Updating the task's progress
            task_data->processed_files++;
            if(found != -1) {
                if(_DEBUG) fprintf(fp,"\t\t\tCached %s with %d directories.\n", cached->dir_name, get_total_dirs(full_path));
                task_data->processed_files += get_total_dirs(full_path);
            }
            task_data->progress = (int)(((float)task_data->processed_files / (float)task_data->total_files) * 100);
            
            if(task_data->progress > 99) {task_data->status = "done";}

            //if(_DEBUG) fprintf(fp, "\tTask %d is %d%% done.\n", task_data->id, (int)task_data->progress);
        } else if(S_ISREG(file_metadata.st_mode)) {
            // The found entry is a file
            root->n_files++;
            root->size += file_metadata.st_size;
        }
    }

    closedir(dir);

    root->is_analysed = 1;
    //if(_DEBUG) fprintf(fp, "\t\t\tFinished analysing '%s'.\n", root->dir_name);
    
    return root;
}

trie_node* insert_node(trie_node* root, const char* path, FILE* fp, da_task* task_data) {
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
            //free(temp_path);
            return currentNode;
        }

        token = strtok(NULL, delim);
    }
    
    free(temp_path);

    trie_node* computed_root = insert_directory_rec(currentNode, path, fp, task_data);

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

int get_total_dirs(const char* path) {
    int total_directories = 0;
    
    DIR* dir = opendir(path);
    if(dir == NULL) {perror("Error opening directory.\n"); return -1;}

    char full_path[PATH_MAX];

    struct dirent* entry;
    while(entry = readdir(dir)) {
        if(strcmp(".", entry->d_name) == 0 
        || strcmp("..", entry->d_name) == 0) {
                continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat file_metadata;
        if(stat(full_path, &file_metadata) != 0) {
            perror("Error getting file or directory info.\n");
            return -1;
        }

        if(S_ISDIR(file_metadata.st_mode)) {
            total_directories += 1;
            total_directories += get_total_dirs(full_path);
        }
    }

    closedir(dir);
    return total_directories;
}

int get_total_files(const char* path) {
    int total_files = 0;
    
    DIR* dir = opendir(path);
    if(dir == NULL) {perror("Error opening directory.\n"); return -1;}

    char full_path[PATH_MAX];

    struct dirent* entry;
    while(entry = readdir(dir)) {
        if(strcmp(".", entry->d_name) == 0 
        || strcmp("..", entry->d_name) == 0) {
                continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat file_metadata;
        if(stat(full_path, &file_metadata) != 0) {
            perror("Error getting file or directory info.\n");
            return -1;
        }

        if(S_ISDIR(file_metadata.st_mode)) {
            total_files += get_total_files(full_path);
        }

        if(S_ISREG(file_metadata.st_mode)) {
            total_files += 1;
        }
    }

    closedir(dir);
    return total_files;
}

trie_node* find_node(trie_node* root, const char* path) {
    const char* delim = "/";
    char* temp_path = strdup(path);

    trie_node* currentNode = root;

    char* token = strtok(temp_path, delim);
    while(token != NULL) {
        int found = 0;
        for(int i = 0; i < currentNode->n_directories; ++i) {
            if(strcmp(token, currentNode->subdirectories[i]->dir_name) == 0) {
                currentNode = currentNode->subdirectories[i];
                found = 1;
                break;
            }
        }

        if(!found) {
            free(temp_path);
            return NULL;
        }

        token = strtok(NULL, delim);
    }

    free(temp_path);
    return currentNode;
}
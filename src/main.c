#include "../include/parser.h"
#include "../include/dir_trie_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <pthread.h>

#define TASK_CREATED_SUCCESS 100

#define MAX_TASKS 1000

typedef struct {
    pthread_t ALL_TASKS[MAX_TASKS];
    int n_tasks;
} task_manager;

task_manager tm;

cmd_option options[] = {
    {"-a", "--add"     , &cmd_add         },
    {"-p", "--priority", &cmd_set_priority},
    {"-S", "--suspend" , &cmd_suspend     },
    {"-R", "--resume"  , &cmd_resume      },
    {"-r", "--remove"  , &cmd_remove      },
    {"-i", "--info"    , &cmd_info        },
    {"-l", "--list"    , &cmd_list        },
    {"-P", "--print"   , &cmd_print       },
    {NULL, NULL        , NULL             }
};

void print_cmd(cmd_data data) {
    printf("CMD_DATA INFO:\n");
    printf("path: %s\n", data.path);
    printf("priority: %s\n", data.priority_name);
    printf("task_id: %d\n", data.task_id);
    printf("mask: %d\n", data.mask);
}

void* tm_create_task(void* args) {
    
    
    pthread_exit(NULL);
}

int da_create_task(cmd_data* data) {
    static int next_task_id = 1;
    data->task_id = next_task_id++;

    //pthread_create(&tm.ALL_TASKS[tm.n_tasks], NULL, &tm_create_task, NULL);

    printf("Created analysis task with ID '%d' for '%s' and priority '%s'.\n", data->task_id, data->path, data->priority_name);
    return TASK_CREATED_SUCCESS;
}

int valid_directory_path(const char* path) {
    char* pattern = "^\\/([a-zA-Z0-9._-]+\\/)*[a-zA-Z0-9_-]+\\/?$";

    regex_t reg;
    int ret;

    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if(ret) {
        perror("Could not compile regex.\n");
        return 0;
    }

    ret = regexec(&reg, path, 0, NULL, 0);
    regfree(&reg);

    return (ret == 0);
}

void analyze(cmd_data* data) {    
    if(!valid_directory_path(data->path)) {
        perror("Invalid path. Use /dir1/dir2/.../\n"); 
        return;
    }

    if(data->mask & ADD_MASK) {
        da_create_task(data);

    } else if(data->mask & REMOVE_MASK) {
        if(data->task_id == -1) { 
            perror("Usage: -r | --remove <id>\n"); 
            return;
        }
    }
}

int main() {
    //This is the starting node for our daemon. Any create_node() call will become a descendant of root.
    trie_node* root = create_node("/");

    FILE* fp = fopen("output.txt", "w");

    const char command[] = "da --add /home/test/";
    cmd_data parsed_line = parse(options, command);

    analyze(&parsed_line);
}
#include "../include/parser.h"
#include "../include/dir_trie_api.h"
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <regex.h>
#include <pthread.h>
#include <unistd.h>

#define TASK_CREATED_SUCCESS 100
#define TASK_CREATED_FAILURE 101

#define MAX_TASKS 1000

typedef struct {
    da_task ALL_TASKS[MAX_TASKS];
    int n_tasks;
} task_manager;

typedef struct {
    trie_node* root;
    cmd_data* data;
    FILE* fp;
    da_task* task_data;

    int task_index;
} tm_create_task_args;

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
    tm_create_task_args* arg = (tm_create_task_args*) args;
    if(_DEBUG) printf("\tCreated thread 'create_task'.\n");

    int tf = get_total_files(arg->data->path);;
    tm.ALL_TASKS[arg->task_index].total_files = tf;
    insert_node(arg->root, arg->data->path, arg->fp, arg->task_data);

    if(_DEBUG) printf("/!\\ Thread ended successfully.\n");
    pthread_exit(NULL);
}

int da_create_task(cmd_data* data, trie_node* root, FILE* fp) {
    static int next_task_id = 1;
    data->task_id = next_task_id++;

    // FIX LATER: Add mutex here
    tm_create_task_args* args = (tm_create_task_args*)(malloc(sizeof(tm_create_task_args)));
    if(!args) {perror("Malloc failed."); return 0;}

    tm.ALL_TASKS[tm.n_tasks].id = data->task_id;
    args->root = root;
    args->fp = fp;
    args->data = data;
    args->task_index = tm.n_tasks;
    args->task_data = &tm.ALL_TASKS[tm.n_tasks];

    pthread_attr_t attr;
    struct sched_param param;

    // Setting the thread's priority
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = data->priority + 1; // priorities are between 0 and 2 but the sched_priority only supports values above 0 so i add 1, it works the same
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    if(pthread_create(&tm.ALL_TASKS[tm.n_tasks].thread, &attr, &tm_create_task, (void*)args) != 0){
        perror("Could not create thread.\n"); 
        return TASK_CREATED_FAILURE;
    }

    pthread_detach(tm.ALL_TASKS[tm.n_tasks].thread);
    tm.n_tasks++;
    // End mutex here

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

void analyze(cmd_data* data, trie_node* root, FILE* fp) {    
    if(!valid_directory_path(data->path)) {
        perror("Invalid path. Use /dir1/dir2/.../\n"); 
        return;
    }

    if(data->mask & ADD_MASK) {
        if(da_create_task(data, root, fp) != TASK_CREATED_SUCCESS) {
            perror("Could not create task.\n");
            return;
        }

    } else if(data->mask & REMOVE_MASK) {
        if(data->task_id == -1) { 
            perror("Usage: -r | --remove <id>\n"); 
            return;
        }
    }
}

int main() {
    //This is the starting node for our daemon. Any create_node() call will become a descendant of root.
    trie_node* root = create_node("");

    FILE* fp = fopen("output.txt", "w");

    const char command[] = "da --add /home/claudiu/projects/da -p 0";
    cmd_data parsed_line = parse(options, command);

    const char command2[] = "da --add /home/claudiu/projects -p 1";
    cmd_data parsed_line2 = parse(options, command2);

    analyze(&parsed_line, root, fp);
    analyze(&parsed_line2, root, fp);
    sleep(10);

    print(root, 0, fp);
}
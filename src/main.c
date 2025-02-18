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

    int tf = get_total_dirs(arg->data->path);;
    tm.ALL_TASKS[arg->task_index].total_files = tf;
    tm.ALL_TASKS[arg->task_index].status = "progress";
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
    tm.ALL_TASKS[tm.n_tasks].paused = 0;
    tm.ALL_TASKS[tm.n_tasks].status = "pending";
    tm.ALL_TASKS[tm.n_tasks].path = strdup(data->path);
    tm.ALL_TASKS[tm.n_tasks].priority = data->priority;

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

    pthread_mutex_init(&tm.ALL_TASKS[tm.n_tasks].lock, NULL);
    pthread_cond_init(&tm.ALL_TASKS[tm.n_tasks].cond, NULL);

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

da_task* find_task(int id) {
    int index = find_task_index(id);

    if(index != -1) 
        return &tm.ALL_TASKS[index];

    return NULL;
}

int find_task_index(int id) {
    for(int i = 0; i < tm.n_tasks; ++i)
        if(tm.ALL_TASKS[i].id == id) 
            return i;
    
    return -1;
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

void analyze(cmd_data* data, trie_node* root, FILE* fp) {    
    if(data->mask & ADD_MASK) {
        if(!valid_directory_path(data->path)) {
            perror("Invalid path. Use /dir1/dir2/.../\n"); 
            return;
        }

        if(da_create_task(data, root, fp) != TASK_CREATED_SUCCESS) {
            perror("Could not create task.\n");
            return;
        }

    } else if(data->mask & SUSPEND_MASK) {
        if(data->task_id == -1) {
            perror("Usage: -s | --suspend <id>\n");
            return;
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            perror("Could not find task with given id.\n");
            return;
        }

        pthread_mutex_lock(&task->lock);
        task->paused = 1;
        pthread_mutex_unlock(&task->lock);
    } else if(data->mask & RESUME_MASK) {
        if(data->task_id == -1) {
            perror("Usage: -R | --resume <id>\n");
            return;
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            perror("Could not find task with given id.\n");
            return;
        }

        pthread_mutex_lock(&task->lock);
        task->paused = 0;
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
    } else if(data->mask & REMOVE_MASK) {
        if(data->task_id == -1) {
            perror("Usage: -r | --remove <id>\n");
            return;
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            perror("Could not find task with given id.\n");
            return;
        }

        pthread_mutex_lock(&task->lock);
        task->is_deleted = 1;
        pthread_mutex_unlock(&task->lock);
    } else if (data->mask & INFO_MASK) {
        if(data->task_id == -1) {
            perror("Usage: -i | --info <id>\n");
            return;
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            perror("Could not find task with given id.\n");
            return;
        }

        printf("Info: Task with ID '%d' has status '%s'.\n", task->id, task->status);
    } else if(data->mask & LIST_MASK) {
        printf("%-4s %-3s %-50s %-21s %-20s\n", 
            "ID",
            "PRI",
            "PATH",
            "STATUS",
            "DETAILS"
        );
        
        for(int i = 0; i < tm.n_tasks; ++i) {
            da_task* current = &tm.ALL_TASKS[i];
            
            if(current->is_deleted == 0) {
                printf("%-4d %-3s %-50s %-21s %-20s\n",
                    current->id,
                    format_priority(current->priority),
                    current->path,
                    format_progress(current->progress),
                    format_details(current->path)
                );
            }
        }
    } else if(data->mask & PRINT_MASK) {
        if(data->task_id == -1) {
            perror("Usage: -P | --print <id>\n");
            return;
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            perror("Could not find task with given id.\n");
            return;
        }

        if(strcmp(task->status, "done") != 0){
            perror("The task found is not done yet.\n");
            return;
        }


    }
}

int main() {
    //This is the starting node for our daemon. Any create_node() call will become a descendant of root.
    trie_node* root = create_node("");

    FILE* fp = fopen("output.txt", "w");

    const char command[] = "da --add /home/claudiu/projects/da -p 2";
    cmd_data parsed_line = parse(options, command);

    const char command2[] = "da -a /home/claudiu -p 0";
    cmd_data parsed_line2 = parse(options, command2);
    
    const char command3[] = "da -P 2";
    cmd_data parsed_line3 = parse(options, command3);

    analyze(&parsed_line, root, fp);
    analyze(&parsed_line2, root, fp);
    usleep(100000);
    analyze(&parsed_line3, root, fp);
    sleep(1);

    print(root, 0, fp);
}
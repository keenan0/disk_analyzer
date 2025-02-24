#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include "../include/parser.h"
#include "../include/dir_trie_api.h"
#include "../include/utils.h"
#include <errno.h>

#define SOCKET_PATH "/tmp/disk_analyzer.sock"

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
trie_node* root;
FILE* fp;

void* tm_create_task(void* args) {
    tm_create_task_args* arg = (tm_create_task_args*) args;
    //if(_DEBUG) printf("\tCreated thread 'create_task'.\n");
    syslog(LOG_INFO, "Inside the routine: %s", arg->task_data->path);

    int tf = get_total_dirs(arg->data->path);;
    tm.ALL_TASKS[arg->task_index].total_files = tf;
    tm.ALL_TASKS[arg->task_index].status = "progress";
    insert_node(arg->root, arg->data->path, arg->fp, arg->task_data);

    syslog(LOG_INFO, "Exiting the routine: %s", arg->task_data->path);

    //if(_DEBUG) printf("/!\\ Thread ended successfully.\n");
    pthread_exit(NULL);
}

int da_create_task(cmd_data* data, trie_node* root, FILE* fp, char* buffer) {
    static int next_task_id = 1;
    data->task_id = next_task_id++;

    // FIX LATER: Add mutex here
    tm_create_task_args* args = (tm_create_task_args*)(malloc(sizeof(tm_create_task_args)));
    if(!args) {snprintf(buffer, 4096, "Malloc failed."); return 0;}

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
    
    syslog(LOG_INFO, "Before creating thread: %s", data->path);
    if(pthread_create(&tm.ALL_TASKS[tm.n_tasks].thread, &attr, &tm_create_task, (void*)args) != 0) {
        snprintf(buffer, 4096, "Could not create thread: %s path: %s erro: %s.\n", strerror(errno), data->path); 
        return TASK_CREATED_FAILURE;
    }

    pthread_detach(tm.ALL_TASKS[tm.n_tasks].thread);
    tm.n_tasks++;
    // End mutex here

    snprintf(buffer, 4096, "Created analysis task with ID '%d' for '%s' and priority '%s'.\n", data->task_id, data->path, data->priority_name);
    return TASK_CREATED_SUCCESS;
}

int find_task_index(int id) {
    for(int i = 0; i < tm.n_tasks; ++i)
        if(tm.ALL_TASKS[i].id == id) 
            return i;
    
    return -1;
}

da_task* find_task(int id) {
    int index = find_task_index(id);

    if(index != -1) 
        return &tm.ALL_TASKS[index];

    return NULL;
}

void print_node_terminal(trie_node* root, const char* path, const char* full_path, int root_size, int is_in_root, char* buffer) {    
    float percentage = (float)root->size / root_size * 100;
    // A hashtag represents 2% 
    int hashtags = (int)(percentage / 2);

    snprintf(buffer + strlen(buffer), 4096 - strlen(buffer), "|-%-30s %5.1f%% %10s %-50s\n",
        (is_in_root ? full_path : path),
        percentage,
        format_bytes(root->size),
        format_hashtag(hashtags)
    ); 

    for(int i = 0; i < root->n_directories; ++i) {
        if(is_in_root) snprintf(buffer + strlen(buffer), 4096 - strlen(buffer), "|\n");
        
        char new_path[PATH_MAX];
        snprintf(new_path, PATH_MAX, "%s/%s", path, root->subdirectories[i]->dir_name);
        
        print_node_terminal(root->subdirectories[i], new_path, full_path, root_size, 0, buffer);
    }
}

char* analyze(cmd_data* data, trie_node* root, FILE* fp) {    
    char* buffer = (char*)malloc(4096 * sizeof(char));

    if(data->mask & ADD_MASK) {
        if(!valid_directory_path(data->path)) {
            return "Invalid path. Use /dir1/dir2/.../\n";
        }

        if(da_create_task(data, root, fp, buffer) != TASK_CREATED_SUCCESS) {
            return "Could not create task.\n";
        }
        
        return buffer;
    } else if(data->mask & SUSPEND_MASK) {
        if(data->task_id == -1) {
            return "Usage: -s | --suspend <id>\n";
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            return "Could not find task with given id.\n";
        }

        pthread_mutex_lock(&task->lock);
        task->paused = 1;
        pthread_mutex_unlock(&task->lock);
    } else if(data->mask & RESUME_MASK) {
        if(data->task_id == -1) {
            return "Usage: -R | --resume <id>\n";
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            return "Could not find task with given id.\n";
        }

        pthread_mutex_lock(&task->lock);
        task->paused = 0;
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
    } else if(data->mask & REMOVE_MASK) {
        if(data->task_id == -1) {
            return "Usage: -r | --remove <id>\n";
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            return "Could not find task with given id.\n";
        }

        pthread_mutex_lock(&task->lock);
        task->is_deleted = 1;
        pthread_mutex_unlock(&task->lock);
    } else if(data->mask & INFO_MASK) {
        if(data->task_id == -1) {
            return "Usage: -i | --info <id>\n";
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            return "Could not find task with given id.\n";
        }

        snprintf(buffer, 4096, "Info: Task with ID '%d' has status '%s'.\n", task->id, task->status);
        return buffer;
    } else if(data->mask & LIST_MASK) {
        snprintf(buffer, 4096, "%-4s %-3s %-50s %-21s %-20s\n", 
            "ID",
            "PRI",
            "PATH",
            "STATUS",
            "DETAILS"
        );
        
        for(int i = 0; i < tm.n_tasks; ++i) {
            da_task* current = &tm.ALL_TASKS[i];
            
            if(current->is_deleted == 0) {
                snprintf(buffer + strlen(buffer), 4096 - strlen(buffer), "%-4d %-3s %-50s %-21s %-20s\n",
                    current->id,
                    format_priority(current->priority),
                    current->path,
                    format_progress(current->progress),
                    format_details(current->path)
                );
            }
        }

        return buffer;
    } else if(data->mask & PRINT_MASK) {
        if(data->task_id == -1) {
            return "Usage: -P | --print <id>\n";
        }

        da_task* task = find_task(data->task_id);
        if(task == NULL) {
            return "Could not find task with given id.\n";
        }

        if(strcmp(task->status, "done") != 0){
            return "The task found is not done yet.\n";
        }

        snprintf(buffer, 4096, "%-30s %8s %10s %-50s\n", 
            "Path",
            "Usage",
            "Size",
            "Amount"
        );

        trie_node* path_node = find_node(root, task->path);
        if(path_node == NULL) {
            return "Given path is not valid.\n";
        }

        print_node_terminal(path_node, "", task->path, path_node->size, 1, buffer);

        return buffer;
    }

    return "Unknown option.\n";
}

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        syslog(LOG_INFO, "Daemon received SIGTERM, shutting down.");
        unlink(SOCKET_PATH);
        closelog();
        exit(0);
    }
}

void daemonize() {
    pid_t pid;

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void handle_command(int client_fd, cmd_data* cmd) {
    char* response = (char*)malloc(4096 * sizeof(char));

    response = analyze(cmd, root, fp);
    
    write(client_fd, response, strlen(response));
}

void start_server() {
    int server_fd, client_fd;
    struct sockaddr_un addr;

    static cmd_data buffer;
    char path_buffer[1024];
    char prio_buffer[256];

    unlink(SOCKET_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "Error creating socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (unlink(SOCKET_PATH) < 0 && errno != ENOENT) {
        syslog(LOG_ERR, "Failed to remove old socket");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Change permission to be able to run ./client option rather than sudo ./client option
    chmod(SOCKET_PATH, 0666);

    listen(server_fd, 5);
    syslog(LOG_INFO, "Daemon started, listening for commands...");

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
    
        memset(&buffer, 0, sizeof(cmd_data));
        ssize_t bytes_read = read(client_fd, &buffer, sizeof(cmd_data));
        if (bytes_read <= 0) {
            syslog(LOG_ERR, "Error reading from client or connection closed. bytes_read: %ld", bytes_read);
            close(client_fd);
            continue;
        }
        syslog(LOG_INFO, "Daemon received cmd_data.");
    
        // Receive path length
        size_t path_len;
        read(client_fd, &path_len, sizeof(size_t));
        if (path_len > 0) {
            read(client_fd, path_buffer, path_len);
            path_buffer[path_len - 1] = '\0'; // Ensure null termination
            buffer.path = path_buffer; // Assign received string
            syslog(LOG_INFO, "Daemon received path: %s.", buffer.path);
        } else {
            buffer.path = NULL;
        }
    
        // Receive priority_name length
        size_t prio_len;
        read(client_fd, &prio_len, sizeof(size_t));
        if (prio_len > 0) {
            read(client_fd, prio_buffer, prio_len);
            prio_buffer[prio_len - 1] = '\0'; // Ensure null termination
            buffer.priority_name = prio_buffer; // Assign received string
            syslog(LOG_INFO, "Daemon received priority: %s.", buffer.priority_name);
        } else {
            buffer.priority_name = NULL;
        }
    
        syslog(LOG_INFO, "Received: path=%s, mask=%d, priority=%d, task_id=%d, priority_name=%s",
            buffer.path, buffer.mask, buffer.priority, buffer.task_id, buffer.priority_name);
    
        handle_command(client_fd, &buffer);
        close(client_fd);
    }
}

int main() {
    root = create_node("");

    daemonize();
    openlog("disk_analyzer", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon initialized.");

    signal(SIGTERM, signal_handler);
    start_server();

    return 0;
}

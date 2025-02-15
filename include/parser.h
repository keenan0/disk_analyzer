#ifndef PARSER_H
#define PARSER_H

#define DAEMON_NAME "da"
#define MAX_TOKENS 100

#define ADD_MASK 1
#define SUSPEND_MASK 2
#define RESUME_MASK 4
#define REMOVE_MASK 8
#define LIST_MASK 16
#define PRINT_MASK 32
#define INFO_MASK 64

typedef struct {
    // A command has a mask with each bit representing an action
    // There are flags for:
    //      add     = 1
    //      suspend = 2
    //      resume  = 4
    //      remove  = 8
    //      list    = 16
    //      print   = 32
    //      info    = 64

    // The mask must be a power of two at all times

    int mask;
    const char* path;

    int priority;
    const char* priority_name;

    int task_id;
} cmd_data;

typedef struct {
    const char* short_cmd;
    const char* long_cmd;
    
    void (*handler)(cmd_data* data, const char* arg);
} cmd_option;

void cmd_add(cmd_data* data, const char* path);
void cmd_list(cmd_data* data, const char* arg);
void cmd_set_priority(cmd_data* data, const char* arg);
void cmd_suspend(cmd_data* data, const char* id);
void cmd_resume(cmd_data* data, const char* id);
void cmd_remove(cmd_data* data, const char* id);
void cmd_info(cmd_data* data, const char* id);
void cmd_print(cmd_data* data, const char* arg);

char** split(const char* line, const char* delim);
int search_option(cmd_option* options, const char* arg);

int is_power_of_two(int num);

cmd_data parse(cmd_option* options, const char* command);

#endif
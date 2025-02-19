#include "../include/parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void cmd_add(cmd_data* data, const char* path) {
    data->mask |= ADD_MASK;
    data->path = strdup(path);
}

void cmd_list(cmd_data* data, const char* arg) { data->mask |= LIST_MASK; }

void cmd_set_priority(cmd_data* data, const char* arg) {
    if(arg == NULL) return;
    int num_priority = atoi(arg);

    data->priority = num_priority;

    if(num_priority < 0 || num_priority > 3) return;
    switch (num_priority) {
    case 0:
        data->priority_name = "low";
        break;
    case 1:
        data->priority_name = "mid";
        break;
    case 2:
        data->priority_name = "high";
        break;
    default:
        data->priority = 0;
        data->priority_name = "low";
        break;
    }
}

void cmd_suspend(cmd_data* data, const char* id) {
    data->mask |= SUSPEND_MASK;
    
    if(id == NULL) {return;}

    int task_id = atoi(id);
    if(!task_id) { perror("failed atoi convert"); return;}

    data->task_id = task_id;
}

void cmd_resume(cmd_data* data, const char* id) {
    data->mask |= RESUME_MASK;
    
    if(id == NULL) {return;}
    
    int task_id = atoi(id);
    if(!task_id) { perror("failed atoi convert"); return; }

    data->task_id = task_id;
}

void cmd_remove(cmd_data* data, const char* id) {
    data->mask |= REMOVE_MASK;
    
    if(id == NULL) {return;}
    
    int task_id = atoi(id);
    if(!task_id) { perror("failed atoi convert"); return; }

    data->task_id = task_id;
};

void cmd_info(cmd_data* data, const char* id) {
    data->mask |= INFO_MASK;
    
    if(id == NULL) {return;}

    int task_id = atoi(id);
    if(!task_id) { perror("failed atoi convert"); return; }

    data->task_id = task_id;
}

void cmd_print(cmd_data* data, const char* id) { 
    data->mask |= PRINT_MASK; 

    if(id == NULL) {return;}

    int task_id = atoi(id);
    if(!task_id) { perror("failed atoi convert"); return;}

    data->task_id = task_id;
}
 
char** split(const char* line, const char* delim) {
    char** tokens = (char**)malloc(MAX_TOKENS * sizeof(char*));
    if(!tokens) {perror("malloc failed"); exit(EXIT_FAILURE);}

    char* line_copy = strdup(line);
    if(!line_copy) {perror("strdup failed"); exit(EXIT_FAILURE);}

    char* arg = strtok(line_copy, delim);
    int index = 0;

    while(arg != NULL && index < MAX_TOKENS - 1) {
        tokens[index] = strdup(arg);
        if(!tokens[index]) {perror("strdup failed"); exit(EXIT_FAILURE);}
        
        index++;
        arg = strtok(NULL, delim);
    }

    tokens[index] = NULL;
    free(line_copy);

    return tokens;
}

int search_option(cmd_option* options, const char* arg) {
    int index = 0;
    
    while(options[index].handler != NULL) {
        if(strcmp(options[index].short_cmd, arg) == 0 || strcmp(options[index].long_cmd, arg) == 0) {
            return index;
        }

        index++;
    }

    return index;
}

int is_power_of_two(int num) { return (num > 0) && ((num & (num - 1)) == 0); }

cmd_data parse(cmd_option* options, const char* command) {
    cmd_data result = {
        0,
        "/",
        0,
        "low",
        -1
    };

    char** args = split(command, " "); 

    void (*currentHandle)() = NULL;
    int index = 0;

    if(strcmp(args[index++], DAEMON_NAME) != 0) {exit(EXIT_FAILURE);}

    while(args[index] != NULL) {
        char* currentCommand = args[index];

        cmd_option found_option = options[search_option(options, currentCommand)];
        printf("searching for %s, found %d: %s\n", currentCommand, search_option(options, currentCommand), found_option.long_cmd);
        if(found_option.handler != NULL) {
            if(currentHandle != NULL) 
                currentHandle(&result, NULL);
            
            currentHandle = found_option.handler;
        } else if(currentHandle != NULL) {
            currentHandle(&result, currentCommand);
            currentHandle = NULL;
        } else {
            printf("unexpected value: %s\n", currentCommand);
        }
        index++;
    }

    if(currentHandle != NULL) {
        currentHandle(&result, NULL);
    }

    // Check if the result is valid
    if(is_power_of_two(result.mask) == 0) {
        result.mask = 0;
    }

    return result;
}
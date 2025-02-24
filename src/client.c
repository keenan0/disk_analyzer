#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../include/parser.h"

#define SOCKET_PATH "/tmp/disk_analyzer.sock"

int create_client_socket() {
    int client_socket;
    struct sockaddr_un server_addr;

    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to daemon");
        close(client_socket);
        exit(1);
    }

    return client_socket;
}

void send_command(int client_socket, cmd_data* command) {
    if (send(client_socket, command, sizeof(*command), 0) < 0) {
        perror("Failed to send command structure");
        close(client_socket);
        exit(1);
    }

    size_t path_len = (command->path != NULL) ? strlen(command->path) + 1 : 0;
    send(client_socket, &path_len, sizeof(size_t), 0);
    if (path_len > 0) {
        send(client_socket, command->path, path_len, 0);
    }

    size_t prio_len = (command->priority_name != NULL) ? strlen(command->priority_name) + 1 : 0;
    send(client_socket, &prio_len, sizeof(size_t), 0); 

    if (prio_len > 0) {
        send(client_socket, command->priority_name, prio_len, 0);
    }
}

void receive_response(int client_socket) {
    char response[4096];
    int bytes = recv(client_socket, response, sizeof(response) - 1, 0);
    if (bytes > 0) {
        response[bytes] = '\0';
        printf("%s\n", response);
    } else {
        printf("No response from daemon.\n");
    }
    close(client_socket);
}

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: da <command>\n");
        return 1;
    }

    static char command_line[4096] = {0};
    for (int i = 1; i < argc; i++) {
        strcat(command_line, argv[i]);
        strcat(command_line, " ");
    }

    static cmd_data command;
    command = parse(options, command_line);
    //printf("%s %d %d %d %s\n", command.path, command.mask, command.priority, command.task_id, command.priority_name);

    int client_socket = create_client_socket();
    send_command(client_socket, &command);
    receive_response(client_socket);

    return 0;
}

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/un.h>
#include <stdbool.h>

#define PACKET_SIZE 1024
const int SOCKET_PORT = 1234;
const char *SOCKET_FILE = "socket";

struct sockaddr_in serv_in_addr;
struct sockaddr_un serv_un_addr;

int create_socket(int socket_type) {
    int socket_file_descriptor = socket(socket_type, SOCK_STREAM, 0);
    if (socket_file_descriptor == -1) {
        printf("Error during creating socket\n");
        exit(-1);
    }
    return socket_file_descriptor;
}

void set_socket_blocking_mode(int socket_file_descriptor, bool is_blocking) {
    if (!is_blocking)
        fcntl(socket_file_descriptor, F_SETFL, O_NONBLOCK);
}

void init_unix_address(struct sockaddr_un *serv_addr) {
    serv_addr->sun_family = AF_UNIX;
    strcpy(serv_addr->sun_path, SOCKET_FILE);
    unlink(SOCKET_FILE);
}

void listen_socket(int socket_fd) {
    if (listen(socket_fd, 10) < 0) {
        printf("Listen failed\n");
        exit(-1);
    } else {
        printf("Listening ...\n");
    }
}

void bind_unix_socket(int socket_fd, struct sockaddr_un *serv_addr) {
    if (bind(socket_fd, (struct sockaddr *) serv_addr,
             strlen(serv_addr->sun_path) + sizeof(serv_addr->sun_family)) < 0) {
        printf("Binding failed\n");
        exit(-1);
    } else {
        printf("Binding successful!\n");
    }
}

int open_unix(bool is_blocking, struct sockaddr_un *serv_addr) {
    int socket_file_descriptor = create_socket(AF_UNIX);
    set_socket_blocking_mode(socket_file_descriptor, is_blocking);
    init_unix_address(serv_addr);
    bind_unix_socket(socket_file_descriptor, serv_addr);
    listen_socket(socket_file_descriptor);
    return socket_file_descriptor;
}

void init_inet_address(struct sockaddr_in *serv_addr) {
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr->sin_port = htons(SOCKET_PORT);
}

void bind_inet_socket(int socket_fd, struct sockaddr_in *serv_addr) {
    if (bind(socket_fd, (struct sockaddr *) serv_addr, sizeof(*serv_addr)) < 0) {
        printf("Binding failed\n");
        exit(-1);
    } else {
        printf("Binding successful!\n");
    }
}

int open_inet(bool is_blocking, struct sockaddr_in *serv_addr) {
    int socket_file_descriptor = create_socket(AF_INET);
    set_socket_blocking_mode(socket_file_descriptor, is_blocking);
    init_inet_address(serv_addr);
    bind_inet_socket(socket_file_descriptor, serv_addr);
    listen_socket(socket_file_descriptor);
    return socket_file_descriptor;
}

int open_socket(const char *type_of_socket, bool is_blocking, struct sockaddr *serv_addr) {
    if (strcmp(type_of_socket, "UNIX") == 0)
        return open_unix(is_blocking, &serv_un_addr);
    else if (strcmp(type_of_socket, "INET") == 0)
        return open_inet(is_blocking, &serv_in_addr);

    return -1;
}

int main(int argc, char *argv[]) {
    char type_of_socket[] = "UNIX";
    bool is_blocking = true;

    char data_buffer[PACKET_SIZE];

    struct sockaddr *serv_addr = NULL;

    printf("Opening socket ...\n");
    int socket_file_descriptor = open_socket(type_of_socket, is_blocking, serv_addr);
    while (true) {
        const int new_socket = accept(socket_file_descriptor, serv_addr, (socklen_t *) sizeof(*serv_addr));
        if (new_socket == -1) {
            if (is_blocking == true) {
                printf("Error during accept new socket\n");
                exit(-1);
            } else {
                printf("Waiting for the connection ...\n");
                sleep(1);
            }
            continue;
        }
        printf("New connection!\n");
        uint64_t total = 0;
        while (true) {
            const int received_data_len = read(new_socket, data_buffer, sizeof(data_buffer));
            //printf(data_buffer);
            total += received_data_len;
            if (received_data_len == 0) {
                printf("Received empty packet\n");
                break;
            } else {
                printf("Received data_buffer with packet size = %d\n", received_data_len);
            }
        }
        printf("Total received bytes from socket = %lu\n", total);
        close(new_socket);
    }
    return 0;
}
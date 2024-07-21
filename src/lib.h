#ifndef LIB_H_
#define LIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DEFAULT_PORT 6969
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

struct client_info {
    int sock;
    struct sockaddr_in address;
    bool active;
};

struct client_info clients[MAX_CLIENTS];
bool server_running = false;
char *ip = NULL;
int port = DEFAULT_PORT;

void parse_ip_port(char *arg, char **ip, int *port) {
    *ip = strtok(arg, ":");
    char *port_str = strtok(NULL, ":");
    *port = (port_str != NULL) ? atoi(port_str) : DEFAULT_PORT;
}

void print_ip() {
    FILE *fp;
    char public_ip[BUFFER_SIZE] = {0};
    if ((fp = popen("curl -s ifconfig.me", "r")) == NULL) {
        perror("popen");
        return;
    }
    if (fgets(public_ip, sizeof(public_ip), fp) == NULL) {
        perror("fgets");
        pclose(fp);
        return;
    }
    pclose(fp);
    public_ip[strcspn(public_ip, "\n")] = '\0';
    int sock;
    struct sockaddr_in serv = { .sin_family = AF_INET, .sin_port = htons(80) };
    char local_ip[BUFFER_SIZE] = {0};
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return;
    }
    inet_pton(AF_INET, "8.8.8.8", &serv.sin_addr);
    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sock);
        return;
    }
    if (getsockname(sock, (struct sockaddr *)&name, &namelen) < 0) {
        perror("getsockname");
        close(sock);
        return;
    }
    close(sock);
    if (inet_ntop(AF_INET, &name.sin_addr, local_ip, sizeof(local_ip)) == NULL) {
        perror("inet_ntop");
        return;
    }
    printf("Public IP: %s, Local IP: %s -> ./chat -c %s:%d\n", public_ip, local_ip, local_ip, DEFAULT_PORT);
}

int check_server_running(int port) {
    int sock;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 0;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return 0;
    }
    close(sock);
    return 1;
}

void send_to_all_clients(char *message, int sender_sock) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && clients[i].sock != sender_sock) {
            send(clients[i].sock, message, strlen(message), 0);
        }
    }
}

void cleanup_clients() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active) {
            close(clients[i].sock);
            if (server_running) {
                printf("\x1b[A\nClient %s:%d Disconnected.\nServer❯ ", inet_ntoa(clients[i].address.sin_addr), ntohs(clients[i].address.sin_port));
                fflush(stdout);
            }
        }
    }
}

void *receive_messages(void *arg) {
    struct client_info *client = (struct client_info *)arg;
    int sock = client->sock;
    struct sockaddr_in addr = client->address;
    char buffer[BUFFER_SIZE];
    int valread;
    bool connected = false;
    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0) {
        if (!connected) {
            if (server_running)
                printf("\x1b[A\nClient %s:%d Connected.\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            connected = true;
        }
        if (server_running) {
            printf("\x1b[2K\r%s\nServer❯ ", buffer);
            fflush(stdout);
        } else {
            printf("\x1b[2K\r%s\n%s❯ ", buffer, getlogin());
            fflush(stdout);
        }
        send_to_all_clients(buffer, sock);
        memset(buffer, 0, BUFFER_SIZE);
    }
    if (valread == 0 || !server_running) {
        if (connected && server_running) {
            printf("\x1b[A\nClient %s:%d Disconnected.\nServer❯ ", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            fflush(stdout);
        }
    } else if (valread == -1 && errno != EINTR) {
        perror("Error reading from socket");
    }
    client->active = false;
    close(sock);
    return NULL;
}

int localcmd(char *buffer, int sock) {
    if (strcmp(buffer, "exit") == 0) {
        printf("Exiting...\n");
        if (sock > 0) {
            close(sock);
        }
        exit(0);
    } else if (strcmp(buffer, "ip") == 0) {
        print_ip();
    } else if (strcmp(buffer, "help") == 0) {
        printf("Available commands:\n  help      - Display this help message\n  exit      - Disconnect and exit the program\n  ip        - Shows your ip and command to connect if is a server\n");
    } else {
        return 0;
    }
    return 1;
}

void *server_input(void *arg) {
    char *buffer;
    while (true) {
        buffer = readline("Server❯ ");
        if (buffer == NULL)
            break;
        if (*buffer) { 
            add_history(buffer);
            if (localcmd(buffer, -1) == 0) {
                char prefixed_message[BUFFER_SIZE + 50];
                snprintf(prefixed_message, sizeof(prefixed_message), "Server❯ %s", buffer);
                send_to_all_clients(prefixed_message, -1);
            }
        }
        free(buffer);
    }
    return NULL;
}

void client_input(int sock) {
    char *buffer;
    char *login_name = getlogin();
    char name[256];
    if (login_name) {
        sprintf(name, "%s❯ ", login_name);
    } else {
        sprintf(name, "Client❯ ");
    }
    while (true) {
        buffer = readline(name);
        if (buffer == NULL)
            break;
        if (*buffer) {
            add_history(buffer);
            if (localcmd(buffer, sock) == 0) {
                char prefixed_message[BUFFER_SIZE + 50];
                snprintf(prefixed_message, sizeof(prefixed_message), "%s%s", name, buffer);
                send(sock, prefixed_message, strlen(prefixed_message), 0);
            }
        }
        free(buffer);
    }
}

void run_server(const char *ip, int port) {
    struct sockaddr_in address;
    int server_fd, new_socket;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    print_ip();
    printf("Server listening on port %d\n", port);
    server_running = true;
    pthread_t input_thread_id;
    if (pthread_create(&input_thread_id, NULL, server_input, NULL) != 0) {
        perror("could not create input thread");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept failed");
            cleanup_clients();
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        int i;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i].active) {
                clients[i].sock = new_socket;
                clients[i].address = address;
                clients[i].active = true;
                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, receive_messages, (void *)&clients[i]) != 0) {
                    perror("could not create thread");
                    close(new_socket);
                    cleanup_clients();
                    close(server_fd);
                    exit(EXIT_FAILURE);
                }
                pthread_detach(thread_id);
                break;
            }
        }
        if (i == MAX_CLIENTS) {
            printf("Too many clients. Connection rejected.\n");
            close(new_socket);
        }
    }
    close(server_fd);
    server_running = false;
    cleanup_clients();
}

void run_client(char *ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to %s:%d.\n", ip, port);
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receive_messages, (void *)&sock) < 0) {
        perror("could not create thread");
        close(sock);
        exit(EXIT_FAILURE);
    }
    client_input(sock);
    close(sock);
}

#endif /* LIB_H_ */

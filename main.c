#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define DEFAULT_PORT 6969
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

struct client_info {
    int sock;
    struct sockaddr_in address;
    bool active;
};

struct client_info clients[MAX_CLIENTS];
bool server_running = 0;

void *receive_messages(void *arg) {
    struct client_info *client = (struct client_info *)arg;
    int sock = client->sock;
    struct sockaddr_in addr = client->address;
    char buffer[BUFFER_SIZE];
    int valread;
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s", buffer);
            char message[BUFFER_SIZE + NAME_MAX + 10];
            char username[NAME_MAX + 1];
            if (getlogin_r(username, NAME_MAX) == 0) {
                sprintf(message, "\n%s:%d@%s> %s", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), username, buffer);
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i].active && clients[i].sock != sock) {
                        send(clients[i].sock, message, strlen(message), 0);
                    }
                }
            } else {
                perror("Failed to get username");
            }
        } else if (valread == 0) {
            printf("Client %s:%d disconnected.\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            break;
        } else {
            perror("Error reading from socket");
            break;
        }
    }
    // Mark client as inactive
    client->active = 0;
    close(sock);
    return NULL;
}

void send_message_to_client(int client_index, const char *message) {
    if (client_index >= 0 && client_index < MAX_CLIENTS && clients[client_index].active) {
        if (strcmp(message, "/ip")) {
            
        }
        send(clients[client_index].sock, message, strlen(message), 0);
    } else {
        printf("Client index %d is not valid or inactive.\n", client_index);
    }
}

char *ip() {
  struct ifaddrs *ifaddr, *ifa;
  int family, s;
  char host[NI_MAXHOST];
  static char ip_address[NI_MAXHOST];

  if (getifaddrs(&ifaddr) == -1) {
    return NULL;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;

    family = ifa->ifa_addr->sa_family;

    if (family == AF_INET || family == AF_INET6) {
      s = getnameinfo(ifa->ifa_addr,
                      (family == AF_INET) ? sizeof(struct sockaddr_in)
                                          : sizeof(struct sockaddr_in6),
                      host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (s != 0) {
        freeifaddrs(ifaddr);
        return NULL;
      }

      if (strcmp(ifa->ifa_name, "lo") != 0) {
        strncpy(ip_address, host, NI_MAXHOST);
        freeifaddrs(ifaddr);
        return ip_address;
      }
    }
  }

  freeifaddrs(ifaddr);
  return NULL;
}

void print_public_ip() {
    char command[BUFFER_SIZE] = "curl -s ifconfig.me";
    FILE *fp;
    char result[BUFFER_SIZE] = {0};
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        exit(EXIT_FAILURE);
    }
    if (fgets(result, sizeof(result), fp) != NULL) {
        printf("Public IP: %s To connect, run: ./chat -c %s:%d\n", result, result, DEFAULT_PORT);
    }
    pclose(fp);
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

void run_server(const char *ip_address, int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    if (ip_address == NULL) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip_address, &address.sin_addr) <= 0) {
            perror("Invalid IP address");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
    }
    address.sin_port = htons(port);
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    print_public_ip();
    printf("Server listening on port %d\n", port);
    server_running = 1;
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        int i;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i].active) {
                clients[i].sock = new_socket;
                clients[i].address = address;
                clients[i].active = 1;

                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, receive_messages, (void *)&clients[i]) != 0) {
                    perror("could not create thread");
                    close(new_socket);
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
}

void run_client(char *server_ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server at %s:%d.\n", server_ip, port);
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receive_messages, (void *)&sock) < 0) {
        perror("could not create thread");
        close(sock);
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    while (1) {
        printf("You> ");
        fflush(stdout); // Ensure prompt is displayed immediately
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            send(sock, buffer, strlen(buffer), 0);
        } else {
            perror("Error reading input");
            break;
        }
    }
    close(sock);
}

int main(int argc, char *argv[]) {

  char *ip_addr = ip();
  printf("IP: %s\n", ip_addr);

  if (argc == 1) {
    if (check_server_running(DEFAULT_PORT)) {
      run_client("127.0.0.1", DEFAULT_PORT);
    } else {
      run_server(NULL, DEFAULT_PORT);
    }
    } else if (argc == 3 && strcmp(argv[1], "-s") == 0) {
        char *ip_port = argv[2];
        char *ip_address = strtok(ip_port, ":");
        int port = atoi(strtok(NULL, ":"));
        run_server(ip_address, port);
    } else if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        char *server_ip = strtok(argv[2], ":");
        int port = atoi(strtok(NULL, ":"));
        run_client(server_ip, port);
    } else if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage:\n");
        printf("  %s             Start the server or connect to the locally running server\n", argv[0]);
        printf("  %s -s <ip>:<port> Start the server with custom IP and port\n", argv[0]);
        printf("  %s -c <ip>:<port> Connect to the server at the specified IP address and port\n", argv[0]);
    }
    return 0;
}

#include "lib.h"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        if (!check_server_running(DEFAULT_PORT)) {
            run_server("127.0.0.1", DEFAULT_PORT);
        } else {
            run_client("127.0.0.1", DEFAULT_PORT);
        }
    } else if (argc == 3) {
        parse_ip_port(argv[2], &ip, &port);
        if (strcmp(argv[1], "-s") == 0) {
            run_server(ip, port);
        } else if (strcmp(argv[1], "-c") == 0) {
            run_client(ip, port);
        }
    } else if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage:\n  %s             Start the server or connect to the locally running server\n  %s -s <ip>:<port> Start the server with custom IP and port\n  %s -c <ip>:<port> Connect to the server at the specified IP address and port\n", argv[0], argv[0], argv[0]);
    } else {
        fprintf(stderr, "Invalid arguments. Use '%s -h' for help.\n", argv[0]);
        return EXIT_FAILURE;
    }
    return 0;
}

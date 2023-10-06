#include "client.h"
typedef enum {
    STATE_PARSE_ARGUMENTS,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_CREATE_SOCKET,
    STATE_CONNECT_SOCKET,
    STATE_SEND_FILES,
    STATE_CLEANUP,
    STATE_ERROR
} ClientState;

typedef struct {
    ClientState error_state;         // The state in which the error occurred
    ClientState next_intended_state; // The next state we intended to move to
    const char* error_message;       // A description of the error
} ErrorContext;

ErrorContext error_context; // Global error context
int main(int argc, char *argv[]) {
    ClientState current_state = STATE_PARSE_ARGUMENTS;
    int success = 1;  // A flag to indicate if the state processing was successful.

    char *address;
    char *port_str;
    in_port_t port;
    struct sockaddr_storage addr;
    int sockfd;
    char **file_paths;
    int num_files;

    while (success) {
        switch (current_state) {
            case STATE_PARSE_ARGUMENTS:
                if (parse_arguments(argc, argv, &address, &port_str, &file_paths, &num_files) == -1) {
                    current_state = STATE_ERROR;
                } else {
                    current_state = STATE_HANDLE_ARGUMENTS;
                }
                break;

            case STATE_HANDLE_ARGUMENTS:
                if (handle_arguments(argv[0], address, port_str, &port) == -1) {
                    current_state = STATE_ERROR;
                } else {
                    current_state = STATE_CONVERT_ADDRESS;
                }
                break;

            case STATE_CONVERT_ADDRESS:
                convert_address(address, &addr);
                current_state = STATE_CREATE_SOCKET;
                break;

            case STATE_CREATE_SOCKET:
                sockfd = socket_create(addr.ss_family, SOCK_STREAM, 0);
                if (sockfd == -1) {
                    current_state = STATE_ERROR;
                } else {
                    current_state = STATE_CONNECT_SOCKET;
                }
                break;

            case STATE_CONNECT_SOCKET:
                socket_connect(sockfd, &addr, port);
                current_state = STATE_SEND_FILES;
                break;

            case STATE_SEND_FILES:
                for (int i = 0; i < num_files; ++i) {
                    send_file(sockfd, file_paths[i]);
                }
                current_state = STATE_CLEANUP;
                break;

            case STATE_CLEANUP:
                socket_close(sockfd);
                success = 0;  // End the loop.
                break;

            case STATE_ERROR:
                fprintf(stderr, "Error detected in state: %d\n", error_context.error_state);
                fprintf(stderr, "Intended to move to state: %d\n", error_context.next_intended_state);
                fprintf(stderr, "Reason: %s\n", error_context.error_message);
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}



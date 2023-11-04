//
// Created by Sami Roudgarian on 2023-10-04.
//

#ifndef SOCKET_FSM_SERVER_H
#define SOCKET_FSM_SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/poll.h>

int setup_signal_handler(void* ctx);
void sigint_handler(int signum);
int parse_arguments(int argc, char *argv[], char **ip_address, char **port, char **directory, void* ctx);
int handle_arguments(const char *binary_name, const char *ip_address, const char *port_str, in_port_t *port, void* ctx);
int parse_in_port_t(const char *binary_name, const char *port_str, in_port_t *parsed_value, void* ctx);
void usage(const char *program_name, const char *message);
int convert_address(const char *address, struct sockaddr_storage *addr, void* ctx);
int socket_create(int domain, int type, int protocol, void* ctx);
int socket_bind(int sockfd, struct sockaddr_storage *addr, in_port_t port, void* ctx);
int start_listening(int server_fd, int backlog, void* ctx);
int handle_new_client(int server_socket, int **client_sockets, nfds_t *max_clients, void* ctx);
int socket_close(int sockfd, void* ctx);
int handle_disconnection(int sd, int **client_sockets, const nfds_t *max_clients, int client, void* ctx);
int receive_files(int sd, int **client_sockets, const nfds_t *max_clients, const char *dir, int *client, void* ctx);
int read_buffer_size(int sockfd, uint32_t size, void *buffer, void* ctx);
int setup_server_socket(int sockfd, void* ctx);
void setup_fds(struct pollfd *fds, int *client_sockets, nfds_t max_clients, int sockfd, int *client);
int handle_clients(struct pollfd *fds, nfds_t max_clients, int *client_sockets, char *directory, int *client, void* ctx);
int cleanup_server(int *client_sockets, nfds_t max_clients, struct pollfd *fds, int sockfd, void* ctx);

#define UNKNOWN_OPTION_MESSAGE_LEN 24
#define BASE_TEN 10

// Helper macros
typedef enum {
    STATE_PARSE_ARGUMENTS,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_SOCKET_CREATE,
    STATE_SETUP_SERVER_SOCKET,
    STATE_SOCKET_BIND,
    STATE_START_LISTENING,
    STATE_SETUP_SIGNAL_HANDLER,
    STATE_POLL,
    STATE_HANDLE_NEW_CLIENT,
    STATE_HANDLE_CLIENTS,
    STATE_CLEANUP,
    STATE_ERROR,
    STATE_EXIT // useful to have an explicit exit state
} server_state;


typedef struct {
    server_state state;
    server_state (*state_handler)(void* context);
    server_state next_states[2];  // 0 for success, 1 for failure
} FSMState;
typedef server_state (*StateHandlerFunc)(void* context);

#define BASE_FILENAME(file) ({ \
    const char *slash = strrchr(file, '/'); \
    slash ? slash + 1 : file; \
})

typedef struct {
    int argc;
    char **argv;
    char                    *address;
    char                    *port_str;
    char                    *directory;
    in_port_t               port;
    int                     *client_sockets;
    nfds_t                  max_clients;
    int                     num_ready;
    int                     sockfd;
    int                     enable;
    struct sockaddr_storage addr;
    struct pollfd *fds;
    int  client[1024];
    char *trace_message;
    server_state trace_state;
    int trace_line;
    char    *error_message;
    const char    *function_name;
    const char    *file_name;
    int     error_line;
} FSMContext;
#define SET_ERROR(ctx, msg) \
    do { \
        ctx -> error_message = msg; \
        ctx -> function_name = __func__; \
        ctx -> file_name = BASE_FILENAME(__FILE__);  \
        ctx -> error_line = __LINE__; \
    } while (0)

#define SET_TRACE(ctx, msg, curr_state) \
    do { \
        ctx->trace_message = msg; \
        ctx->trace_state = curr_state; \
        ctx->trace_line = __LINE__; \
        printf("TRACE: %s \nEntered state %s (%d) at line %d.\n\n", \
               ctx->trace_message, state_to_string(ctx->trace_state),ctx->trace_state, ctx->trace_line); \
    } while (0)


extern volatile int exit_flag;
#endif //SOCKET_FSM_SERVER_H

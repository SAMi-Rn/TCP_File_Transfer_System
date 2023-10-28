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

int setup_signal_handler(void);
void sigint_handler(int signum);
int parse_arguments(int argc, char *argv[], char **ip_address, char **port, char **directory);
int handle_arguments(const char *binary_name, const char *ip_address, const char *port_str, in_port_t *port);
int parse_in_port_t(const char *binary_name, const char *port_str, in_port_t *parsed_value);
_Noreturn static void usage(const char *program_name, int exit_code, const char *message);
int convert_address(const char *address, struct sockaddr_storage *addr);
int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, struct sockaddr_storage *addr, in_port_t port);
int start_listening(int server_fd, int backlog);
int handle_new_client(int server_socket, int **client_sockets, nfds_t *max_clients);
int socket_close(int sockfd);
int handle_disconnection(int sd, int **client_sockets, const nfds_t *max_clients, int client);
int receive_files(int sd, int **client_sockets, const nfds_t *max_clients, const char *dir, int *client);
int read_buffer_size(int sockfd, uint32_t size, void *buffer);
int setup_server_socket(int sockfd, struct sockaddr_storage *addr, in_port_t port);
void setup_fds(struct pollfd *fds, int *client_sockets, nfds_t max_clients, int sockfd, int *client);
int handle_clients(struct pollfd *fds, nfds_t max_clients, int *client_sockets, char *directory, int *client);
int cleanup_server(int *client_sockets, nfds_t max_clients, struct pollfd *fds, int sockfd);
#define UNKNOWN_OPTION_MESSAGE_LEN 24
#define BASE_TEN 10
// Helper macros
#define SET_ERROR(ctx, msg, from_state, to_state) \
    do { \
        ctx->error_message = msg; \
        ctx->error_from_state = from_state; \
        ctx->error_to_state = to_state; \
        ctx->error_line = __LINE__; \
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

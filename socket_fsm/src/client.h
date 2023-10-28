//
// Created by Sami Roudgarian on 2023-10-04.
//

#ifndef SOCKET_FSM_CLIENT_H
#define SOCKET_FSM_CLIENT_H

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int parse_arguments(int argc, char *argv[], char **address, char **port, char ***file_paths, int *num_files);
int handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port);
int parse_in_port_t(const char *binary_name, const char *str, in_port_t *parsed_value);
_Noreturn static void usage(const char *program_name, int exit_code, const char *message);
int convert_address(const char *address, struct sockaddr_storage *addr);
int socket_create(int domain, int type, int protocol);
int socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port);
int socket_close(int sockfd);
int send_file(int sockfd, const char *file_path);


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
#endif //SOCKET_FSM_CLIENT_H

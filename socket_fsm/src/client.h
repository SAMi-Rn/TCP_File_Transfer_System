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
#include <glob.h>

int parse_arguments(int argc, char *argv[], char **address, char **port, char ***file_paths, int *num_files, void* ctx);
int handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port, void* ctx);
int parse_in_port_t(const char *binary_name, const char *str, in_port_t *parsed_value, void* ctx);
void usage(const char *program_name,const char *message);
int convert_address(const char *address, struct sockaddr_storage *addr, void* ctx);
int socket_create(int domain, int type, int protocol, void* ctx);
int socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port, void* ctx);
int socket_close(int sockfd, void* ctx);
int send_file(int sockfd, const char *file_path, void* ctx);


#define UNKNOWN_OPTION_MESSAGE_LEN 24
#define BASE_TEN 10
typedef enum {
    STATE_PARSE_ARGUMENTS,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_SOCKET_CREATE,
    STATE_SOCKET_CONNECT,
    STATE_SEND_FILE,
    STATE_CLEANUP,
    STATE_ERROR,
    STATE_EXIT,
} client_state;

typedef struct {
    client_state state;
    client_state (*state_handler)(void* context);
    client_state next_states[2];  // 0 for success, 1 for failure
} FSMState;

typedef client_state (*StateHandlerFunc)(void* context);

#define BASE_FILENAME(file) ({ \
    const char *slash = strrchr(file, '/'); \
    slash ? slash + 1 : file; \
})

typedef struct {
    int argc;
    char **argv;
    char *address;
    char *port_str;
    in_port_t port;
    struct sockaddr_storage addr;
    int sockfd;
    char **file_paths;
    int num_files;
    int current_file_index;
    char *trace_message;
    client_state trace_state;
    int trace_line;
    char    *error_message;
    const char    *function_name;
    const char    *file_name;
    int     error_line;
} FSMContext;

// Helper macros
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
#endif //SOCKET_FSM_CLIENT_H

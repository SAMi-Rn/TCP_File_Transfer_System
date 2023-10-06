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


#endif //SOCKET_FSM_CLIENT_H

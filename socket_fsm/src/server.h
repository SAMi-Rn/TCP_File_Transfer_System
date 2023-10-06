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

static void setup_signal_handler(void);
static void sigint_handler(int signum);
static void parse_arguments(int argc, char *argv[], char **ip_address, char **port, char **directory);
static void handle_arguments(const char *binary_name, const char *ip_address, const char *port_str, in_port_t *port);
static in_port_t parse_in_port_t(const char *binary_name, const char *port_str);
_Noreturn static void usage(const char *program_name, int exit_code, const char *message);
static void convert_address(const char *address, struct sockaddr_storage *addr);
static int socket_create(int domain, int type, int protocol);
static void socket_bind(int sockfd, struct sockaddr_storage *addr, in_port_t port);
static void start_listening(int server_fd, int backlog);
static void handle_new_client(int server_socket, int **client_sockets, nfds_t *max_clients);
static void socket_close(int sockfd);
static void handle_disconnection(int sd, int **client_sockets, const nfds_t *max_clients, int client);
static void receive_files(int sd, int **client_sockets, const nfds_t *max_clients, const char *dir, int *client);
#define UNKNOWN_OPTION_MESSAGE_LEN 24
#define BASE_TEN 10
#endif //SOCKET_FSM_SERVER_H

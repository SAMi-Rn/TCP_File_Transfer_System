#include "server.h"


static volatile sig_atomic_t exit_flag = 0;


int main(int argc, char *argv[])
{
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

    address  = NULL;
    port_str = NULL;
    client_sockets = NULL;
    max_clients    = 0;
    fds            = NULL;
    int  client[1024];
    parse_arguments(argc, argv, &address, &port_str,&directory);
    handle_arguments(argv[0], address, port_str, &port);
    convert_address(address, &addr);
    sockfd = socket_create(addr.ss_family, SOCK_STREAM, 0);
    enable = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    socket_bind(sockfd, &addr, port);
    start_listening(sockfd, SOMAXCONN);
    setup_signal_handler();

    while(!exit_flag)
    {
        struct pollfd *temp_fds;
        temp_fds = (struct pollfd *)realloc(fds, (max_clients + 1) * sizeof(struct pollfd));
        if(temp_fds == NULL)
        {
            perror("Realloc error");
            free(fds);
            exit(EXIT_FAILURE);
        }
        fds = temp_fds;
        // Set up the pollfd structure for the server socket
        fds[0].fd     = sockfd;
        fds[0].events = POLLIN;

        // Set up the pollfd structures for all client sockets
        for(uint32_t i = 0; i < max_clients; i++)
        {
            int sd;

            sd                = client_sockets[i];
            client[sd]        = (int)i+1;
            fds[i + 1].fd     = sd;
            fds[i + 1].events = POLLIN;
        }
        // Use poll to monitor all sockets for activity
        num_ready = poll(fds, max_clients + 1, -1);
        if(num_ready < 0)
        {
            if(errno == EINTR)
            {
                // The poll call was interrupted by a signal (e.g., SIGINT)
                // Continue the loop and retry the poll call
                continue;
            }

            perror("Poll error");
            exit(EXIT_FAILURE);
        }
        // Handle new client connections
        if(fds[0].revents & POLLIN)
        {
            handle_new_client(sockfd, &client_sockets, &max_clients);
        }
        // Handle incoming data from existing clients
        for(uint32_t i = 0; i < max_clients; i++)
        {
            int sd;

            sd = client_sockets[i];

            if(fds[i + 1].revents & POLLIN)
            {
                receive_files(sd, &client_sockets, &max_clients, directory,client);
                printf("\n");

            }
        }
    }

    printf("Cleaning up\n");
    //Cleanup and close all client sockets
    for(uint32_t i = 0; i < max_clients; i++)
    {
        int sd;

        sd = client_sockets[i];

        if(sd > 0)
        {
            socket_close(sd);
        }
    }

    free(client_sockets);
    free(fds);
    socket_close(sockfd);
    printf("Server exited successfully.\n");

    return EXIT_SUCCESS;
}



static void parse_arguments(int argc, char *argv[], char **ip_address, char **port, char **directory)
{
    int opt;
    opterr     = 0;
    while((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt)
        {
            case 'h':
            {
                usage(argv[0], EXIT_SUCCESS, NULL);
            }
            case '?':
            {
                char message[UNKNOWN_OPTION_MESSAGE_LEN];
                snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                usage(argv[0], EXIT_FAILURE, message);
            }
            default:
            {
                usage(argv[0], EXIT_FAILURE, NULL);
            }
        }
    }
    if(optind > argc -3 )
    {
        usage(argv[0], EXIT_FAILURE, "Missing arguments.");
    }
    if(optind < argc - 3)
    {
        usage(argv[0], EXIT_FAILURE, "Too many arguments.");
    }
    *ip_address = argv[optind];
    *port       = argv[optind + 1];
    *directory = argv[optind + 2];
}


static void handle_arguments(const char *binary_name, const char *ip_address, const char *port_str, in_port_t *port)
{
    if(ip_address == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "The ip address is required.");
    }
    if(port_str == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "The port is required.");
    }
    *port = parse_in_port_t(binary_name, port_str);
}


in_port_t parse_in_port_t(const char *binary_name, const char *str)
{
    char      *endptr;
    uintmax_t parsed_value;
    errno        = 0;
    parsed_value = strtoumax(str, &endptr, BASE_TEN);
    if(errno != 0)
    {
        perror("Error parsing in_port_t");
        exit(EXIT_FAILURE);
    }

    // Check if there are any non-numeric characters in the input string
    if(*endptr != '\0')
    {
        usage(binary_name, EXIT_FAILURE, "Invalid characters in input.");
    }

    // Check if the parsed value is within the valid range for in_port_t
    if(parsed_value > UINT16_MAX)
    {
        usage(binary_name, EXIT_FAILURE, "in_port_t value out of range.");
    }
    return (in_port_t)parsed_value;
}


_Noreturn static void usage(const char *program_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n", message);
    }
    fprintf(stderr, "Usage: %s [-h] <ip address> <port>\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h  Display this help message\n", stderr);
    exit(exit_code);
}


static void setup_signal_handler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void sigint_handler(int signum)
{
    exit_flag = 1;
}


#pragma GCC diagnostic pop


static void convert_address(const char *address, struct sockaddr_storage *addr)
{
    memset(addr, 0, sizeof(*addr));
    if(inet_pton(AF_INET, address, &(((struct sockaddr_in *)addr)->sin_addr)) == 1)
    {
        // IPv4 address
        addr->ss_family = AF_INET;
    }
    else if(inet_pton(AF_INET6, address, &(((struct sockaddr_in6 *)addr)->sin6_addr)) == 1)
    {
        // IPv6 address
        addr->ss_family = AF_INET6;
    }
}


static int socket_create(int domain, int type, int protocol)
{
    int sockfd;
    sockfd = socket(domain, type, protocol);
    if(sockfd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


static void socket_bind(int sockfd, struct sockaddr_storage *addr, in_port_t port)
{
    char      addr_str[INET6_ADDRSTRLEN];
    in_port_t net_port;
    if(inet_ntop(addr->ss_family, addr->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr), addr_str, sizeof(addr_str)) == NULL)
    {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    printf("Binding to: %s:%u\n", addr_str, port);
    net_port = htons(port);
    if(addr->ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;
        ipv4_addr = (struct sockaddr_in *)addr;
        ipv4_addr->sin_port = net_port;
        if(bind(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1)
        {
            perror("Binding failed");
            exit(EXIT_FAILURE);
        }
    }
    else if(addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;
        ipv6_addr = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_port = net_port;
        if(bind(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in6)) == -1)
        {
            perror("Binding failed");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr, "Invalid address family: %d\n", addr->ss_family);
        exit(EXIT_FAILURE);
    }
    printf("Bound to socket: %s:%u\n", addr_str, port);
}


static void start_listening(int server_fd, int backlog)
{
    if(listen(server_fd, backlog) == -1)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Listening for incoming connections...\n");
}

static void handle_new_client(int server_socket, int **client_sockets, nfds_t *max_clients)
{


    errno = 0;
    struct sockaddr_storage address;
    socklen_t          client_len;
    int                new_socket;

    client_len = sizeof(address);
    new_socket = accept(server_socket, (struct sockaddr *)&address, &client_len);

    if(new_socket == -1)
    {
        if(errno != EINTR)
        {
            perror("accept failed");
        }
        exit(EXIT_FAILURE);
    }

    printf("New connection established\n");

    // Increase the size of the client_sockets array
    (*max_clients)++;
    *client_sockets = (int *)realloc(*client_sockets, sizeof(int) * (*max_clients));

    if(*client_sockets == NULL)
    {
        perror("Realloc error");
        exit(EXIT_FAILURE);
    }

    (*client_sockets)[(*max_clients) - 1] = new_socket;
}



static void socket_close(int sockfd)
{
    if(close(sockfd) == -1)
    {
        perror("Error closing socket");
        exit(EXIT_FAILURE);
    }
}


static void receive_files(int sd, int **client_sockets, const nfds_t *max_clients, const char *dir, int *client) {
    uint32_t filename_size;
    uint32_t valread ;
    valread = read(sd, &filename_size, sizeof(filename_size));

    if (valread <= 0) {
        printf("No more file left from client %d \n", client[sd]);
        handle_disconnection(sd, client_sockets, max_clients, client[sd]);

        return;
    }
    printf("\nreceiving files from client %d\n",client[sd]);
    char filename[filename_size + 1];
    valread = read(sd, filename, filename_size);
    if (valread <= 0) {
        handle_disconnection(sd, client_sockets, max_clients, client[sd]);
        return;
    }
    filename[filename_size] = '\0';
    uint32_t file_size;
    valread = read(sd, &file_size, sizeof(filename_size));
    if (valread <= 0) {
        handle_disconnection(sd, client_sockets, max_clients, client[sd]);
        return;
    }

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("fopen file path");
        return;
    }

    printf("File name: %s with the File size: %u is receiving.\n", filename, file_size);
    uint32_t bytes_read = 0;

    while (bytes_read < file_size)
    {
        // Inlined receive_buffer_size
        uint32_t buffer_size;
        uint32_t result = read(sd, &buffer_size, sizeof(buffer_size));
        if (result == -1)
        {
            fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
            return;
        }

        // Inlined safe_malloc
        char *buffer = malloc(buffer_size);
        if (!buffer && buffer_size > 0)
        {
            perror("Malloc failed\n");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, buffer_size);

        // Inlined receive_buffer
        uint32_t bytes_received_for_current_buffer = 0;
        while (bytes_received_for_current_buffer < buffer_size)
        {
            uint32_t temp_bytes_received = read(sd, buffer + bytes_received_for_current_buffer, buffer_size - bytes_received_for_current_buffer);
            if (temp_bytes_received == -1)
            {
                fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                free(buffer);
                return;
            }
            bytes_received_for_current_buffer += temp_bytes_received;
        }

        bytes_read += bytes_received_for_current_buffer;

        fwrite(buffer, 1, buffer_size, fp);
        fflush(fp);

        free(buffer);
    }
    fclose(fp);
}



static void handle_disconnection(int sd, int **client_sockets, const nfds_t *max_clients, int client) {
    printf("Client %d disconnected\n", client);
    close(sd);

    for (size_t i = 0; i < *max_clients; i++) {
        if ((*client_sockets)[i] == sd) {
            (*client_sockets)[i] = 0;
            break;
        }
    }
}

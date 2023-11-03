#include "server.h"
volatile int exit_flag = 0;

int parse_arguments(int argc, char *argv[], char **ip_address, char **port, char **directory, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    int opt;
    opterr     = 0;
    while((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt)
        {
            case 'h':
            {

                usage(argv[0], NULL);
                return -1;

            }
            case '?':
            {
                char message[UNKNOWN_OPTION_MESSAGE_LEN];
                snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                usage(argv[0], message);
                return -1;

            }
            default:
            {
                usage(argv[0], NULL);
                return -1;


            }
        }
    }
    if(optind > argc -3 )
    {
        SET_ERROR( context, "Missing arguments");
        return -1;

    }
    if(optind < argc - 3)
    {
        SET_ERROR( context, "Too many arguments.");
        return -1;
    }
    *ip_address = argv[optind];
    *port       = argv[optind + 1];
    *directory = argv[optind + 2];
    return 0;
}


int handle_arguments(const char *binary_name, const char *ip_address, const char *port_str, in_port_t *port, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    if(ip_address == NULL)
    {
        SET_ERROR( context, "The address is required.");
        return -1;

    }
    if(port_str == NULL)
    {
        SET_ERROR( context, "The port is required.");
        return -1;
    }
    if(parse_in_port_t(binary_name, port_str, port, ctx)== -1){
        return -1;
    }
    return 0;
}


int parse_in_port_t(const char *binary_name, const char *str, in_port_t *parsed_value, void* ctx)
{

    FSMContext* context = (FSMContext*) ctx;
    char      *endptr;
    uintmax_t temp_value;
    errno        = 0;
    temp_value = strtoumax(str, &endptr, BASE_TEN);
    if(errno != 0)
    {

        SET_ERROR( context, "Error parsing in_port_t");
        return -1;
    }

    // Check if there are any non-numeric characters in the input string
    if(*endptr != '\0')
    {
        SET_ERROR( context, "Invalid characters in input.");
        return -1;
    }

    // Check if the parsed value is within the valid range for in_port_t
    if(temp_value > UINT16_MAX)
    {
        SET_ERROR( context, "in_port_t value out of range.");
        return -1;
    }
    *parsed_value = (in_port_t )temp_value;
    return 0;
}


void usage(const char *program_name, const char *message)
{

    if(message)
    {
        fprintf(stderr, "%s\n", message);
    }
    fprintf(stderr, "Usage: %s [-h] <ip 4 or 6 address to bind to> <port> ./directory-to-store-files\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h  Display this help message\n", stderr);
}


int setup_signal_handler(void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        SET_ERROR( context, "sigaction");
        return -1;
    }
    return 0;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void sigint_handler(int signum)
{
    exit_flag = 1;
}


#pragma GCC diagnostic pop


int convert_address(const char *address, struct sockaddr_storage *addr, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    memset(addr, 0, sizeof(*addr));
    if(inet_pton(AF_INET, address, &(((struct sockaddr_in *)addr)->sin_addr)) == 1)
    {
        // IPv4 address
        addr->ss_family = AF_INET;
        return 0;
    }
    else if(inet_pton(AF_INET6, address, &(((struct sockaddr_in6 *)addr)->sin6_addr)) == 1)
    {
        // IPv6 address
        addr->ss_family = AF_INET6;
        return 0;
    }
    SET_ERROR(context, "Address family not supported");
    return -1;
}


int socket_create(int domain, int type, int protocol, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    int sockfd;
    sockfd = socket(domain, type, protocol);
    if(sockfd == -1)
    {
        SET_ERROR(context, "Socket creation failed");
        return -1;
    }
    return sockfd;
}


int socket_bind(int sockfd, struct sockaddr_storage *addr, in_port_t port, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    char      addr_str[INET6_ADDRSTRLEN];
    in_port_t net_port;
    if(inet_ntop(addr->ss_family, addr->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr), addr_str, sizeof(addr_str)) == NULL)
    {
        SET_ERROR(context, "inet_ntop");
        return -1;

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
            SET_ERROR(context,"Binding failed");
            return -1;
        }
    }
    else if(addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;
        ipv6_addr = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_port = net_port;
        if(bind(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in6)) == -1)
        {
            SET_ERROR(context,"Binding failed");
            return -1;
        }
    }
    else
    {
        SET_ERROR(context,"Invalid address family");
        return -1;
    }
    printf("Bound to socket: %s:%u\n", addr_str, port);
    return 0;
}


int start_listening(int server_fd, int backlog, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    if(listen(server_fd, backlog) == -1)
    {
        SET_ERROR(context,"listen failed");
        close(server_fd);
        return -1;
    }
    printf("Listening for incoming connections...\n");
    return 0;
}

int handle_new_client(int server_socket, int **client_sockets, nfds_t *max_clients, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
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
            SET_ERROR(context,strerror(errno));

        }
        return -1;
    }

    printf("New connection established\n");

    // Increase the size of the client_sockets array
    (*max_clients)++;
    *client_sockets = (int *)realloc(*client_sockets, sizeof(int) * (*max_clients));

    if(*client_sockets == NULL)
    {
        SET_ERROR(context,"Realloc error");
        return -1;
    }

    (*client_sockets)[(*max_clients) - 1] = new_socket;
    return 0;
}



int socket_close(int sockfd, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    if(close(sockfd) == -1)
    {
        SET_ERROR(context,"Error closing socket");
        return -1;
    }
    return 0;
}


int receive_files(int sd, int **client_sockets, const nfds_t *max_clients, const char *dir, int *client, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    uint32_t filename_size;
    uint32_t valread ;
    valread = read(sd, &filename_size, sizeof(filename_size));

    if (valread <= 0) {
        printf("No more file left from client %d \n", client[sd]);
        handle_disconnection(sd, client_sockets, max_clients, client[sd], ctx);

        return 0;
    }
    printf("\nreceiving files from client %d\n",client[sd]);
    char filename[filename_size + 1];
    valread = read(sd, filename, filename_size);
    if (valread <= 0) {
        handle_disconnection(sd, client_sockets, max_clients, client[sd], ctx);
        return -1;
    }
    filename[filename_size] = '\0';
    uint32_t file_size;
    valread = read(sd, &file_size, sizeof(filename_size));
    if (valread <= 0) {
        handle_disconnection(sd, client_sockets, max_clients, client[sd], ctx);
        return -1;
    }

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        perror("fopen file path");
        return -1;
    }

    printf("File name: %s with the File size: %u is receiving.\n", filename, file_size);
    uint32_t bytes_read = 0;

    while (bytes_read < file_size)
    {
        uint32_t buffer_size;
        if (read_buffer_size(sd, sizeof(buffer_size), (void *) &buffer_size, ctx))

        {
            SET_ERROR(context,"Error reading buffer size");
            fprintf(stderr, "read buffer size: %s (%d)\n", strerror(errno), errno);
            return -1;
        }

        char *buffer = malloc(buffer_size);
        if (!buffer && buffer_size > 0)
        {
            SET_ERROR(context,"Malloc failed");
            perror("Malloc failed\n");
            return -1;
        }

        memset(buffer, 0, buffer_size);

        uint32_t bytes_received_for_current_buffer = 0;
        while (bytes_received_for_current_buffer < buffer_size)
        {
            uint32_t temp_bytes_received = read(sd, buffer + bytes_received_for_current_buffer, buffer_size - bytes_received_for_current_buffer);
            if (temp_bytes_received == -1)
            {
                SET_ERROR(context,"Recv failed");
                fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                free(buffer);
                return -1;
            }
            bytes_received_for_current_buffer += temp_bytes_received;
        }

        bytes_read += bytes_received_for_current_buffer;

        fwrite(buffer, 1, buffer_size, fp);
        fflush(fp);

        free(buffer);
    }
    fclose(fp);
    return 0;
}

int read_buffer_size(int sockfd, uint32_t size, void *buffer, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    uint32_t bytes_read;
    uint32_t result;

    bytes_read = 0;
    while (bytes_read < size)
    {
        result = read(sockfd, buffer + bytes_read, size - bytes_read);

        if (result <= 0)
        {
            SET_ERROR(context,"Read buffer size failed ");
            fprintf(stderr, "read size: %s (%d)\n", strerror(errno), errno);
            return -1;
        }

        bytes_read += result;
    }

    return 0;
}

int handle_disconnection(int sd, int **client_sockets, const nfds_t *max_clients, int client, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    printf("Client %d disconnected\n", client);
    close(sd);

    for (size_t i = 0; i < *max_clients; i++) {
        if ((*client_sockets)[i] == sd) {
            (*client_sockets)[i] = 0;
            return 0;
        }
    }
    SET_ERROR(context,"Error closing the disconnection");
    // if we reach here, we couldn't find the socket descriptor in our list
    fprintf(stderr, "Error: Could not find socket descriptor %d in client_sockets array.\n", sd);
    return -1;
}

int setup_server_socket(int sockfd,void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        SET_ERROR(context,"setsockopt failed");
        return -1;
    }
    return 0;
}

void setup_fds(struct pollfd *fds, int *client_sockets, nfds_t max_clients, int sockfd, int *client) {
    // Set up the pollfd structure for the server socket
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    // Set up the pollfd structures for all client sockets
    for(uint32_t i = 0; i < max_clients; i++) {
        int sd = client_sockets[i];
        client[sd]= (int)i+1;
        fds[i + 1].fd = sd;
        fds[i + 1].events = POLLIN;
    }
}

int handle_clients(struct pollfd *fds, nfds_t max_clients, int *client_sockets, char *directory, int *client, void* ctx) {
    for(uint32_t i = 0; i < max_clients; i++) {
        int sd = client_sockets[i];
        if(fds[i + 1].revents & POLLIN) {
            int result = receive_files(sd, &client_sockets, &max_clients, directory, client,  ctx);
            printf("\n");
            if (result < 0) {
                return -1; // Return error if receive_files fails.
            }
        }
    }
    return 0;  // Return 0 if everything went smoothly
}

int cleanup_server(int *client_sockets, nfds_t max_clients, struct pollfd *fds, int sockfd, void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    printf("Cleaning up\n");
    for (uint32_t i = 0; i < max_clients; i++) {
        int sd = client_sockets[i];
        if (sd > 0) {
            if(close(sd) < 0) {
                SET_ERROR(context,"Error closing client socket");
                return -1;
            }
        }
    }

    free(client_sockets);
    free(fds);
    if (close(sockfd) < 0) {
        SET_ERROR(context,"Error closing server socket");
        return -1;
    }

    printf("Server exited successfully.\n");
    return 0;
}

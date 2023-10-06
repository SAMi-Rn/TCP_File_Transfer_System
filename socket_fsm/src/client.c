#include "client.h"



int parse_arguments(int argc, char *argv[], char **address, char **port, char ***file_paths, int *num_files)
{
    int opt;

    opterr = 0;

    while((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt)
        {
            case 'h':
            {
                return -1;
                usage(argv[0], EXIT_SUCCESS, NULL);

            }
            case '?':
            {
                char message[UNKNOWN_OPTION_MESSAGE_LEN];
                snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                return -1;
                usage(argv[0], EXIT_FAILURE, message);

            }
            default:
            {
                return -1;
                usage(argv[0], EXIT_FAILURE, NULL);

            }
        }
    }

    if(optind + 2 >= argc)
    {
        usage(argv[0], EXIT_FAILURE, "Too few arguments.");
        return -1;
    }

    *address = argv[optind];
    *port    = argv[optind + 1];
    *num_files = argc - (optind + 2);
    *file_paths = (char**) malloc(*num_files * sizeof(char*));

    for (int i = 0; i < *num_files; i++)
    {
        (*file_paths)[i] = argv[optind + 2 + i];
    }
    return 0;
}


int handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port)
{
    if(address == NULL)
    {
        return -1;
        usage(binary_name, EXIT_FAILURE, "The port is required.");
    }

    if(port_str == NULL)
    {
        return -1;
        usage(binary_name, EXIT_FAILURE, "The address is required.");
    }
    in_port_t *parsed_value;
    *port = parse_in_port_t(binary_name, port_str, parsed_value);
    return 0;
}


int parse_in_port_t(const char *binary_name, const char *str, in_port_t *parsed_value)
{
    char      *endptr;
    uintmax_t temp_value;

    errno        = 0;
    temp_value = strtoumax(str, &endptr, BASE_TEN);

    if(errno != 0)
    {
        perror("Error parsing in_port_t");
        return -1;
        exit(EXIT_FAILURE);
    }

    // Check if there are any non-numeric characters in the input string
    if(*endptr != '\0')
    {
        usage(binary_name, EXIT_FAILURE, "Invalid characters in input.");
    }

    // Check if the parsed value is within the valid range for in_port_t
    if(temp_value > UINT16_MAX)
    {
        usage(binary_name, EXIT_FAILURE, "in_port_t value out of range.");
    }
    *parsed_value = (in_port_t )temp_value;
    return 0;
}


_Noreturn void usage(const char *program_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] <address> <port>\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h  Display this help message\n", stderr);
    exit(exit_code);
}


int convert_address(const char *address, struct sockaddr_storage *addr)
{
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
    return -1;
}


int socket_create(int domain, int type, int protocol)
{
    int sockfd;
    sockfd = socket(domain, type, protocol);
    return sockfd;
}


int socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port)
{
    char      addr_str[INET6_ADDRSTRLEN];
    in_port_t net_port;

    if(inet_ntop(addr->ss_family, addr->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr), addr_str, sizeof(addr_str)) == NULL)
    {
        perror("inet_ntop");
        return -1;
        exit(EXIT_FAILURE);
    }

    printf("Connecting to: %s:%u\n", addr_str, port);
    net_port = htons(port);

    if(addr->ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;
        ipv4_addr = (struct sockaddr_in *)addr;
        ipv4_addr->sin_port = net_port;
        if(connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1)
        {
            perror("connect AF_INET");
            return -1;
            exit(EXIT_FAILURE);
        }
    }
    else if(addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;
        ipv6_addr = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_port = net_port;
        if(connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in6)) == -1)
        {
            perror("connect AF_INET6");
            return -1;
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr, "Invalid address family: %d\n", addr->ss_family);
        return -1;
        exit(EXIT_FAILURE);
    }


    printf("Connected to: %s:%u\n", addr_str, port);
    return 0;
}


int socket_close(int sockfd)
{
    if(close(sockfd) == -1)
    {
        perror("Error closing socket");
        return -1;
        exit(EXIT_FAILURE);
    }
    return 0;
}

int send_file(int sockfd, const char *file_path)
{
    sleep(3);

    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file %s: ", file_path);
        perror("");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    uint32_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t length = strlen(file_path);
    char *pathCopy = malloc(length + 1);

    if (pathCopy == NULL) {
        perror("Failed to allocate memory");
        return -1;
        exit(EXIT_FAILURE);
    }
    strcpy(pathCopy, file_path);
    char *filename = basename(pathCopy);
    uint32_t filename_size = strlen(filename);
    // Send the filename size and filename

    write(sockfd, &filename_size, sizeof(filename_size));

    write(sockfd, filename, filename_size);

    write(sockfd, &file_size, sizeof(file_size));

    printf("File name: %s with the File size: %u Bytes is sending.\n", filename, file_size);

    char buffer[1024];
    uint32_t buffer_size;

    while (file_size > 0)
    {

        memset(buffer, 0, sizeof(buffer));  // Clear the buffer

        buffer_size = fread(buffer, 1, sizeof(buffer) - 1, fp);

        if (buffer_size == 0) {
            // Handle end of file or error in fread
            return -1;
            perror("bytes read");
            break;
        }

        uint32_t bytes_written = write(sockfd, &buffer_size, sizeof(buffer_size));
        if (bytes_written <= 0) {
            // Handle write error
            perror("bytes written");
            return -1;
            break;
        }

        bytes_written = write(sockfd, buffer, buffer_size);

        if (bytes_written <= 0) {
            // Handle write error
            perror("bytes writen");
            return -1;
            break;
        }

        file_size -= buffer_size;
    }
    fclose(fp);
    return 0;
}
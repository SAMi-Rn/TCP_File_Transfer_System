
#include "client.h"


int parse_arguments(int argc, char *argv[], char **address, char **port, char ***file_paths, int *num_files, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    int opt;

    opterr = 0;

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
                usage(argv[0],  NULL);
                return -1;
            }
        }
    }

    if(optind + 2 >= argc)
    {
        SET_ERROR( context, "Too few arguments.");
        return -1;
    }

    *address = argv[optind];
    *port    = argv[optind + 1];
    int total_files = argc - (optind + 2);
    int valid_files_count = 0;

    // First pass: Count valid (existing) files
    for (int i = 0; i < total_files; i++)
    {
        if (access(argv[optind + 2 + i], F_OK) == 0) // F_OK tests for the existence of the file
        {
            valid_files_count++;
        }
    }
    *num_files = valid_files_count;
    *file_paths = (char**) malloc(*num_files * sizeof(char*));
    // Second pass: Store valid file paths
    int j = 0;
    for (int i = 0; i < total_files; i++)
    {
        if (access(argv[optind + 2 + i], F_OK) == 0)
        {
            (*file_paths)[j] = argv[optind + 2 + i];
            j++;
        }
    }
    return 0;
}


int handle_arguments(const char *binary_name, const char *address, const char *port_str, in_port_t *port, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    if(address == NULL)
    {
        SET_ERROR( context, "The port is required.");
        return -1;
    }

    if(port_str == NULL)
    {
        SET_ERROR( context, "The address is required.");
        return -1;
    }

    parse_in_port_t(binary_name, port_str, port, ctx);
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


void usage(const char *program_name,  const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] <address> <port>\n", program_name);
    fputs("Options:\n", stderr);
    fputs("  -h  Display this help message\n", stderr);
}


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


int socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port,  void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    char      addr_str[INET6_ADDRSTRLEN];
    in_port_t net_port;

    if(inet_ntop(addr->ss_family, addr->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr), addr_str, sizeof(addr_str)) == NULL)
    {
        SET_ERROR(context, "inet_ntop");
        return -1;
    }

    printf("\nConnecting to: %s:%u\n", addr_str, port);
    net_port = htons(port);

    if(addr->ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;
        ipv4_addr = (struct sockaddr_in *)addr;
        ipv4_addr->sin_port = net_port;
        if(connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1)
        {
            SET_ERROR(context, "connect AF_INET");
            return -1;
        }
    }
    else if(addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;
        ipv6_addr = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_port = net_port;
        if(connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in6)) == -1)
        {
            SET_ERROR(context, "connect AF_INET6");
            return -1;
        }
    }
    else
    {
        SET_ERROR(context,"Invalid address family");
        return -1;
    }


    printf("Connected to: %s:%u\n\n", addr_str, port);
    return 0;
}


int socket_close(int sockfd ,void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    if(close(sockfd) == -1)
    {
        SET_ERROR(context,"Error closing socket");
        return -1;
    }
    return 0;
}

int send_file(int sockfd, const char *file_path, void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    sleep(3);

    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        SET_ERROR(context,"Error opening file");
        fprintf(stderr, "File Path %s: ", file_path);
        perror("");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    uint32_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t length = strlen(file_path);
    char *pathCopy = malloc(length + 1);

    if (pathCopy == NULL) {
        SET_ERROR(context,"Failed to allocate memory");
        return -1;
    }
    strcpy(pathCopy, file_path);
    char *filename = basename(pathCopy);
    uint32_t filename_size = strlen(filename);
    // Send the filename size and filename

    write(sockfd, &filename_size, sizeof(filename_size));

    write(sockfd, filename, filename_size);

    write(sockfd, &file_size, sizeof(file_size));

    printf("\nFile name: %s with the File size: %u Bytes is sending.\n\n", filename, file_size);

    char buffer[1024];
    uint32_t buffer_size;

    while (file_size > 0)
    {

        memset(buffer, 0, sizeof(buffer));  // Clear the buffer

        buffer_size = fread(buffer, 1, sizeof(buffer) - 1, fp);

        if (buffer_size == 0) {
            SET_ERROR(context,"bytes read");
            return -1;
        }

        uint32_t bytes_written = write(sockfd, &buffer_size, sizeof(buffer_size));
        if (bytes_written <= 0) {
            SET_ERROR(context,"bytes written");
            return -1;
        }

        bytes_written = write(sockfd, buffer, buffer_size);

        if (bytes_written <= 0) {
            SET_ERROR(context,"bytes written");
            return -1;
        }

        file_size -= buffer_size;
    }
    fclose(fp);
    return 0;
}
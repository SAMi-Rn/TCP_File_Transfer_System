#include "client.h"

typedef enum {
    STATE_PARSE_ARGUMENTS,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_SOCKET_CREATE,
    STATE_SOCKET_CONNECT,
    STATE_SEND_FILE,
    STATE_CLEANUP,
    STATE_EXIT,
    STATE_ERROR
} client_state;
const char* state_to_string(client_state state) {
    switch(state) {
        case STATE_PARSE_ARGUMENTS:      return "STATE_PARSE_ARGUMENTS";
        case STATE_HANDLE_ARGUMENTS:     return "STATE_HANDLE_ARGUMENTS";
        case STATE_CONVERT_ADDRESS:      return "STATE_CONVERT_ADDRESS";
        case STATE_SOCKET_CREATE:        return "STATE_SOCKET_CREATE";
        case STATE_SOCKET_CONNECT:       return "STATE_SOCKET_CONNECT";
        case STATE_SEND_FILE:            return "STATE_SEND_FILE";
        case STATE_CLEANUP:              return "STATE_CLEANUP";
        case STATE_EXIT:                 return "STATE_EXIT";
        case STATE_ERROR:                return "STATE_ERROR";
        default:                         return "UNKNOWN_STATE";
    }
}
typedef struct {
    client_state state;
    client_state (*state_handler)(void* context);
    client_state next_states[2];  // 0 for success, 1 for failure
} FSMState;

typedef client_state (*StateHandlerFunc)(void* context);

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
    char *error_message;
    client_state error_from_state;
    client_state error_to_state;
    int error_line;
} FSMContext;


client_state parse_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering parse_arguments_handler .", STATE_PARSE_ARGUMENTS);

    if (parse_arguments(context->argc, context->argv, &context->address, &context->port_str, &context->file_paths, &context->num_files) != 0) {
        SET_ERROR(context, "Failed to parse arguments.", STATE_PARSE_ARGUMENTS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_HANDLE_ARGUMENTS;
}
client_state handle_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_arguments_handler.", STATE_HANDLE_ARGUMENTS);

    if (handle_arguments(context->argv[0], context->address, context->port_str, &context->port) != 0) {
        SET_ERROR(context, "Failed to handle arguments.", STATE_HANDLE_ARGUMENTS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_CONVERT_ADDRESS;
}

client_state convert_address_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering convert_address_handler.", STATE_CONVERT_ADDRESS);

    if (convert_address(context->address, &context->addr) != 0) {
        SET_ERROR(context, "Failed to convert address.", STATE_CONVERT_ADDRESS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SOCKET_CREATE;
}


client_state socket_create_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_create_handler.", STATE_SOCKET_CREATE);

    context->sockfd = socket_create(AF_INET, SOCK_STREAM, 0);
    if (context->sockfd == -1) {
        SET_ERROR(context, "Failed to create socket.", STATE_SOCKET_CREATE, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SOCKET_CONNECT;
}

client_state socket_connect_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_connect_handler.", STATE_SOCKET_CONNECT);

    if (socket_connect(context->sockfd, &context->addr, context->port) != 0) {
        SET_ERROR(context, "Failed to connect socket.", STATE_SOCKET_CONNECT, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SEND_FILE;
}

client_state send_file_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering send_file_handler.", STATE_SEND_FILE);

    if (context->current_file_index < context->num_files) {
        if (send_file(context->sockfd, context->file_paths[context->current_file_index]) != 0) {
            SET_ERROR(context, "Failed to send file.", STATE_SEND_FILE, STATE_ERROR);
            return STATE_ERROR;
        }
        context->current_file_index++;
        return STATE_SEND_FILE;  // repeat for the next file
    }
    return STATE_CLEANUP;
}

client_state cleanup_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering cleanup_handler.", STATE_CLEANUP);

    if (socket_close(context->sockfd) != 0) {
        SET_ERROR(context, "Failed to close socket.", STATE_CLEANUP, STATE_ERROR);
        return STATE_ERROR;
    }
    free(context->file_paths);
    return STATE_EXIT; // Or return STATE_EXIT or similar if you have an exit state
}


FSMState fsm_table[] = {
        { STATE_PARSE_ARGUMENTS,  parse_arguments_handler, { STATE_HANDLE_ARGUMENTS, STATE_ERROR } },
        { STATE_HANDLE_ARGUMENTS, handle_arguments_handler, { STATE_CONVERT_ADDRESS, STATE_ERROR } },
        { STATE_CONVERT_ADDRESS,  convert_address_handler,  { STATE_SOCKET_CREATE, STATE_ERROR } },
        { STATE_SOCKET_CREATE,    socket_create_handler,    { STATE_SOCKET_CONNECT, STATE_ERROR } },
        { STATE_SOCKET_CONNECT,   socket_connect_handler,   { STATE_SEND_FILE, STATE_ERROR } },
        { STATE_SEND_FILE,        send_file_handler,        { STATE_SEND_FILE, STATE_CLEANUP } },
        { STATE_CLEANUP,          cleanup_handler,          {  STATE_EXIT, STATE_ERROR } },
        { STATE_EXIT, NULL, { STATE_EXIT, STATE_ERROR } }  // We won't really use this state's handler

};



int main(int argc, char *argv[]) {
    FSMContext context = {
            .argc = argc,
            .argv = argv,
            .current_file_index = 0
    };

    client_state current_state = STATE_PARSE_ARGUMENTS;

    while (current_state != STATE_EXIT && current_state != STATE_ERROR) {
        FSMState* current_fsm_state = &fsm_table[current_state];
        client_state next_state = current_fsm_state->state_handler(&context);


        if (next_state == current_fsm_state->next_states[0]) {
            current_state = current_fsm_state->next_states[0];
        } else {
            current_state = current_fsm_state->next_states[1];
        }
    }



    if (current_state == STATE_ERROR) {
        fprintf(stderr, "Error: %s\nOccurred transitioning from state %d to state %d at line %d.\n",
                context.error_message, context.error_from_state, context.error_to_state, context.error_line);
    }

    return current_state == STATE_EXIT ? 0 : 1;
}

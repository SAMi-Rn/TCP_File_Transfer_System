#include "server.h"



typedef enum {
    STATE_PARSE_ARGUMENTS,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_SOCKET_CREATE,
    STATE_SETUP_SERVER_SOCKET,
    STATE_SOCKET_BIND,
    STATE_START_LISTENING,
    STATE_SETUP_SIGNAL_HANDLER,
    STATE_POLL,
    STATE_HANDLE_NEW_CLIENT,
    STATE_HANDLE_CLIENTS,
    STATE_ERROR,
    STATE_CLEANUP,
    STATE_EXIT // useful to have an explicit exit state
} ServerState;

typedef struct {
    ServerState state;
    ServerState (*state_handler)(void* context);
    ServerState next_states[2];  // 0 for success, 1 for failure
} FSMState;

typedef ServerState (*StateHandlerFunc)(void* context);
typedef struct {
    int argc;
    char **argv;
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
    int  client[1024];
    char *trace_message;
    ServerState trace_state;
    int trace_line;
    char *error_message;
    ServerState error_from_state;
    ServerState error_to_state;
    int error_line;
} FSMContext;

ServerState parse_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering parse_arguments_handler.", STATE_PARSE_ARGUMENTS);

    if (parse_arguments(context->argc, context->argv, &context->address, &context->port_str, &context->directory) != 0) {
        SET_ERROR(context, "Failed to parse arguments.", STATE_PARSE_ARGUMENTS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_HANDLE_ARGUMENTS;
}

ServerState handle_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_arguments_handler.", STATE_HANDLE_ARGUMENTS);

    if (handle_arguments(context->argv[0], context->address, context->port_str, &context->port) !=0) {
        SET_ERROR(context, "Failed to handle arguments.", STATE_HANDLE_ARGUMENTS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_CONVERT_ADDRESS;
}

ServerState convert_address_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering convert_address_handler.", STATE_CONVERT_ADDRESS);

    if (convert_address(context->address, &context->addr) != 0) {
        SET_ERROR(context, "Failed to convert address.", STATE_CONVERT_ADDRESS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SOCKET_CREATE;
}

ServerState socket_create_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_create_handler.", STATE_SOCKET_CREATE);

    context->sockfd = socket_create(context->addr.ss_family, SOCK_STREAM, 0);
    if (context->sockfd == -1) {
        SET_ERROR(context, "Failed to create socket.", STATE_SOCKET_CREATE, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SETUP_SERVER_SOCKET;
}

ServerState setup_server_socket_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering setup_server_socket_handler.", STATE_SETUP_SERVER_SOCKET);

    if (setup_server_socket(context->sockfd, &context->addr, context->port) != 0) {
        SET_ERROR(context, "Failed to set up the server socket.", STATE_SETUP_SERVER_SOCKET, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SOCKET_BIND;
}

ServerState socket_bind_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_bind_handler.", STATE_SOCKET_BIND);

    if (socket_bind(context->sockfd, &context->addr, context->port) != 0) {
        SET_ERROR(context, "Failed to bind socket.", STATE_SOCKET_BIND, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_START_LISTENING;
}

ServerState start_listening_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering start_listening_handler.", STATE_START_LISTENING);

    if (start_listening(context->sockfd, SOMAXCONN) != 0) {
        SET_ERROR(context, "Failed to start listening on the socket.", STATE_START_LISTENING, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_SETUP_SIGNAL_HANDLER;
}

ServerState setup_signal_handler_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering setup_signal_handler_handler.", STATE_SETUP_SIGNAL_HANDLER);

    if (setup_signal_handler() != 0) {
        SET_ERROR(context, "Failed to set up signal handler.", STATE_SETUP_SIGNAL_HANDLER, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_POLL;
}

ServerState poll_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering poll_handler.", STATE_POLL);
    // Avoid reallocating if possible
    struct pollfd* temp_fds = realloc(context->fds, (context->max_clients + 1) * sizeof(struct pollfd));
    if(temp_fds == NULL) {
        SET_ERROR(context, "Realloc error.", STATE_POLL, STATE_ERROR);
        return STATE_ERROR;
    }
    context->fds = temp_fds;

    // Ensure setup_fds is working correctly and not causing segmentation faults
    // Consider checking its return value or doing error handling inside setup_fds
    setup_fds(context->fds, context->client_sockets, context->max_clients, context->sockfd, context->client);

    context->num_ready = poll(context->fds, context->max_clients + 1, -1);

    // Handle the poll error
    if(context->num_ready < 0) {
        if(errno == EINTR) {
            printf("\nexit flag: %d\n", exit_flag);
            if(exit_flag) {
                return STATE_CLEANUP;
            }
            // Handle other cases if needed
        } else {
            SET_ERROR(context, "Poll error.", STATE_POLL, STATE_ERROR);
            return STATE_ERROR;
        }
    }

    // Ensure we are not accessing invalid memory
    if(context->max_clients >= 0 && context->fds[0].revents & POLLIN) {
        return STATE_HANDLE_NEW_CLIENT;
    } else {
        return STATE_HANDLE_CLIENTS;
    }
}



ServerState handle_new_client_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_new_client_handler.", STATE_HANDLE_NEW_CLIENT);
    if (handle_new_client(context->sockfd, &context->client_sockets, &context->max_clients) != 0) {
        SET_ERROR(context, "Failed to handle new client.", STATE_HANDLE_NEW_CLIENT, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_POLL;
}

ServerState handle_clients_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_clients_handler.", STATE_HANDLE_CLIENTS);
    if (handle_clients(context->fds, context->max_clients, context->client_sockets, context->directory, context->client) != 0) {
        SET_ERROR(context, "Failed to handle clients.", STATE_HANDLE_CLIENTS, STATE_ERROR);
        return STATE_ERROR;
    }
    return STATE_POLL;
}

ServerState cleanup_server_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering cleanup_server_handler.", STATE_CLEANUP);

    int result = cleanup_server(context->client_sockets, context->max_clients, context->fds, context->sockfd);

    if(result < 0) {
        SET_ERROR(context, "Failed during cleanup.", STATE_CLEANUP, STATE_ERROR);
        return STATE_ERROR;
    }

    // If cleanup was successful, we can terminate the program or return to a specific state.
    exit(EXIT_SUCCESS);  // this will terminate the program
}

FSMState fsm_table[] = {
        { STATE_PARSE_ARGUMENTS,      parse_arguments_handler,      {STATE_HANDLE_ARGUMENTS,    STATE_ERROR} },
        { STATE_HANDLE_ARGUMENTS,     handle_arguments_handler,     {STATE_CONVERT_ADDRESS,     STATE_ERROR} },
        { STATE_CONVERT_ADDRESS,      convert_address_handler,      {STATE_SOCKET_CREATE,       STATE_ERROR} },
        { STATE_SOCKET_CREATE,        socket_create_handler,        {STATE_SETUP_SERVER_SOCKET, STATE_ERROR} },
        { STATE_SETUP_SERVER_SOCKET,  setup_server_socket_handler,  {STATE_SOCKET_BIND,         STATE_ERROR} },
        { STATE_SOCKET_BIND,          socket_bind_handler,          {STATE_START_LISTENING,     STATE_ERROR} },
        { STATE_START_LISTENING,      start_listening_handler,      {STATE_SETUP_SIGNAL_HANDLER,STATE_ERROR} },
        { STATE_SETUP_SIGNAL_HANDLER, setup_signal_handler_handler, {STATE_POLL,                STATE_ERROR} },
        { STATE_POLL,                 poll_handler,                 {STATE_HANDLE_NEW_CLIENT,   STATE_HANDLE_CLIENTS} },
        { STATE_HANDLE_NEW_CLIENT,    handle_new_client_handler,    {STATE_POLL,                STATE_ERROR} },
        { STATE_HANDLE_CLIENTS,       handle_clients_handler,       {STATE_POLL,                STATE_ERROR} },
        { STATE_CLEANUP,              cleanup_server_handler,       {STATE_EXIT,                STATE_ERROR} },  // changed next state to STATE_EXIT
        { STATE_EXIT,                 NULL,                         {STATE_EXIT,                STATE_ERROR} }
};

int main(int argc, char **argv) {
    FSMContext context;
    memset(&context, 0, sizeof(context)); // Zero out the context structure
    context.argc = argc;
    context.argv = argv;

    ServerState current_state = STATE_PARSE_ARGUMENTS;

    while(current_state != STATE_EXIT && current_state != STATE_ERROR) {
        // Find the handler for the current state
        FSMState* current_fsm_state = &fsm_table[current_state];
        // Ensure the state has a handler
        // Ensure the state has a handler
        if (current_fsm_state->state_handler == NULL) {
            fprintf(stderr, "Error: No handler for state %d\n", current_state);
            current_state = STATE_ERROR;
            continue;
        }

        // Execute the state's handler
        ServerState next_state = current_fsm_state->state_handler(&context);

        // If the handler returns a state not defined in the next_states array of the FSM, use it directly
        if (next_state != current_fsm_state->next_states[0] && next_state != current_fsm_state->next_states[1]) {
            current_state = next_state;
        }
            // Otherwise, decide based on the exit_flag
        else {
            if (exit_flag) {
                current_state = STATE_CLEANUP;
            } else {
                // Assuming the first state in next_states[] is the "normal" transition and the second is an "error" or "alternative" transition
                current_state = (next_state == current_fsm_state->next_states[0]) ? current_fsm_state->next_states[0] : current_fsm_state->next_states[1];
            }
        }
    }
    if (current_state == STATE_ERROR) {
        fprintf(stderr, "Error: %s\nOccurred transitioning from state %d to state %d at line %d.\n",
                context.error_message, context.error_from_state, context.error_to_state, context.error_line);
    }

    return current_state == STATE_EXIT ? 0 : 1;
}

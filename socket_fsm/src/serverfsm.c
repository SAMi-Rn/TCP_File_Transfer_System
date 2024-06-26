#include "server.h"
const char* state_to_string(server_state state) {
    switch(state) {
        case STATE_PARSE_ARGUMENTS:      return "STATE_PARSE_ARGUMENTS";
        case STATE_HANDLE_ARGUMENTS:     return "STATE_HANDLE_ARGUMENTS";
        case STATE_CONVERT_ADDRESS:      return "STATE_CONVERT_ADDRESS";
        case STATE_SOCKET_CREATE:        return "STATE_SOCKET_CREATE";
        case STATE_SETUP_SERVER_SOCKET:  return "STATE_SETUP_SERVER_SOCKET";
        case STATE_SOCKET_BIND:          return "STATE_SOCKET_BIND";
        case STATE_START_LISTENING:      return "STATE_START_LISTENING";
        case STATE_SETUP_SIGNAL_HANDLER: return "STATE_SETUP_SIGNAL_HANDLER";
        case STATE_POLL:                 return "STATE_POLL";
        case STATE_HANDLE_NEW_CLIENT:    return "STATE_HANDLE_NEW_CLIENT";
        case STATE_HANDLE_CLIENTS:       return "STATE_HANDLE_CLIENTS";
        case STATE_CLEANUP:              return "STATE_CLEANUP";
        case STATE_ERROR:                return "STATE_ERROR";
        case STATE_EXIT:                 return "STATE_EXIT";
        default:                         return "UNKNOWN_STATE";
    }
}

server_state parse_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering parse_arguments_handler.", STATE_PARSE_ARGUMENTS);

    if (parse_arguments(context->argc, context->argv, &context->address, &context->port_str, &context->directory, ctx) != 0) {

        return STATE_ERROR;
    }
    return STATE_HANDLE_ARGUMENTS;
}

server_state handle_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_arguments_handler.", STATE_HANDLE_ARGUMENTS);

    if (handle_arguments(context->argv[0], context->address, context->port_str, &context->port, ctx) !=0) {
        return STATE_ERROR;
    }
    return STATE_CONVERT_ADDRESS;
}

server_state convert_address_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering convert_address_handler.", STATE_CONVERT_ADDRESS);

    if (convert_address(context->address, &context->addr, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_SOCKET_CREATE;
}

server_state socket_create_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_create_handler.", STATE_SOCKET_CREATE);

    context->sockfd = socket_create(context->addr.ss_family, SOCK_STREAM, 0, ctx);
    if (context->sockfd == -1) {
        return STATE_ERROR;
    }
    return STATE_SETUP_SERVER_SOCKET;
}

server_state setup_server_socket_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering setup_server_socket_handler.", STATE_SETUP_SERVER_SOCKET);

    if (setup_server_socket(context->sockfd, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_SOCKET_BIND;
}

server_state socket_bind_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_bind_handler.", STATE_SOCKET_BIND);

    if (socket_bind(context->sockfd, &context->addr, context->port,  ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_START_LISTENING;
}

server_state start_listening_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering start_listening_handler.", STATE_START_LISTENING);

    if (start_listening(context->sockfd, SOMAXCONN,  ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_SETUP_SIGNAL_HANDLER;
}

server_state setup_signal_handler_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering setup_signal_handler_handler.", STATE_SETUP_SIGNAL_HANDLER);

    if (setup_signal_handler( ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_POLL;
}

server_state poll_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering poll_handler.", STATE_POLL);

    context->fds = realloc(context->fds, (context->max_clients + 1) * sizeof(struct pollfd));
    if(context->fds == NULL) {
        return STATE_ERROR;
    }

    setup_fds(context->fds, context->client_sockets, context->max_clients, context->sockfd, context->client);

    context->num_ready = poll(context->fds, context->max_clients + 1, -1);
    if(context->num_ready < 0 && errno != EINTR) {
        SET_ERROR(context, "Poll error.");
        return STATE_ERROR;
    }
    if(context->num_ready < 0)
    {
        if(errno == EINTR)
        {
            return STATE_CLEANUP;
        }

        SET_ERROR(context, "Poll error.");
        return STATE_ERROR;
    }
    if(context->fds[0].revents & POLLIN) {
        return STATE_HANDLE_NEW_CLIENT;
    } else {
        return STATE_HANDLE_CLIENTS;
    }
}



server_state handle_new_client_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_new_client_handler.", STATE_HANDLE_NEW_CLIENT);
    if (handle_new_client(context->sockfd, &context->client_sockets, &context->max_clients,  ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_POLL;
}

server_state handle_clients_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_clients_handler.", STATE_HANDLE_CLIENTS);
    if (handle_clients(context->fds, context->max_clients, context->client_sockets, context->directory, context->client,  ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_POLL;
}

server_state cleanup_server_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering cleanup_server_handler.", STATE_CLEANUP);

    int result = cleanup_server(context->client_sockets, context->max_clients, context->fds, context->sockfd,  ctx);

    if(result < 0) {
        return STATE_ERROR;
    }

    // If cleanup was successful, we can terminate the program or return to a specific state.
    return STATE_EXIT; // this will terminate the program
}

server_state error_handler(void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering error_handler.", STATE_ERROR);
    fprintf(stderr, "ERROR: %s\nIn the function: %s \nInside the file: %s\nOn the line: %d\n",
            context->error_message, context->function_name, context->file_name, context->error_line);

    return STATE_CLEANUP;
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
        { STATE_CLEANUP,              cleanup_server_handler,       {STATE_EXIT,                STATE_ERROR} },
        { STATE_ERROR,                error_handler,                {STATE_CLEANUP,             STATE_CLEANUP} },
        { STATE_EXIT,                 NULL,                         {STATE_EXIT,                STATE_EXIT} }
};


int main(int argc, char **argv) {
    FSMContext context;
    memset(&context, 0, sizeof(context)); // Zero out the context structure
    context.argc = argc;
    context.argv = argv;

    server_state current_state = STATE_PARSE_ARGUMENTS;

    while(current_state != STATE_EXIT) {
        // Ensure the current state is within valid bounds
        if (current_state < 0 || current_state >= sizeof(fsm_table) / sizeof(FSMState)) {
            fprintf(stderr, "Error: Invalid state detected (%d).\n", current_state);
            return EXIT_FAILURE;
        }

        // Get the current FSM state
        FSMState* current_fsm_state = &fsm_table[current_state];

        // Ensure the state handler is not NULL
        if (!current_fsm_state->state_handler) {
            fprintf(stderr, "Error: Handler for state %d is NULL\n", current_state);
            return EXIT_FAILURE;
        }

        // Call the state handler
        server_state next_state = current_fsm_state->state_handler(&context);

        // If the exit flag is set, move to cleanup state
        if (exit_flag) {
            current_state = next_state;
            continue;

        }
        if (next_state == current_fsm_state->next_states[0] || next_state == current_fsm_state->next_states[1]) {
            current_state = next_state;
        } else {
            fprintf(stderr, "Error: Unexpected next state %d returned from handler for state %d\n", next_state, current_state);
            break;
        }

    }
    return current_state == STATE_EXIT ? EXIT_SUCCESS : EXIT_FAILURE;
}

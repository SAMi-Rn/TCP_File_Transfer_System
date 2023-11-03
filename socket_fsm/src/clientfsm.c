#include "client.h"
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

client_state parse_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering parse_arguments_handler .", STATE_PARSE_ARGUMENTS);

    if (parse_arguments(context->argc, context->argv, &context->address, &context->port_str, &context->file_paths, &context->num_files, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_HANDLE_ARGUMENTS;
}
client_state handle_arguments_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering handle_arguments_handler.", STATE_HANDLE_ARGUMENTS);

    if (handle_arguments(context->argv[0], context->address, context->port_str, &context->port, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_CONVERT_ADDRESS;
}

client_state convert_address_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering convert_address_handler.", STATE_CONVERT_ADDRESS);

    if (convert_address(context->address, &context->addr, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_SOCKET_CREATE;
}


client_state socket_create_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_create_handler.", STATE_SOCKET_CREATE);

    context->sockfd = socket_create(context->addr.ss_family, SOCK_STREAM, 0, ctx);
    if (context->sockfd == -1) {
        return STATE_ERROR;
    }
    return STATE_SOCKET_CONNECT;
}

client_state socket_connect_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering socket_connect_handler.", STATE_SOCKET_CONNECT);

    if (socket_connect(context->sockfd, &context->addr, context->port, ctx) != 0) {
        return STATE_ERROR;
    }
    return STATE_SEND_FILE;
}

client_state send_file_handler(void* ctx) {
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering send_file_handler.", STATE_SEND_FILE);

    if (context->current_file_index < context->num_files) {
        if (send_file(context->sockfd, context->file_paths[context->current_file_index], ctx) != 0) {
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

    if (socket_close(context->sockfd, ctx) != 0) {
        return STATE_ERROR;
    }
    free(context->file_paths);
    return STATE_EXIT; // Or return STATE_EXIT or similar if you have an exit state
}

client_state error_handler(void* ctx)
{
    FSMContext* context = (FSMContext*) ctx;
    SET_TRACE(context, "Entering error_handler.", STATE_ERROR);
    fprintf(stderr, "ERROR: %s\nIn the function: %s \nInside the file: %s\nOn the line: %d\n",
            context->error_message, context->function_name, context->file_name, context->error_line);

    return STATE_CLEANUP;
}
FSMState fsm_table[] = {
        { STATE_PARSE_ARGUMENTS,  parse_arguments_handler, { STATE_HANDLE_ARGUMENTS, STATE_ERROR } },
        { STATE_HANDLE_ARGUMENTS, handle_arguments_handler, { STATE_CONVERT_ADDRESS, STATE_ERROR } },
        { STATE_CONVERT_ADDRESS,  convert_address_handler,  { STATE_SOCKET_CREATE, STATE_ERROR } },
        { STATE_SOCKET_CREATE,    socket_create_handler,    { STATE_SOCKET_CONNECT, STATE_ERROR } },
        { STATE_SOCKET_CONNECT,   socket_connect_handler,   { STATE_SEND_FILE, STATE_ERROR } },
        { STATE_SEND_FILE,        send_file_handler,        { STATE_SEND_FILE, STATE_CLEANUP } },
        { STATE_ERROR,            error_handler,            { STATE_CLEANUP, STATE_CLEANUP } },
        { STATE_CLEANUP,          cleanup_handler,          {  STATE_EXIT, STATE_ERROR } },
        { STATE_EXIT,             NULL,                     { STATE_EXIT, STATE_EXIT } }  // We won't really use this state's handler

};
int main(int argc, char *argv[]) {
    FSMContext context = {
            .argc = argc,
            .argv = argv,
            .current_file_index = 0
    };

    client_state current_state = STATE_PARSE_ARGUMENTS;

    while (current_state != STATE_EXIT) {
        FSMState* current_fsm_state = &fsm_table[current_state];
        client_state next_state = current_fsm_state->state_handler(&context);

        if (next_state == STATE_ERROR) {
            current_state = next_state;
            continue;
        }
        if (next_state == current_fsm_state->next_states[0]) {
            current_state = current_fsm_state->next_states[0];
        } else {
            current_state = current_fsm_state->next_states[1];
        }
    }
    return current_state == STATE_EXIT ? EXIT_SUCCESS : EXIT_FAILURE;
}

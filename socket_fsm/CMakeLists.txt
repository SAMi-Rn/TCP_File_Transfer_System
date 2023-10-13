cmake_minimum_required(VERSION 3.26)
project(socket_fsm C)

set(CMAKE_C_STANDARD 17)

add_executable(server src/serverfsm.c
        src/server.c
        src/server.h

)
add_executable(client src/clientfsm.c
        src/client.c
        src/client.h
)



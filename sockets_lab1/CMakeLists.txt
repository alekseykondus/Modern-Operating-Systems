cmake_minimum_required(VERSION 3.15)
project(sockets_lab1)

set(CMAKE_C_STANDARD 99)

add_executable(client client.c)
add_executable(server server.c)


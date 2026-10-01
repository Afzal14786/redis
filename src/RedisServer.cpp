
#include "include/RedisServer.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

/**
 * Global pointer for signal handling
 */

static RedisServer* globalServer = nullptr;

RedisServer::RedisServer(int port) : port(port), server_socket(-1), running(true) {
    globalServer = this;
}

void RedisServer::shutdown() {
    running = false;
    if (server_socket != -1) {
        /**
         * @param close
         * this is a method define inside <unistd> header file as a POSIX system call.
         * it takes socker file descriptor (int) as an argument
         */
        close(server_socket);
    }

    std::cout << "Server Shutdown Completed.\n";
}

/**
 * running the server 
 */
void RedisServer::run() {
    
}
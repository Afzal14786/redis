
#include "../include/RedisServer.h"
#include "../include/RedisCommandHandler.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <cstring>

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
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {  // means there is an error
        std::cerr << "Error while creating server socket.\n";
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error while binding server socket.\n";
        return;
    }

    if (listen(server_socket, 10) < 0) {
        std::cerr << "Error while listining on Server socket.\n";
        return;
    }

    std::cout <<  "Redis server is listining on Port " << port << "\n";

    std::vector<std::thread> threads;  // this is for handling multiple clients
    RedisCommandHandler cmdHandler;

    // till the server is running
    while (running) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            if (running)
                std::cerr << "Error : Accepting client connection.\n";
            break;
        }

        threads.emplace_back([client_socket, &cmdHandler]() {
            char buffer[1024];
            while (true) {
                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) break;
                std::string request(buffer, bytes);
                std::string response = cmdHandler.processCommand(request);
                send(client_socket, response.c_str(), response.size(), 0);  // send the response to the client 
            }
            close(client_socket);  // closing the connection
        });
    }

    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }

    // shutdown
}
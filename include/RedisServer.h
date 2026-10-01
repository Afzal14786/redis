#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include <string>
#include <atomic>

/**
 * @brief This file contains the implementation of 
 * @class RedisServer
 * 
 */

class RedisServer {
public:
    RedisServer(int port);
    void run();
    void shutdown();
private:
    int port;
    int server_socket;
    std::atomic<bool> running;
};

#endif


#include "./include/RedisServer.h"
#include <iostream>


int main(int argc, char* argv[]) {
    /**
     * @param port 6379
     * @details this set the default port 
     */
    int port = 6379;

    /**
     * @details if user want to run this redis server is some specific port
     * @param argv[1]  takes the input from the user
     * @example 6380 or 6381 and so on
     */
    
    try {
        if (argc >= 2) 
            port = std::stoi(argv[1]);
    } catch(const std::exception& e) {
        std::cerr << "Invalid Port : " << e.what() << "\n";
        return 1;
    }

    /**
     * Creates a RedisServer instance configured to use the specified port.
     *
     * The RedisServer object is responsible for managing the connection
     * and communication with the Redis server.
     * @param port The port on which the Redis server will listen.
     */
    RedisServer server(port);

}
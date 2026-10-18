
#include "../include/RedisServer.h"
#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <cstring>
#include <signal.h>

/**
 * @brief Points to the active RedisServer instance for signal handling.
 *
 * The signal handler uses this pointer to request shutdown of the server
 * when SIGINT is received.
 *
 * @note This is a global pointer, so the implementation is designed around
 *       one active RedisServer instance.
 */
static RedisServer* globalServer = nullptr;

/**
 * @brief Handles the SIGINT signal.
 *
 * Logs the received signal, requests server shutdown when a server instance
 * is registered, and then terminates the process using exit().
 *
 * @param signum The numeric identifier of the received signal.
 *
 * @warning This handler performs operations that are not generally
 *          async-signal-safe, including stream-based logging and database
 *          persistence. A safer design would have the handler set a
 *          signal-safe flag and let normal program execution perform
 *          shutdown and persistence.
 */
void signalHandler(int signum) {
    if (globalServer) {
        std::cout << "\ncaught signal " << signum << ", shutting down.\n";
        globalServer->shutdown();
    }
    exit(signum);
}

/**
 * @brief Registers the SIGINT signal handler.
 *
 * When SIGINT is delivered, signalHandler() is invoked. This typically
 * happens when the user presses Ctrl+C in the terminal.
 */
void RedisServer::setupSignalHandler() {
    signal(SIGINT, signalHandler);
}

/**
 * @brief Initializes the RedisServer instance.
 *
 * Stores the configured port, initializes the listening socket descriptor
 * to -1 to indicate that no socket has been created yet, and initializes
 * the atomic running flag to true.
 *
 * The instance is also registered in globalServer so the signal handler
 * can access it, and the SIGINT handler is installed.
 *
 * @param port TCP port on which the server will listen.
 */
RedisServer::RedisServer(int port) : port(port), server_socket(-1), running(true) {
    globalServer = this;
    setupSignalHandler();
}

/**
 * @brief Stops the server and attempts to persist the database.
 *
 * First, sets running to false so the server's main loop knows that
 * shutdown has been requested.
 *
 * If the listening socket descriptor is valid, the method:
 * - Dumps the database to "dump.my_rdb".
 * - Logs whether the dump succeeded.
 * - Closes the listening socket.
 *
 * Finally, it prints a shutdown-completed message.
 *
 * @note The database dump is performed only when server_socket is not -1.
 *       The run() method also attempts a database dump after its client
 *       threads have been joined, so a shutdown can result in two dumps.
 *
 * @note close() is a POSIX system call declared in <unistd.h>. It closes
 *       the file descriptor passed to it.
 */
void RedisServer::shutdown() {
    running = false;
    if (server_socket != -1) {
        /**
         * @brief Persist the database before closing the listening socket.
         *
         * The dump is written to the relative path "dump.my_rdb".
         * The actual file location depends on the process's working directory.
         */
        // before shutdown persist the database
        if (RedisDatabase::getInstance().dump("dump.my_rdb")) {
            std::cout << "Database dumped to dump.my_rdb\n";
        } else {
            std::cerr << "Error while dumping the database.\n";
        }
        close(server_socket);
    }

    std::cout << "Server Shutdown Completed.\n";
}

/**
 * @brief Starts the TCP server and handles incoming client connections.
 *
 * The server startup sequence is:
 *
 * 1. Create a TCP socket using IPv4 and SOCK_STREAM.
 * 2. Enable SO_REUSEADDR to allow the local address to be reused where
 *    permitted by the operating system.
 * 3. Configure the IPv4 address structure to listen on the requested port
 *    and all available local network interfaces.
 * 4. Bind the socket to the configured address and port.
 * 5. Start listening for incoming connections with a backlog of 10.
 *
 * Once the server is listening, it repeatedly accepts client connections
 * while running is true. Every accepted client is handled by a separate
 * worker thread.
 *
 * Each worker thread:
 * - Receives request data into a fixed-size buffer.
 * - Passes the received bytes to RedisCommandHandler::processCommand().
 * - Sends the generated response back to the client.
 * - Closes the client socket when the receive loop ends.
 *
 * After the accept loop exits, the method joins all recorded worker threads
 * and attempts to persist the database to "dump.my_rdb".
 *
 * @note The current implementation uses a 1024-byte receive buffer per
 *       client and creates one thread per accepted connection.
 *
 * @note This method does not implement a complete stream-framing mechanism
 *       for requests split across multiple TCP reads, nor does it handle
 *       partial sends explicitly.
 */

void RedisServer::run() {
    // Create an IPv4 TCP socket.
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {  // means there is an error
        std::cerr << "Error while creating server socket.\n";
        return;
    }

    // Allow the local address to be reused where permitted.
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure the address and port on which the server will listen.
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Associate the socket with the configured local address and port.
    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error while binding server socket.\n";
        return;
    }

    // Start listening for incoming TCP connection requests.
    // The backlog value is 10.
    if (listen(server_socket, 10) < 0) {
        std::cerr << "Error while listining on Server socket.\n";
        return;
    }

    std::cout <<  "Redis server is listining on Port " << port << "\n";

    // Store worker threads so they can be joined before run() returns.
    std::vector<std::thread> threads;  // this is for handling multiple clients

    // The command handler is shared by the client worker threads.
    RedisCommandHandler cmdHandler;

    // Continue accepting connections while the server is running.
    while (running) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            // Report accept errors only if shutdown was not requested.
            if (running)
                std::cerr << "Error : Accepting client connection.\n";
            break;
        }

        // Create a dedicated worker thread for the newly accepted client.
        // The client socket is copied into the thread; cmdHandler is captured
        // by reference and remains in scope until the worker threads are joined.
        threads.emplace_back([client_socket, &cmdHandler]() {
            char buffer[1024];

            // Receive and process requests until the client disconnects
            // or a receive error occurs.
            while (true) {
                memset(buffer, 0, sizeof(buffer));

                // Reserve one byte for the null terminator, although the
                // request string below is constructed using the byte count.
                int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) break;

                // Construct a string using exactly the bytes received.
                std::string request(buffer, bytes);

                // Parse and execute the command, then generate its response.
                std::string response = cmdHandler.processCommand(request);

                // Send the response to the connected client.
                // The current implementation does not retry partial sends.
                send(client_socket, response.c_str(), response.size(), 0);
            }

            // Close the client connection after its receive loop ends.
            close(client_socket);  // closing the connection
        });
    }

    // Wait for all worker threads created by this run() invocation to finish.
    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }

    // Persist the database after the client worker threads have been joined.
    // This is a separate dump from the one that may occur in shutdown().
    if (RedisDatabase::getInstance().dump("dump.my_rdb"))
        std::cout << "Database Dumped to dump.my_rdb\n";
    else
        std::cerr << "Error dumping database\n";

}

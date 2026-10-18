
#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include <string>
#include <atomic>

/**
 * @file RedisServer.h
 * @brief Declares the RedisServer class responsible for running the TCP server.
 *
 * RedisServer manages the server socket, accepts incoming client connections,
 * coordinates the server's running state, and handles shutdown requests.
 */

 /**
  * @class RedisServer
  * @brief Runs a TCP server that processes client commands.
  *
  * The server listens on a configured port and creates a separate thread
  * for each accepted client connection. Client requests are processed by
  * RedisCommandHandler, while the shared RedisDatabase stores the data.
  *
  * The running flag is atomic so that its value can be accessed safely
  * across threads. The server also registers a SIGINT handler for Ctrl+C.
  */
class RedisServer {
public:
    /**
     * @brief Constructs a RedisServer for the specified port.
     *
     * Initializes the socket descriptor to -1 and sets the running flag
     * to true. It also registers this server instance for signal handling.
     *
     * @param port TCP port on which the server will listen.
     */
    RedisServer(int port);

    /**
     * @brief Starts the server and accepts client connections.
     *
     * Creates, binds, and listens on a TCP socket. For each accepted client,
     * a separate worker thread receives requests, processes commands, and
     * sends responses. The method waits for the worker threads to finish
     * before performing its final database dump.
     *
     * @note This method blocks while accepting connections and later while
     *       joining the client threads.
     */
    void run();

    /**
     * @brief Requests server shutdown and persists the database.
     *
     * Sets the running flag to false, attempts to dump the database to
     * "dump.my_rdb" when the server socket is valid, and closes the
     * listening socket.
     *
     * @note This method does not directly close every client socket or
     *       join the worker threads; run() joins its recorded worker threads.
     */
    void shutdown();

private:
    int port;                       ///< TCP port on which the server listens.
    int server_socket;              ///< File descriptor for the listening socket.
    std::atomic<bool> running;       ///< Indicates whether the server should keep running.

    /**
     * @brief Registers the handler for the SIGINT signal.
     *
     * Configures the process to call signalHandler when SIGINT is received,
     * typically when the user presses Ctrl+C in the terminal.
     */
    void setupSignalHandler();
};

#endif

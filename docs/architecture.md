# System Architecture

## 1. Overview

This project implements a Redis-like in-memory database server in C++. It accepts client connections over TCP, processes commands, stores data in memory, and persists database contents to a local file.

The application is organized into three main components:

- **RedisServer:** Manages TCP connections, accepts clients, and handles client communication using worker threads.
- **RedisCommandHandler:** Parses incoming commands, identifies the requested operation, invokes the appropriate database method, and constructs the response.
- **RedisDatabase:** Stores and manages string, list, and hash data. It also provides expiration handling and file-based persistence.

The application entry point, `main.cpp`, initializes the database, configures the server port, starts background persistence, and runs the server.

## 2. High-Level Architecture

```markdown
                  Client Application
                          |
                          | TCP Connection
                          v
                 +-------------------+
                 |    RedisServer    |
                 |                   |
                 | Socket Management |
                 | Accept Connections|
                 +-------------------+
                          |
                          v
                 +-------------------+
                 |   Worker Thread   |
                 |                   |
                 | recv() / send()   |
                 +-------------------+
                          |
                          v
                 +-------------------+
                 | RedisCommandHandler|
                 |                   |
                 | Parse Command     |
                 | Dispatch Command  |
                 | Build Response    |
                 +-------------------+
                          |
                          v
                 +-------------------+
                 |   RedisDatabase   |
                 |                   |
                 | String Store      |
                 | List Store        |
                 | Hash Store        |
                 | Expiration Data   |
                 +-------------------+
                          |
                          v
                 +-------------------+
                 |   In-Memory Data  |
                 +-------------------+

     Persistence:
     RedisDatabase <----> dump.my_rdb
```

The diagram shows the logical request path. Persistence is handled separately from the normal client request path, through database loading and dumping.

## 3. Application Entry Point — `main.cpp`

The `main()` function coordinates application startup.

### 3.1 Port Configuration

The server uses port `6378` by default. If a command-line argument is supplied, the program attempts to convert it to an integer using `std::stoi`.

For example:

```bash
./redis
./redis 6380
```

The second command requests that the server listen on port `6380`.

If conversion throws an exception, the program prints an error and exits with status code `1`.

### 3.2 Database Initialization

Before starting the server, the application calls:

```cpp
RedisDatabase::getInstance().load("dump.my_rdb");
```

The database attempts to restore its contents from the persistence file. If loading fails, the application prints a message and continues with an empty database.

### 3.3 Background Persistence

The application creates a background thread that periodically calls:

```cpp
RedisDatabase::getInstance().dump("dump.my_rdb");
```

The thread sleeps for 300 seconds between attempts, corresponding to five minutes.

The thread is detached, allowing `main()` to continue without joining it. The current implementation does not provide a mechanism to stop this thread explicitly.

### 3.4 Server Startup

After initializing the database and starting the persistence thread, `main()` constructs a `RedisServer` object and calls:

```cpp
server.run();
```

The `run()` method manages the server socket and client connections.

## 4. RedisServer

The `RedisServer` class is responsible for network communication and client connection management.

Its main responsibilities are:

- Creating an IPv4 TCP socket.
- Configuring and binding the socket to the selected port.
- Listening for incoming client connections.
- Accepting connections and creating worker threads.
- Receiving client requests and sending responses.
- Coordinating shutdown and database persistence.

### Client Thread Model

The server creates one worker thread for each accepted client connection. Each worker repeatedly receives data, processes it through `RedisCommandHandler`, and sends the resulting response.

The worker threads are stored in a vector. When the server's accept loop ends, `run()` joins the threads before returning.

## 5. RedisCommandHandler

The `RedisCommandHandler` class acts as the command-processing layer between the network server and the database.

Its responsibilities include:

1. Parsing incoming request data into command tokens.
2. Identifying the command.
3. Dispatching the command to the appropriate handler.
4. Calling the relevant `RedisDatabase` method.
5. Formatting the result as a protocol response.

For example, a `GET` command is dispatched to its handler, which retrieves the requested value from `RedisDatabase` and formats the response.

This separation keeps command interpretation and response formatting outside the database storage layer.

## 6. RedisDatabase

The `RedisDatabase` class manages the in-memory data and provides the operations used by the command handlers.

The implementation maintains separate stores for:

- **Strings:** Key-value data.
- **Lists:** Ordered collections of string values.
- **Hashes:** Field-value mappings grouped under a key.
- **Expiration metadata:** Expiration deadlines associated with keys.

The database uses a singleton instance so the server and background persistence thread can access the same database object.

Database methods use a mutex to protect the relevant shared data structures from concurrent access.

### Expiration Handling

The database maintains expiration deadlines using `std::chrono::steady_clock`. Expired data is removed when operations that invoke the expiration-cleanup helpers access it. Not every list and hash operation currently performs this cleanup.

## 7. Request Lifecycle

Consider a client sending a command such as:

```bash
SET username Afzal
```

The request follows this general sequence:

1. **Client:** Sends command data over the TCP connection.
2. **RedisServer:** The worker thread receives the available bytes using `recv()`.
3. **RedisCommandHandler:** The request is parsed into tokens and the command is identified.
4. **Command handler:** The appropriate database operation is invoked.
5. **RedisDatabase:** The operation accesses or modifies the in-memory stores.
6. **Command handler:** The result is formatted as a response.
7. **RedisServer:** The worker sends the response through the client socket.

This division of responsibility allows the networking, command-processing, and data-storage code to remain separate.

## 8. Persistence and Shutdown

The project uses a local file named `dump.my_rdb` for persistence.

### Database Loading

The database attempts to load the file during application startup, before the server begins accepting client connections.

### Periodic Persistence

A detached background thread attempts to dump the database every five minutes.

### Shutdown Persistence

The server attempts to dump the database during `shutdown()` when its listening socket is valid. After the accept loop ends and the recorded client threads have been joined, `run()` also attempts a dump.

The current implementation can therefore perform multiple dumps during shutdown.

The persistence format stores string, list, and hash records. Expiration deadlines are not persisted or restored, and the current dump implementation does not first purge all expired entries.

## 9. Concurrency Model

The application uses multiple threads to handle simultaneous client connections.

- Each accepted client receives a dedicated worker thread.
- The worker threads share the same `RedisCommandHandler` object.
- Database operations access the shared singleton database.
- Database mutexes protect the data structures used by those operations.
- A separate detached thread periodically performs database persistence.

Although the server supports concurrent connections, this is a basic thread-per-client design rather than a thread pool or event-driven networking model.

## 10. Current Limitations

The following are implementation characteristics to consider during future development:

- A separate thread is created for each accepted client.
- TCP request framing is not fully handled; one `recv()` call is treated as a request.
- Partial socket sends are not retried.
- The detached persistence thread has no explicit shutdown mechanism.
- Signal handling performs operations that are not generally safe inside a POSIX signal handler.
- The persistence format does not save expiration deadlines.
- Some database operations do not proactively clean up expired keys.

These limitations describe the current implementation and can be addressed as separate reliability and performance improvements.

## 11. Summary

The application follows a layered design: `RedisServer` handles network communication, `RedisCommandHandler` interprets commands, and `RedisDatabase` manages data and persistence.

This separation makes the project easier to understand, test, extend, and discuss during software engineering interviews.
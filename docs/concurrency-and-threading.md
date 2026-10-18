# Concurrency and Threading

## 1. Overview

This document explains how the Redis-like server handles multiple client connections, how shared resources are accessed concurrently, and how synchronization is implemented.

The server uses C++ threads to handle multiple clients concurrently. It also runs a background thread for periodic database persistence.

The main concurrency-related components are:

- `RedisServer`: Accepts client connections and creates worker threads.
- `RedisCommandHandler`: Processes commands received by worker threads.
- `RedisDatabase`: Stores shared data and synchronizes database operations.
- Background persistence thread: Periodically attempts to save the database to disk.

Understanding these components is important for evaluating thread safety, data consistency, resource management, and shutdown behavior.

## 2. Threading Model

### 2.1 Main Thread

The main thread executes `main()` in `src/main.cpp`.

Its responsibilities include:

1. Selecting the server port.
2. Loading existing database data from `dump.my_rdb`.
3. Constructing the `RedisServer` instance.
4. Starting the background persistence thread.
5. Calling `server.run()`.

The call to `server.run()` keeps the main thread occupied with the server's accept loop until that loop terminates.

### 2.2 Server Accept Loop

Inside `RedisServer::run()`, the server repeatedly calls `accept()` to receive incoming client connections.

For every accepted connection, it creates a worker thread using `std::thread` and stores that thread in a vector.

Conceptually:

```cpp
int client_socket = accept(server_socket, nullptr, nullptr);

threads.emplace_back([client_socket, &cmdHandler]() {
    // Receive and process client requests.
});
```

This is a thread-per-client design: each accepted connection receives its own worker thread.

### 2.3 Client Worker Threads

Each worker thread handles communication with one client connection.

The worker repeatedly:

1. Receives bytes using `recv()`.
2. Converts the received bytes into a request string.
3. Calls `RedisCommandHandler::processCommand()`.
4. Sends the resulting response using `send()`.
5. Continues until the receive loop ends.
6. Closes its client socket.

Because each worker has its own client socket and local receive buffer, these resources are not shared between different workers.

However, the worker threads share the same command-handler object and database instance.

### 2.4 Background Persistence Thread

In `main.cpp`, a separate thread periodically calls:

```cpp
RedisDatabase::getInstance().dump("dump.my_rdb");
```

The thread sleeps for 300 seconds between dump attempts and is detached using `detach()`.

This allows the main thread to continue running the server without joining the persistence thread.

The current implementation does not provide a stop flag or another explicit mechanism for terminating this thread during shutdown.

## 3. Shared Resources

Concurrent execution requires careful handling of resources accessed by multiple threads.

### 3.1 Shared Command Handler

`RedisServer::run()` creates one `RedisCommandHandler` object:

```cpp
RedisCommandHandler cmdHandler;
```

Each worker thread captures this object by reference:

```cpp
[client_socket, &cmdHandler]
```

Consequently, all worker threads invoke `processCommand()` on the same object.

The implementation uses local variables for parsed command tokens and the generated response. The database is accessed through the singleton instance.

This design avoids constructing a separate command-handler object for every request. Its thread safety depends on whether the shared handler and all methods it invokes avoid unsynchronized shared mutable state.

### 3.2 Shared Database Singleton

The application accesses the database through:

```cpp
RedisDatabase::getInstance()
```

The singleton provides one shared database instance for client workers and the background persistence thread.

Without synchronization, simultaneous database operations could cause data races or corrupt shared containers.

The database therefore uses a mutex to protect access to its internal stores.

### 3.3 Shared Server State

The server maintains the following important state:

- `running`: An atomic boolean indicating whether the server should continue accepting clients.
- `server_socket`: The file descriptor for the listening socket.
- `globalServer`: A global pointer used by the signal handler.

These variables have different synchronization properties. The atomic `running` flag supports concurrent reads and writes, but the socket descriptor and global pointer are not made thread-safe simply by using an atomic flag elsewhere.

## 4. Mutex-Based Synchronization

### 4.1 Why a Mutex Is Needed

The database contains shared data structures for strings, lists, hashes, and expiration metadata.

Multiple worker threads can attempt to read or modify these structures concurrently. A mutex provides mutual exclusion so that operations using the same mutex do not execute their protected sections simultaneously.

For example, database methods acquire `db_mutex` before accessing the relevant stores.

Conceptually:

```cpp
std::lock_guard<std::mutex> lock(db_mutex);
```

The lock is acquired when the guard is constructed and released automatically when the guard leaves scope.

Automatic lock release is particularly useful when a method returns early, because it reduces the risk of accidentally leaving a mutex locked.

### 4.2 Operations Protected by the Database Mutex

The implementation uses `db_mutex` in its database operations, including:

- String operations such as `get()`, `set()`, and `del()`.
- Key operations such as `keys()`, `type()`, `expire()`, and `rename()`.
- List operations such as `list_l_push()`, `list_r_push()`, and `list_l_pop()`.
- Hash operations such as `hash_set()`, `hash_get()`, and `hash_del()`.
- Persistence operations such as `dump()` and `load()`.

These methods synchronize their access through the database mutex.

### 4.3 What the Mutex Guarantees

When all relevant access to a shared data structure is protected by the same mutex, concurrent operations cannot execute their protected sections simultaneously.

For example, if one thread is modifying a list while another thread calls a database method that acquires the same mutex, the second operation must wait until the first operation releases the lock.

This helps prevent concurrent modifications from corrupting the underlying containers.

### 4.4 What the Mutex Does Not Guarantee

A mutex protects the critical sections that use it. It does not automatically make a sequence of separate method calls atomic.

Consider a command handler that performs these operations:

1. Pushes several values into a list using multiple database calls.
2. Retrieves the list length in a separate database call.
3. Returns the length to the client.

Each individual database call may be protected by the mutex, but another client can modify the list between calls.

Therefore, the returned length may reflect a later concurrent modification rather than only the effect of the original command.

This is a distinction between thread-safe individual operations and atomic multi-step commands.

## 5. Persistence and Concurrency

The background persistence thread and client worker threads share the same database instance.

The `dump()` implementation acquires the database mutex while writing the database contents to the dump file.

As a result, database operations that use the same mutex must wait while the dump holds the lock. Similarly, the dump waits for an operation that already holds the mutex.

This protects the in-memory containers while the dump is being generated, but it can affect request latency when persistence takes a long time.

### 5.1 Multiple Dump Attempts

The application can attempt persistence from several execution paths:

- The detached background persistence thread.
- `RedisServer::shutdown()`.
- The end of `RedisServer::run()` after the worker threads have been joined.

Database mutex protection serializes access to the database data during these operations. However, the broader lifecycle still needs careful coordination, especially because the background thread is detached and does not have an explicit shutdown protocol.

### 5.2 Persistence Does Not Guarantee Durability

A successful command response indicates that the command handler produced a response after processing the operation. It does not establish that the data has been saved to disk.

Likewise, the current dump implementation does not check all write errors after successfully opening the file. The persistence mechanism should not be treated as a fully durable storage system.

## 6. Thread Lifecycle and Shutdown

### 6.1 Joining Worker Threads

The server stores worker threads in a vector:

```cpp
for (auto &t : threads) {
    if (t.joinable()) t.join();
}
```

Calling `join()` waits for the corresponding thread to finish.

This ensures that `run()` does not return while its recorded worker threads are still executing.

### 6.2 Why Shutdown Can Block

The server's `shutdown()` method sets `running` to false and closes the listening socket.

Closing the listening socket helps the accept loop terminate. However, it does not directly close the separate client sockets owned by worker threads.

A worker blocked in `recv()` may remain blocked if its client connection stays open and no additional data arrives. In that situation, `run()` may wait indefinitely when it tries to join the worker.

A more complete shutdown design would track active client sockets or use another mechanism to notify workers and unblock their receive operations.

### 6.3 Signal Handling

The constructor registers a handler for `SIGINT`, commonly triggered by Ctrl+C.

The current handler logs a message, calls `shutdown()`, and then calls `exit()`.

This is not a robust POSIX signal-handling design because operations such as stream-based logging, database dumping, and mutex acquisition are not generally safe to perform directly in a signal handler.

A safer approach is to let the signal handler set a suitable signal-safe notification flag and perform shutdown, persistence, and logging in normal program execution.

### 6.4 Shared Socket Descriptor

The `server_socket` member is not atomic and is accessed by both the normal server lifecycle and the shutdown path.

Closing a descriptor while another thread is using it requires careful coordination. Simply making `running` atomic does not synchronize every operation involving `server_socket`.

The socket lifecycle should be designed so that shutdown and normal socket operations cannot interfere unpredictably.

## 7. Current Concurrency Limitations

The current implementation has the following limitations:

1. **One thread per client:** A large number of connections can create a large number of operating-system threads.
2. **Shared handler object:** The same command-handler instance is used by multiple workers, so its implementation must remain safe for concurrent calls.
3. **Multi-step commands are not necessarily atomic:** Separate mutex-protected database calls can interleave with operations from other clients.
4. **Potentially blocking shutdown:** Client workers blocked in `recv()` may prevent `join()` from completing.
5. **Detached persistence thread:** There is no explicit stop-and-join lifecycle for the periodic persistence worker.
6. **Signal-safety concerns:** The signal handler invokes functions that are not generally safe in an asynchronous signal context.
7. **Socket lifecycle coordination:** The listening socket is not protected by a dedicated synchronization strategy.
8. **Lock contention during persistence:** Holding the database mutex during a dump can delay concurrent commands.
9. **No bounded worker pool:** The number of worker threads can grow with the number of accepted client connections.

These are observations about the current implementation and possible areas for future engineering improvements.

## 8. Potential Improvements

Future iterations could consider:

- A bounded thread pool or event-driven networking model.
- A coordinated shutdown mechanism that unblocks active client workers.
- Explicit lifecycle management for the persistence thread.
- Signal-safe shutdown notification.
- Stronger socket descriptor ownership and synchronization.
- Atomic database operations for commands that require multi-step consistency.
- Reducing the duration of database locks during persistence, potentially by taking a consistent snapshot before writing it to disk.
- Load testing with many concurrent clients to measure throughput, latency, and resource usage.

Each improvement should be implemented and tested separately to avoid introducing regressions.

## 9. Summary

The server uses a thread-per-client model and a detached background persistence thread. Its worker threads share a command-handler object and a singleton database.

The database mutex protects individual database operations that acquire it, helping preserve the integrity of the shared data structures. However, it does not automatically make multi-step commands atomic or solve the server's socket and thread shutdown lifecycle.

Understanding these distinctions is essential for explaining the implementation's concurrency model and identifying the next steps toward a more robust server.
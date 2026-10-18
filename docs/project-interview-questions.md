# Redis-Inspired C++ Server — Project Interview Questions and Answers

This document is a comprehensive interview-preparation guide for the Redis-inspired in-memory database server implemented in C++.

It covers the architecture, implementation decisions, networking, protocol parsing, data structures, concurrency, expiration, persistence, error handling, complexity analysis, limitations, and possible improvements.

The answers are written to help explain both **what the project does** and **why it works the way it does**.

> **Important:** These answers describe the implementation reviewed during documentation. Before an interview, verify exact response formats, error messages, and any code details against the current source files.

---

# Table of Contents

1. [Project Overview and Architecture](#1-project-overview-and-architecture)
2. [C++ Design and Object-Oriented Programming](#2-c-design-and-object-oriented-programming)
3. [TCP Networking and Client Communication](#3-tcp-networking-and-client-communication)
4. [Protocol Parsing and Command Processing](#4-protocol-parsing-and-command-processing)
5. [Database Design and Data Structures](#5-database-design-and-data-structures)
6. [Concurrency and Thread Safety](#6-concurrency-and-thread-safety)
7. [TTL and Expiration](#7-ttl-and-expiration)
8. [Persistence and Recovery](#8-persistence-and-recovery)
9. [Error Handling and Reliability](#9-error-handling-and-reliability)
10. [Performance and Complexity](#10-performance-and-complexity)
11. [Limitations and Future Improvements](#11-limitations-and-future-improvements)
12. [Advanced Interview Follow-Ups](#12-advanced-interview-follow-ups)
13. [Final Project Explanation](#13-final-project-explanation)

---

# 1. Project Overview and Architecture

## Q1. Tell me about your project.

**Answer:**

I built a Redis-inspired in-memory key-value database server using C++. The goal was to understand how a database server works internally rather than relying entirely on an existing database implementation.

The server communicates with clients over TCP. It receives commands, parses them, executes the corresponding database operations, and returns responses in Redis-style formats.

The database currently supports three main data structures:

- Strings for key-value storage.
- Lists for ordered collections of strings.
- Hashes for field-value mappings.

In addition to these data structures, I implemented key expiration using TTL and file-based persistence so that stored data can be loaded when the server starts again.

The application is divided into several components. `RedisServer` handles network communication, `RedisCommandHandler` parses and executes commands, and `RedisDatabase` manages the in-memory data. The `main.cpp` file coordinates startup, database loading, periodic persistence, and server execution.

The project also gave me practical experience with TCP sockets, multithreading, mutexes, C++ data structures, protocol parsing, and file I/O.

**Why this answer works:** It explains the problem, the main features, the architecture, and the engineering concepts without getting lost in implementation details.

## Q2. Why did you build a Redis-like server instead of using Redis directly?

**Answer:**

Using Redis would have allowed me to store and retrieve data quickly, but it would not have helped me understand the internal mechanisms responsible for those operations.

I wanted to explore questions such as:

- How does a server accept multiple client connections?
- How are commands represented and parsed?
- How does a command handler communicate with a database layer?
- How should shared data be protected when multiple threads access it?
- How can keys expire automatically?
- How can in-memory data be saved and recovered?
- What problems arise when implementing a network protocol over TCP?

Building a simplified Redis-inspired server allowed me to investigate these questions directly.

I also learned that implementing a database involves more than storing values in maps. Networking, synchronization, persistence, input validation, failure handling, and protocol compatibility all affect correctness.

The project is an educational implementation, not a replacement for production Redis.

## Q3. What is the overall architecture of your project?

**Answer:**

The architecture separates the application into four main responsibilities.

1. **Application entry point:** `main.cpp` parses the optional port argument, loads the persistence file, starts the periodic persistence thread, and runs the server.
2. **Networking layer:** `RedisServer` creates the TCP listening socket, accepts clients, receives data, and sends responses.
3. **Command-processing layer:** `RedisCommandHandler` parses the request, normalizes the command name, validates arguments, dispatches the operation, and formats the response.
4. **Database layer:** `RedisDatabase` manages strings, lists, hashes, expiration metadata, and file persistence.

The request flow is:

```text
Client
  |
  | TCP request
  v
RedisServer
  |
  | Received bytes
  v
RedisCommandHandler
  |
  | Database operation
  v
RedisDatabase
  |
  | Operation result
  v
RedisCommandHandler
  |
  | Formatted response
  v
RedisServer
  |
  | TCP response
  v
Client
```

This separation is useful because each component has a focused responsibility. For example, the database does not need to know how TCP connections are accepted, and the networking layer does not need to know how a hash field is stored.

However, separation alone does not guarantee correctness. The components must also agree on request boundaries, error behavior, synchronization, and data semantics.

## Q4. Explain the responsibility of each major source file.

**Answer:**

| File | Responsibility |
|---|---|
| `src/main.cpp` | Application startup, port parsing, loading persisted data, periodic persistence, and starting the server |
| `include/RedisServer.h` | Declares the server interface and its main state |
| `src/RedisServer.cpp` | Socket setup, connection acceptance, client worker threads, receiving requests, sending responses, and shutdown |
| `include/RedisCommandHandler.h` | Declares the command handler interface |
| `src/RedisCommandHandler.cpp` | Request parsing, command dispatch, argument validation, and response formatting |
| `include/RedisDatabase.h` | Declares the database interface, storage members, synchronization, and persistence operations |
| `src/RedisDatabase.cpp` | Implements string, list, hash, expiration, and persistence operations |

The headers expose interfaces, while the source files contain the corresponding implementations.

This organization improves readability and makes it easier to understand the project one layer at a time.

## Q5. Walk me through what happens when a client executes `SET username Afzal`.

**Answer:**

First, the client sends the request over a TCP connection.

The server's `accept()` loop has already accepted the connection and created a worker thread for that client. The worker receives bytes using `recv()`.

The received data is passed to:

```cpp
cmdHandler.processCommand(request);
```

Inside the command handler, the parser converts the request into tokens:

```cpp
{"SET", "username", "Afzal"}
```

The handler normalizes the command name to uppercase and dispatches it to the `SET` implementation.

The handler then calls the database's `set()` method with the key and value. The database acquires its mutex, writes the value into the string store, and clears the key's existing expiration metadata.

After the operation succeeds, the handler constructs a simple-string response:

```text
+OK\r\n
```

Finally, the worker sends the response to the client.

One implementation-specific detail is that `SET` writes to the string store without removing a same-named entry from the list or hash stores. That is a limitation of the current storage model.

## Q6. What are the main features and what is not implemented?

**Answer:**

The current project supports:

- TCP client connections.
- Redis-style command handling.
- String, list, and hash operations.
- Key expiration.
- File-based persistence.
- Concurrent client handling through worker threads.
- Mutex-protected database operations.
- A subset of RESP request and response formats.

It does not implement the complete Redis protocol or all Redis commands. For example, its request handling does not fully support TCP stream framing or command pipelining. Its persistence format does not restore TTL deadlines, and its database uses separate stores that can contain the same key name.

Being clear about these limitations is important because protocol compatibility, durability, and concurrency behavior are separate engineering requirements.

---

# 2. C++ Design and Object-Oriented Programming

## Q7. Why did you separate the database from the command handler?

**Answer:**

The database and command handler solve different problems.

The command handler understands commands such as `SET`, `GET`, `LPUSH`, and `HSET`. It is responsible for interpreting the request and translating it into a database operation.

The database understands how data is stored, retrieved, modified, expired, and persisted. It should not need to understand the syntax of an incoming network request.

Separating them gives several benefits:

- **Separation of concerns:** Each class has a more focused responsibility.
- **Reusability:** Database methods can be called without going through the TCP server.
- **Testability:** Database behavior can be tested independently from protocol parsing.
- **Maintainability:** A change to command syntax does not necessarily require a change to the storage implementation.

For example, if I change the way `SET` arguments are parsed, I should not need to rewrite the underlying key-value storage operation.

This design is a simple application of separation of concerns and modular programming.

## Q8. Why did you use a singleton for `RedisDatabase`?

**Answer:**

The database is accessed from several parts of the application, including command handlers and the persistence thread.

The implementation uses a singleton accessor:

```cpp
RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}
```

The function-local static object provides a shared database instance. Since C++11, initialization of function-local static variables is thread-safe.

This design avoids creating a separate database object for each client worker thread. Otherwise, different workers could operate on different database instances and would not share the same stored data.

However, thread-safe initialization does not automatically make every database operation thread-safe. That is why the database also uses a mutex to protect its containers.

A singleton is convenient for this project, but it has trade-offs. It introduces global shared state, makes dependency injection harder, and can complicate isolated tests.

In a larger system, I might inject a database interface into the command handler instead of requiring every component to access a global singleton.

## Q9. Why are the database methods protected by a mutex?

**Answer:**

The server creates multiple worker threads, and all of them use the shared database instance.

Suppose two threads execute:

```text
Thread A: SET username Afzal
Thread B: DEL username
```

Both operations may modify the same underlying container. Without synchronization, concurrent reads and writes to standard C++ containers can cause data races and undefined behavior.

The database uses a mutex to serialize access to protected operations:

```cpp
std::lock_guard<std::mutex> lock(db_mutex);
```

The lock guard automatically releases the mutex when it goes out of scope, including when an exception unwinds the stack.

This provides safer access to shared containers and reduces the risk of forgetting to unlock a mutex.

However, a mutex around individual database methods does not make an entire sequence of commands atomic. If one client performs `GET` followed by `SET`, another client can execute an operation between those two commands.

## Q10. Why use `std::atomic<bool>` for the `running` flag?

**Answer:**

The server's `running` flag communicates whether the server should continue accepting clients.

The accept loop reads the flag, while shutdown changes it. Because these operations can occur from different threads, an ordinary `bool` could be accessed concurrently without synchronization.

The implementation uses:

```cpp
std::atomic<bool> running;
```

Atomic operations provide synchronized access to the flag and avoid a data race on that variable.

However, an atomic flag only protects the flag itself. It does not automatically protect `server_socket`, the global server pointer, the database, or the entire shutdown sequence.

For example, changing `running` to `false` does not necessarily interrupt a thread blocked in `accept()` or a worker blocked in `recv()`. The server still needs a deliberate shutdown strategy.

## Q11. Why use `std::vector` for lists?

**Answer:**

The current list implementation stores each list as a vector of strings.

A vector provides:

- Contiguous memory storage.
- Efficient indexed access.
- Amortized constant-time insertion at the end.
- Straightforward iteration and serialization.

These properties are useful for commands such as `LINDEX` and `LRANGE`.

However, inserting or removing an element at the beginning requires shifting the remaining elements. Therefore, `LPUSH` and `LPOP` can require O(n) time.

If workloads contain frequent operations at both ends, a deque may provide more appropriate performance characteristics. A linked list is another possibility, although it has different memory and cache-locality trade-offs.

I would select the data structure based on the required operations and measured workload rather than assuming one container is always best.

## Q12. Why use `std::chrono::steady_clock` for TTL?

**Answer:**

Expiration depends on measuring elapsed time.

`std::chrono::steady_clock` is designed to move forward steadily and is not affected by ordinary wall-clock adjustments in the way a system clock can be.

For example, if a key is given a TTL of 60 seconds, the database calculates a deadline using the current steady-clock time plus 60 seconds.

When the key is checked later, the database compares the deadline against the current steady-clock time.

This is suitable for measuring elapsed durations during one process lifetime.

The trade-off is that a steady-clock time point is not a persistent calendar timestamp. It cannot simply be written to a file and reliably reused after a restart. My current implementation therefore does not persist TTL deadlines.

---

# 3. TCP Networking and Client Communication

## Q13. Why did you use TCP sockets?

**Answer:**

TCP provides a reliable, ordered byte stream between connected endpoints. It is a natural choice for a client-server database protocol because commands and responses need to travel between the client and the server.

The server uses the socket API to:

1. Create a TCP socket.
2. Configure socket options.
3. Bind to a port.
4. Listen for connections.
5. Accept clients.
6. Receive request bytes.
7. Send responses.

TCP handles retransmission and ordering at the transport layer, but it does not define application-level command boundaries.

That last point is particularly important in this project: reliable byte delivery does not guarantee that one `recv()` call contains exactly one complete command.

## Q14. Explain `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, and `send()`.

**Answer:**

These functions provide the main operations needed for a TCP server.

| Function | Purpose |
|---|---|
| `socket()` | Creates a socket descriptor |
| `bind()` | Associates the socket with a local address and port |
| `listen()` | Marks the socket as a listening socket and establishes a pending-connection queue |
| `accept()` | Accepts a client connection and returns a new socket descriptor for that client |
| `recv()` | Receives bytes from a connected socket |
| `send()` | Transmits bytes through a connected socket |

The listening socket and client sockets have different responsibilities.

The listening socket waits for new connections. Each successful `accept()` returns a separate socket used to communicate with that particular client.

In this project, the server uses a worker thread to handle communication on each accepted client socket.

## Q15. What is the difference between the listening socket and the client socket?

**Answer:**

The listening socket is responsible for accepting new connections. It is created, bound, and placed into the listening state.

The client socket is returned by `accept()` and is used for communication with one connected client.

For example, the server may have one listening socket and several accepted client sockets:

```text
Listening socket
       |
       +---- Client socket A
       |
       +---- Client socket B
       |
       +---- Client socket C
```

The server continues accepting connections through the listening socket while worker threads communicate with clients through their respective sockets.

Closing a client socket ends that connection. Closing the listening socket affects the server's ability to accept new connections.

## Q16. What is a TCP stream, and why is it a problem for your implementation?

**Answer:**

TCP provides a continuous stream of bytes. It does not preserve the boundaries between application-level messages.

Suppose a client sends:

```text
SET username Afzal
```

The server might receive all the bytes in one `recv()` call, or it might receive part of the request first and the remaining bytes later.

Similarly, two commands sent close together might arrive in the same read.

The current server constructs a request string from each successful `recv()` result and passes that string directly to the command handler. It does not maintain a complete per-client input buffer that reconstructs commands across arbitrary read boundaries.

This means the implementation can fail when a request is fragmented across reads or when multiple requests arrive together.

A better implementation would:

1. Maintain an input buffer for each client connection.
2. Append newly received bytes to that buffer.
3. Parse complete commands from the buffer.
4. Retain incomplete data until more bytes arrive.
5. Process every complete command in order.
6. Preserve any unconsumed bytes for the next iteration.

This is one of the most important networking improvements for the project.

## Q17. Why is one thread per client useful, and what are its limitations?

**Answer:**

The current server creates a new worker thread for each accepted client.

The main accept loop can continue accepting connections while existing workers handle their clients. This is easier to understand than a more complicated event-driven design and works well as a learning implementation.

However, each thread consumes memory and operating-system resources. Large numbers of concurrent clients can create substantial scheduling overhead and thread-management costs.

A worker may also block inside `recv()` while waiting for more data, keeping the thread occupied.

For a higher-concurrency server, I would consider:

- Non-blocking sockets.
- An event loop using facilities such as `epoll` on Linux.
- A bounded worker pool.
- Explicit limits on concurrent connections.
- Backpressure and connection timeouts.

The appropriate design depends on expected traffic, latency requirements, and implementation complexity.

## Q18. What happens if `send()` does not transmit the complete response?

**Answer:**

A successful `send()` call may transmit fewer bytes than requested. The caller must inspect the returned byte count.

The current implementation calls `send()` once and does not fully handle partial writes.

A robust implementation would maintain a position within the response string and continue sending the unsent portion until the entire response has been transmitted or a terminal error occurs.

It should also handle interruptions and disconnected clients appropriately.

This matters because the command handler may generate a correct response, but the client can still receive an incomplete response if the networking layer does not transmit it fully.

---

# 4. Protocol Parsing and Command Processing

## Q19. What is RESP?

**Answer:**

RESP stands for Redis Serialization Protocol. It defines how Redis clients and servers encode requests and responses.

One commonly used request representation is an array of bulk strings.

For example, the command:

```text
SET username Afzal
```

can be represented as:

```text
*3\r\n
$3\r\n
SET\r\n
$8\r\n
username\r\n
$5\r\n
Afzal\r\n
```

Here:

- `*3` indicates an array containing three elements.
- `$3` declares a bulk string containing three bytes.
- `SET` is the first element.
- `$8` and `$5` declare the lengths of the key and value.
- `\r\n` marks protocol delimiters.

Length information allows values to contain spaces and other characters that would be ambiguous in simple whitespace-separated input.

My parser supports RESP arrays of bulk strings, but it does not implement the complete RESP protocol.

## Q20. How does your parser convert a request into a command?

**Answer:**

The parser, `parseRespCommands()`, converts the incoming request into a vector of strings.

For RESP input, it reads the array element count, reads each bulk-string length, extracts the declared number of bytes, and validates the expected delimiters and input boundaries.

It rejects malformed requests and unexpected trailing data after the parsed request.

The parser also supports a whitespace-separated fallback, which is useful for simple manual testing.

After parsing, a request such as:

```text
SET username Afzal
```

becomes:

```cpp
{"SET", "username", "Afzal"}
```

The command handler then uses the first token as the command name and the remaining tokens as arguments.

The parser does not execute database operations. Keeping parsing separate from command execution helps isolate protocol concerns from database logic.

## Q21. Why normalize command names to uppercase?

**Answer:**

The handler converts the command name to uppercase before dispatching it.

This allows inputs such as:

```text
SET username Afzal
set username Afzal
Set username Afzal
```

to resolve to the same command.

It avoids duplicating command-handling logic for different letter cases.

Only the command name is normalized. Keys, values, and hash fields are not automatically converted to uppercase.

## Q22. How does command dispatch work?

**Answer:**

After parsing, the handler identifies the command using the normalized command name.

The current implementation uses conditional dispatch logic to select the appropriate command branch. Each branch validates its arguments, calls the relevant database method, and constructs a response.

For example, the `GET` branch retrieves a string value from the database and encodes the result into the response format expected by the client.

An unknown command reaches the handler's error path.

This approach is straightforward for a relatively small command set. If the number of commands grows significantly, a dispatch table or registry of command handlers could reduce repeated conditional logic and make the code easier to extend.

## Q23. What is the difference between simple strings, bulk strings, arrays, integers, and errors in RESP-style responses?

**Answer:**

These response types communicate different kinds of results.

| Prefix | Response type | Typical use |
|---|---|---|
| `+` | Simple string | Success status or short textual result |
| `-` | Error | Command or protocol error |
| `:` | Integer | Counts or numeric results |
| `$` | Bulk string | String value or nullable string |
| `*` | Array | Collection of response elements |

For example:

```text
+OK\r\n
```

is a simple success response, while:

```text
:1\r\n
```

is an integer response.

A bulk string contains an explicit length, and arrays contain a count followed by encoded elements.

The current implementation supports several of these response formats. Exact encoding and missing-value behavior must be checked in the relevant command handler.

## Q24. What is command pipelining, and does your server support it?

**Answer:**

Command pipelining allows a client to send multiple commands without waiting for a response after each one.

For example, a client could send several independent commands over one connection and then read their responses in sequence.

Pipelining can reduce round-trip waiting time, but the server must correctly identify every command boundary and generate the corresponding responses in order.

My current parser expects a single supported request representation and rejects unexpected trailing data. Combined with the server's one-read/one-request approach, this means complete Redis-style pipelining is not implemented.

To support it, I would first implement buffered TCP stream parsing. The server would then repeatedly extract complete commands from the connection buffer and process them in order.

---

# 5. Database Design and Data Structures

## Q25. How is data stored in your database?

**Answer:**

The database maintains three separate stores:

- `kv_store` for strings.
- `list_store` for lists of strings.
- `hash_store` for hashes containing field-value pairs.

It also maintains an `expiry_map` that associates key names with expiration deadlines.

The separation makes each data structure straightforward to implement and lets the database expose operations specific to strings, lists, and hashes.

However, the stores are not mutually exclusive. The same key name can exist in more than one store because operations such as `SET` do not consistently remove entries from other stores.

A more Redis-like design would use one top-level keyspace in which every key has exactly one data type and an associated value object.

That design would make type consistency easier to enforce and simplify operations such as `KEYS`, `TYPE`, deletion, and expiration.

## Q26. Why does your database need an expiration map?

**Answer:**

The data stores hold actual values, while the expiration map holds expiration metadata.

For example, the database might contain:

```text
kv_store:
    session -> abc123

expiry_map:
    session -> expiration deadline
```

This separation allows the database to determine whether a key should still be accessible without embedding TTL metadata separately into every value representation.

When a key expires, the cleanup logic removes the key from the data stores and removes its expiration entry.

The current design is simple, but the expiration map is keyed only by key name. Because the database uses separate data stores, same-named entries in different stores share the same expiration metadata.

A unified keyspace with one type-tagged value per key would make the relationship between data and TTL easier to manage.

## Q27. Why does `keys()` remove expired keys before returning the result?

**Answer:**

An expired key should not normally be exposed as a valid key.

The current `keys()` method acquires the database mutex and purges expired entries across the stores before collecting key names.

This is an example of lazy expiration: the database does not necessarily run a dedicated cleanup operation for every key at the exact moment its TTL expires. Instead, certain operations trigger cleanup.

The benefit is simplicity. The database does not need to maintain a separate timer for every key.

The trade-off is that expired entries may remain physically present until cleanup occurs. Operations that do not check expiration can therefore observe stale data.

## Q28. How does deletion work across your data stores?

**Answer:**

The database has a helper called `removeKeyFromAllStoresLocked()`.

Its responsibility is to remove a key from the string, list, and hash stores and erase its expiration metadata.

The helper assumes the caller already holds the database mutex, which avoids acquiring the same non-recursive mutex a second time.

This centralizes deletion behavior and helps prevent stale data from remaining in another store after a key is removed.

However, the database's `del()` method checks whether the key exists before removing it and does not first purge an expired key. Therefore, its result can differ from the result that would be obtained after expiration cleanup.

The helper also illustrates a useful implementation rule: functions with names ending in `Locked` should have clearly documented locking preconditions.

## Q29. How are hash operations implemented?

**Answer:**

Each hash is represented as a mapping from field names to values.

For example:

```text
user:
    name -> Afzal
    role -> developer
```

The `HSET` operation creates a hash if necessary, then inserts or updates the requested field. It returns a result indicating whether the field was newly created.

The retrieval operations look up a field or enumerate the hash's fields and values. `HDEL` removes fields and deletes the hash key when the last field is removed.

`HMSET` updates multiple field-value pairs under one database mutex acquisition. The implementation does this directly rather than repeatedly calling the individually locking `hash_set()` method, which would risk deadlocking on the same mutex.

The database uses map-based storage, so enumeration order follows the container's iteration behavior rather than necessarily matching insertion order.

## Q30. What is the difference between `LPUSH` and `RPUSH` in your implementation?

**Answer:**

Both commands insert values into a list, but they use different ends of the vector.

`LPUSH` inserts at the beginning. When multiple values are supplied, the handler inserts them one at a time in the supplied order, so the final order of those newly inserted elements is reversed.

`RPUSH` appends values to the end in the supplied order.

For a new list:

```text
LPUSH tasks first second third
```

produces:

```text
third, second, first
```

whereas:

```text
RPUSH tasks first second third
```

produces:

```text
first, second, third
```

The implementation detail matters because the command's behavior depends on both the command name and how multiple arguments are processed.

---

# 6. Concurrency and Thread Safety

## Q31. How does your server handle multiple clients at the same time?

**Answer:**

The main server thread runs an accept loop. When a client connects, `accept()` returns a client socket, and the server creates a worker thread to handle that connection.

The worker repeatedly receives request bytes, processes each received buffer, sends the resulting response, and closes the client socket when its loop ends.

The database is shared between worker threads, so database operations use a mutex.

There is also a separate detached thread that periodically saves the database.

This model supports concurrent clients, but it is not an unlimited scaling strategy. Creating a thread for every connection can become expensive, and workers blocked on client reads can occupy resources.

## Q32. Is your database thread-safe?

**Answer:**

The database protects its main data-store operations using a mutex, which prevents simultaneous unsynchronized access to the protected containers.

For example, a `GET` operation and a `SET` operation cannot execute their protected container access simultaneously while using the same mutex.

However, saying the entire database is completely thread-safe requires examining all shared state and every operation. The server's socket descriptor, global server pointer, shutdown lifecycle, and worker-thread management have separate synchronization concerns.

Additionally, thread safety is not the same as transaction support. Individual methods can be protected while a multi-command sequence remains interleavable.

My current implementation provides mutex-protected database operations, but it does not provide transactions or comprehensive synchronization for every part of the server lifecycle.

## Q33. What is a data race?

**Answer:**

A data race occurs when multiple threads access the same memory location concurrently, at least one access is a write, and the accesses are not properly synchronized.

In C++, a data race on ordinary shared memory causes undefined behavior.

For example, if two threads modify a standard map concurrently without synchronization, the program may behave unpredictably or corrupt its state.

The database mutex helps prevent this for protected database operations.

An atomic variable can protect an individual flag, but it does not automatically protect unrelated variables. This is why the `running` flag being atomic does not make the socket descriptor or global server pointer safe by itself.

## Q34. What is the difference between a mutex and an atomic variable?

**Answer:**

A mutex provides mutual exclusion for a critical section. A thread acquires the mutex before accessing protected state, and other threads attempting to acquire it must wait until it becomes available.

An atomic variable provides indivisible operations on one atomic object. It is useful for shared counters, flags, and other values that can be manipulated through atomic operations.

In this project:

- `std::mutex` protects the database containers and associated operations.
- `std::atomic<bool>` is used for the server's `running` flag.

An atomic flag is not a replacement for a mutex when an operation must safely update several related data structures together.

For example, removing a key from three stores and an expiration map requires coordinated access to those structures, which is why the database uses a mutex.

## Q35. What is a deadlock, and how does your project avoid one in `HMSET`?

**Answer:**

A deadlock occurs when threads become stuck waiting for resources held by one another, or when a thread attempts to acquire a non-recursive mutex it already holds.

Consider a database method that locks `db_mutex` and then calls another method that also attempts to lock `db_mutex`. With an ordinary non-recursive mutex, the second acquisition can block indefinitely.

The `HMSET` implementation avoids this by applying all field updates directly while holding one lock instead of repeatedly calling the individually locking `hash_set()` method.

This is a useful pattern for implementing batch operations: define a clear locking boundary and avoid nested acquisitions of the same non-recursive mutex.

## Q36. What happens when the persistence thread and client workers access the database simultaneously?

**Answer:**

The periodic persistence thread calls `dump()` to serialize the database. The `dump()` method acquires the same database mutex used by database operations.

Consequently, a dump cannot iterate over the protected containers simultaneously with another operation holding that mutex.

This protects the serialization process from unsynchronized container access.

However, it also means that persistence can block client operations while the dump holds the lock. For a large database, this may increase request latency.

A more advanced design could take a consistent snapshot under the lock, release the lock, and serialize the snapshot outside the critical section. That reduces lock duration at the cost of additional memory and snapshot-management complexity.

## Q37. What problems exist in the current shutdown design?

**Answer:**

The shutdown method sets `running` to false, attempts to persist the database, closes the listening socket, and prints completion messages.

There are several limitations:

- The listening socket is not fully synchronized with all concurrent access.
- A worker thread may remain blocked in `recv()` while a client connection remains open.
- The server joins worker threads after leaving the accept loop, so shutdown can wait for workers that have not finished.
- The signal handler performs operations that are not generally safe in a POSIX signal-handler context.

A more robust design would separate shutdown notification from cleanup.

For example, a signal handler could set a suitable signal-safe flag or notify the normal execution flow. The main thread could then stop accepting connections, close or shut down client connections in a coordinated way, join workers, persist data, and release resources.

---

# 7. TTL and Expiration

## Q38. How does the TTL mechanism work?

**Answer:**

TTL stands for time to live. It determines how long a key remains valid after an expiration is configured.

When the `EXPIRE` command receives a positive number of seconds, the database computes an expiration deadline using `std::chrono::steady_clock` and stores it in `expiry_map`.

For example:

```text
SET session abc123
EXPIRE session 60
```

The database records a deadline approximately 60 seconds after the expiration operation.

When a cleanup-triggering operation checks the key, the database compares the deadline against the current steady-clock time. If the deadline has passed, it removes the key from the data stores and expiration metadata.

A zero or negative TTL causes immediate removal in the current implementation.

## Q39. What is lazy expiration?

**Answer:**

Lazy expiration means the database checks whether a key has expired when an operation accesses it, instead of requiring a timer to delete every key at its exact expiration time.

The current project uses this approach for some operations.

For example, `get()` purges the requested key if it has expired, while `keys()` performs a cleanup pass over all stores before listing keys.

This approach is relatively simple because it does not require scheduling a separate timer for each key.

The drawback is that an expired key can remain physically stored if no operation triggers cleanup. Since not every list and hash operation checks expiration, the implementation can expose inconsistent behavior across command types.

## Q40. Does every command check expiration?

**Answer:**

No. Expiration cleanup is not consistently performed by every database method.

The reviewed implementation includes the following behavior:

| Operation | Expiration behavior |
|---|---|
| `get()` | Purges the requested key if expired |
| `type()` | Purges the requested key if expired |
| `keys()` | Purges expired keys across the stores |
| `expire()` | Purges the requested key before checking existence |
| `set()` | Clears the key's expiration metadata |
| `del()` | Does not purge the requested key first |
| `rename()` | Does not purge the source key first |
| List operations | Generally do not independently purge expired keys |
| Hash operations | Generally do not independently purge expired keys |
| `dump()` | Does not perform a full expiration purge |

The distinction matters because expiration metadata and physical deletion are not identical. A deadline can pass before the corresponding entry is removed from storage.

A future improvement would centralize key validation so all relevant operations enforce expiration consistently.

## Q41. Why are TTLs not persisted?

**Answer:**

The current dump format stores string, list, and hash records, but it does not store expiration deadlines.

The expiration map is cleared when the database is loaded. The saved file therefore does not contain enough information to restore a key's remaining lifetime.

As a result, data that was saved before its TTL expired can be loaded after a restart without the original expiration.

For example, suppose a key has 30 seconds remaining when the server saves the database. If the server restarts after 30 seconds, the restored key does not automatically expire based on that original deadline because the TTL was not saved.

A future persistence format could store an absolute expiration timestamp and discard keys that have already expired during recovery. Alternatively, it could store a remaining TTL, although interpreting that value across downtime requires a clear policy.

---

# 8. Persistence and Recovery

## Q42. How does persistence work in your project?

**Answer:**

The project uses a custom file-based persistence mechanism.

At startup, `main.cpp` attempts to load `dump.my_rdb` before starting the server.

A separate background thread periodically calls `dump()` every 300 seconds, or five minutes.

The shutdown method also attempts to save the database, and the server's `run()` method performs another dump after the accept loop and worker-thread joins complete.

The `dump()` method serializes strings, lists, and hashes into records identified by markers such as `K`, `L`, and `H`.

This approach allows the project to restore stored data after restarting, but it is a simplified persistence mechanism rather than a fully crash-safe database storage engine.

## Q43. Explain the dump file format.

**Answer:**

The current persistence implementation writes different record types for different data structures.

- `K` records represent string key-value pairs.
- `L` records represent list data.
- `H` records represent hash data.

Hash fields and values are combined using a colon separator. The loader splits the stored field-value representation at the first colon.

The format is whitespace-delimited, so values or fields containing whitespace may not be represented safely without additional encoding.

The use of a colon also creates ambiguity when the format cannot distinguish a delimiter from a literal colon in the stored data.

A more robust format would use explicit length prefixes, a binary encoding, or another well-defined serialization scheme that supports arbitrary string contents.

## Q44. Is your persistence mechanism crash-safe?

**Answer:**

Not fully.

The current implementation writes directly to the dump file. It does not comprehensively check every write operation or guarantee that a complete previous snapshot remains available if the process or system fails during a write.

A process crash during serialization could leave a truncated or partially written dump.

A more robust approach would be:

1. Create a consistent database snapshot.
2. Serialize it into a temporary file.
3. Verify that all writes and the flush succeed.
4. Use the appropriate operating-system durability operations when required.
5. Atomically replace the previous snapshot with the completed file.
6. Validate the file during recovery.

The exact durability guarantees depend on the filesystem, operating system, and synchronization strategy. An atomic rename alone does not guarantee that all file contents have been durably committed to storage.

## Q45. What happens if the dump file is missing?

**Answer:**

The startup code attempts to load `dump.my_rdb`.

If loading fails because the file cannot be opened, the program logs that no dump was found or loading failed and continues toward starting the server.

The intended behavior is to allow the server to start with an empty database when no previous dump is available.

However, the message combines two different cases: a missing file and a genuine loading failure. The current startup logic does not clearly distinguish those situations in its output.

A future improvement would report missing files separately from malformed data, permission failures, and other I/O errors.

## Q46. What is the difference between persistence and replication?

**Answer:**

Persistence preserves data across process restarts or system failures by storing it on durable storage.

Replication maintains copies of data across multiple server instances.

My project implements a simplified form of persistence through a local dump file. It does not implement replication.

Replication introduces additional problems, including network communication between nodes, consistency, leader or primary selection, synchronization after disconnection, and failure recovery.

Persistence and replication can complement each other, but they solve different problems.

---

# 9. Error Handling and Reliability

## Q47. How does the server handle an unknown command?

**Answer:**

The command handler normalizes the command name and checks whether it matches a supported command.

If no branch recognizes the command, the handler returns an error response.

This prevents unsupported commands from being silently treated as valid database operations.

However, the exact error string and response encoding should be verified against the current source before being quoted as an exact implementation guarantee.

## Q48. How does the server handle socket errors?

**Answer:**

The server checks several important socket operations, including socket creation, binding, listening, accepting connections, and receiving data.

Failures during socket setup are logged and cause the server's run method to return.

If `accept()` fails, the server logs the error when it is still running and exits the accept loop.

The worker exits its receive loop when `recv()` returns zero or a negative result.

The current implementation does not handle every failure comprehensively. For example, `setsockopt()` results are ignored, partial sends are not handled, and some early socket-setup error paths may leave a descriptor open.

A more robust implementation would use consistent resource cleanup, explicit error classification, and retry behavior where appropriate.

## Q49. What is RAII, and how could it improve your server?

**Answer:**

RAII stands for Resource Acquisition Is Initialization. It is a C++ technique in which an object's lifetime controls the lifetime of a resource.

For example, a lock guard acquires a mutex in its constructor and releases it in its destructor. This makes the lock's lifetime follow the scope of the guard.

The same principle can be applied to socket descriptors through an owning wrapper whose destructor closes the descriptor.

In the current server, socket cleanup is performed manually. If an error causes an early return, it can be easy to forget to close a descriptor.

An RAII socket wrapper would make cleanup more reliable because the descriptor would be released when the wrapper is destroyed.

RAII is particularly useful for resources such as sockets, files, locks, and dynamically managed memory.

---

# 10. Performance and Complexity

## Q50. What is the time complexity of the main operations?

**Answer:**

The exact complexity depends on the underlying container, the number of elements, and whether cleanup or serialization is required.

The following table gives the general costs of the operations in the current design. Hash-map and map lookup costs assume their normal average or container-specific behavior.

| Operation | Approximate complexity |
|---|---|
| String lookup using a hash map | Average O(1) |
| String insertion using a hash map | Average O(1) |
| Hash field lookup using a map | O(log h) |
| Hash field insertion using a map | O(log h) |
| List indexed access using a vector | O(1) |
| `LPUSH` using a vector | O(n) per insertion |
| `RPUSH` using a vector | Amortized O(1) per append |
| `LPOP` using a vector | O(n) |
| `RPOP` using a vector | O(1) |
| List range retrieval | O(k) after index handling, where k is the returned range size |
| List removal by value | O(n) |
| Listing all keys | O(N), excluding any additional work needed for expiration cleanup |
| Dumping the database | O(N) in the amount of stored data, plus serialization and I/O costs |

Here, `n` is the number of list elements, `h` is the number of fields in a hash, `k` is the number of returned list elements, and `N` is the amount of data processed.

These are operation-level estimates. The database mutex can also affect observed latency because operations may need to wait for other operations to release the lock.

## Q51. What is the main performance bottleneck in your current design?

**Answer:**

There is no single bottleneck for every workload, but several design choices can limit performance.

First, the database uses a shared mutex. A long operation such as a full database dump can hold the lock while client operations wait.

Second, list operations at the beginning of a vector require shifting elements. Workloads with frequent `LPUSH` and `LPOP` operations can therefore incur linear-time work.

Third, the server creates a thread for each client. Large numbers of concurrent connections can increase thread-management and scheduling overhead.

Fourth, the persistence thread periodically serializes the database. Its I/O and lock duration may increase latency as the amount of data grows.

I would measure operation latency, lock wait time, CPU usage, memory consumption, and concurrent-connection behavior before deciding which improvement should come first.

## Q52. How would you improve performance for a large number of concurrent clients?

**Answer:**

I would first establish the workload and measure the existing implementation. Then I would consider improvements in stages.

1. **Fix protocol framing.** A buffered parser would make request processing correct for fragmented and combined TCP messages.
2. **Improve connection handling.** A bounded worker pool or non-blocking event loop could avoid creating an unbounded number of threads.
3. **Reduce lock duration.** Snapshot-based persistence could serialize data outside the main database critical section.
4. **Choose appropriate data structures.** A deque could improve list operations at both ends.
5. **Improve expiration management.** A more systematic cleanup strategy could reduce stale entries and inconsistent behavior.
6. **Add profiling and benchmarks.** This would show whether the actual bottleneck is networking, parsing, lock contention, storage, or file I/O.

I would avoid introducing complexity such as lock-free structures before measurements demonstrate a need.

---

# 11. Limitations and Future Improvements

## Q53. What are the biggest limitations of your project?

**Answer:**

The main limitations are:

1. **Incomplete protocol handling:** The parser supports a subset of RESP, but the server does not correctly reconstruct arbitrary commands across TCP reads or support full command pipelining.
2. **Thread-per-client model:** Each client gets a worker thread, which may become expensive at high concurrency.
3. **Separate stores for data types:** A key name can exist in more than one store.
4. **Inconsistent expiration cleanup:** Not every operation checks whether a key has expired.
5. **Limited persistence guarantees:** TTLs are not persisted, and the dump format does not comprehensively handle write failures or arbitrary string contents.
6. **Shutdown limitations:** Worker threads can remain blocked on client reads, and the signal handler performs operations that are not generally safe in that context.
7. **Incomplete socket-write handling:** Partial sends are not handled robustly.

These limitations do not make the project useless. They identify the engineering problems that become visible when moving from a simple implementation toward a reliable server.

## Q54. If you had more time, what would you improve first?

**Answer:**

My first priority would be correctness at the protocol and lifecycle boundaries.

I would start with buffered TCP framing so incomplete requests are retained and multiple complete requests can be processed correctly. Without that, even a correct command implementation can fail when the network delivers bytes in unexpected chunks.

Next, I would unify the keyspace so every key has exactly one data type and associated expiration metadata.

I would then improve persistence by introducing a well-defined serialization format, comprehensive I/O error checks, and safer snapshot replacement.

After that, I would address worker-thread scaling and coordinated shutdown.

Finally, I would add automated tests and benchmarks so future changes could be evaluated systematically.

This order prioritizes correctness and reliability before optimization.

## Q55. How would you make the project more production-ready?

**Answer:**

A production-oriented version would need substantially stronger guarantees in several areas.

- **Protocol correctness:** Complete buffered framing, explicit request-size limits, robust parsing, and well-defined error responses.
- **Data consistency:** One authoritative keyspace with consistent data types and TTL behavior.
- **Durability:** A validated persistence format, safe snapshot writing, and clearly documented crash-recovery guarantees.
- **Concurrency:** Bounded resource usage, a coordinated shutdown mechanism, and reduced lock contention.
- **Security:** Authentication and authorization where required, request limits, safe binding configuration, and protection against resource-exhaustion attacks.
- **Reliability:** Automated tests, fuzz testing for the parser, error-path testing, and clear operational logging.
- **Observability:** Metrics for request counts, latency, errors, active connections, and persistence duration.
- **Compatibility:** A defined protocol and command-compatibility target, backed by automated tests.

I would not claim that the current project already provides these features. They are the areas I would address before presenting it as a production database service.

---

# 12. Advanced Interview Follow-Ups

## Q56. Why doesn't a mutex make the whole database transactional?

**Answer:**

The mutex protects individual critical sections, not necessarily a sequence of separate commands.

Suppose a client executes:

```text
GET counter
SET counter 11
```

Another client can execute a command between those two operations because the first command releases the mutex before the second command begins.

The result is that the pair of commands is not one atomic transaction.

To support transactions, the system would need an explicit mechanism that groups operations into one unit with defined isolation semantics. That might involve a transaction lock, command queue, version checking, or another concurrency-control strategy.

Transactions also need rules for failure and rollback if multiple operations are supposed to succeed or fail together.

## Q57. Why can your database return duplicate names from `KEYS`?

**Answer:**

The database stores strings, lists, and hashes in separate containers.

The `keys()` method iterates over the containers and appends their key names to the result. It does not necessarily deduplicate names that appear in multiple stores.

For example, if the string store and list store both contain the name `user`, the output can contain that name twice.

This behavior follows from the current storage model. A unified keyspace would make each key name correspond to one data-type entry and avoid this particular source of duplicates.

## Q58. What happens if `SET` is called on a key that already exists as a list?

**Answer:**

The current `SET` implementation writes the value into the string store and removes the key's expiration metadata. It does not remove the same-named entry from the list or hash store.

Therefore, the key name can exist in both the string and list stores.

Because `TYPE` checks the string store first, it may report `string` even though a list entry with the same name also exists.

This is a limitation of using independent stores without a single authoritative keyspace.

A more Redis-like design would replace the previous value and data type when setting a key to a different type, following the database's defined type rules.

## Q59. Why does `dump()` holding the database mutex affect latency?

**Answer:**

The dump operation must read the stored data while preventing concurrent modifications from invalidating the serialization process.

The current implementation locks the database mutex during dumping. While that lock is held, other database operations that require the same mutex must wait.

As the database grows, serialization can take longer, increasing the period during which client operations are blocked.

One improvement would be to create a consistent snapshot while holding the lock, then release the lock and serialize that snapshot to disk. This reduces the time other operations must wait, although snapshot creation has its own memory and processing costs.

## Q60. What is the difference between logical expiration and physical deletion?

**Answer:**

Logical expiration means a key should no longer be treated as valid because its TTL deadline has passed.

Physical deletion means its data and associated metadata have actually been removed from the database's containers.

In a lazy-expiration design, these events may occur at different times.

A key can be logically expired but physically present until a cleanup operation removes it.

This distinction explains why a database needs a consistent expiration policy across reads, writes, listings, persistence, and deletion operations.

## Q61. Why is a custom persistence file format a design challenge?

**Answer:**

A serialization format must preserve enough information to reconstruct the original values without ambiguity.

The current format uses record markers and whitespace-delimited values. Hash field-value pairs use a colon separator.

These choices are simple, but they create problems when keys, fields, or values contain delimiters or whitespace. The format also does not preserve expiration deadlines.

A more robust design could use explicit length-prefixed fields, a well-defined binary representation, or a serialization format that safely represents arbitrary strings.

The loader should validate record lengths, required fields, record types, and malformed data before trusting the file.

Persistence design is not only about writing bytes; it is about ensuring that recovery produces a correct database state.

## Q62. Why is calling complex operations from a signal handler unsafe?

**Answer:**

POSIX signal handlers execute in a restricted environment. Many ordinary library functions are not async-signal-safe.

The current handler calls shutdown logic that can log messages, acquire mutexes, write a persistence file, and close a socket. If a signal interrupts a thread while it is executing related library or locking code, invoking those operations from the handler can cause deadlocks or undefined behavior.

A safer approach is to make the handler do as little as possible, such as setting a suitable signal-safe flag. The main execution flow can then perform cleanup, persistence, and thread coordination in normal program context.

This separation improves shutdown reliability.

## Q63. How would you test the parser beyond ordinary unit tests?

**Answer:**

I would test both expected requests and malformed or adversarial inputs.

Examples include:

- Empty input.
- Unknown commands.
- Missing CRLF delimiters.
- Invalid array counts.
- Invalid bulk-string lengths.
- Declared lengths larger than the available input.
- Trailing bytes after a complete request.
- Arguments containing spaces and special characters.
- Incomplete commands split across separate TCP reads.
- Multiple commands delivered in one read.
- Very large input lengths and resource-exhaustion cases.

I would also consider fuzz testing, which automatically generates many inputs to find crashes, hangs, and unexpected parser behavior.

Parser tests should verify not only that valid input is accepted, but also that malformed input is rejected safely without reading beyond the buffer or corrupting application state.

## Q64. How would you explain the difference between an educational implementation and a production database?

**Answer:**

An educational implementation focuses on learning core concepts and demonstrating a working design. A production database must also provide clearly defined correctness, durability, security, scalability, and operational guarantees.

My project demonstrates TCP server design, command parsing, in-memory data structures, synchronization, expiration, and basic persistence.

However, it does not yet provide complete Redis protocol compatibility, robust request framing, comprehensive crash-safe persistence, consistent expiration across every operation, or a scalable event-driven networking model.

Recognizing these gaps is part of understanding the system. The next step is not merely to add more commands, but to strengthen the guarantees around the features that already exist.

---

# 13. Final Project Explanation

## Q65. Explain the project in approximately two minutes.

**Answer:**

I developed a Redis-inspired in-memory database server using C++. I built it to understand how database servers handle networking, command parsing, storage, concurrency, expiration, and persistence internally.

The project uses a layered architecture. The `RedisServer` class manages TCP connections and client communication. `RedisCommandHandler` parses incoming requests, validates commands, dispatches operations, and generates Redis-style responses. `RedisDatabase` manages the underlying strings, lists, hashes, expiration metadata, and persistence operations.

For concurrency, the server creates a worker thread for each client, while the database uses a mutex to protect its shared data structures. I also implemented TTL-based expiration using `std::chrono::steady_clock` and a custom file-based persistence mechanism that loads a dump during startup and periodically saves database state.

The project supports a subset of Redis-style commands and RESP formats. During development, I learned that a server requires more than correct data structures: TCP stream framing, partial writes, synchronization, persistence recovery, and graceful shutdown are also important.

The implementation has limitations, including incomplete TCP request framing, a thread-per-client architecture, and persistence that does not preserve TTLs. Those limitations give me a concrete roadmap for improving the system further.

## Q66. What did you learn from this project?

**Answer:**

The biggest lesson was understanding how several systems concepts interact in a real application.

I learned that TCP is a byte stream rather than a message-oriented transport, so the application must implement its own request framing.

I learned how shared state introduces concurrency problems and why mutexes must protect related operations consistently.

I also learned that expiration and persistence are not isolated features. Their interaction affects whether data is visible, whether it can be recovered after restart, and whether a saved snapshot accurately represents the database.

Finally, I learned to evaluate an implementation by its guarantees and edge cases, not just by whether a demonstration works.

## Q67. What would you say if an interviewer identifies a limitation in your implementation?

**Answer:**

I would acknowledge the limitation, explain the current behavior, and describe a concrete improvement.

For example, if an interviewer points out that TCP reads do not correspond to complete commands, I would explain that the current server treats each `recv()` result as one request. That is a known limitation because TCP preserves byte order but not application-level message boundaries.

I would then describe how I would introduce a per-client input buffer, append incoming bytes, parse complete commands, and retain incomplete data until more bytes arrive.

I would not claim that the current implementation already handles that case correctly.

Being able to explain a limitation accurately—and reason through a practical solution—is a stronger demonstration of engineering understanding than overstating the system's capabilities.

---

# Final Revision Checklist

Before an interview, make sure you can answer these questions without memorizing every sentence.

- [ ] Can I explain the full request lifecycle from client to database and back?
- [ ] Can I explain the responsibility of every major source file?
- [ ] Can I walk through `SET`, `GET`, `LPUSH`, and `HSET` in the code?
- [ ] Can I explain the parser's supported RESP format and its limitations?
- [ ] Can I explain why TCP request framing is necessary?
- [ ] Can I explain why the server uses worker threads and why the database uses a mutex?
- [ ] Can I explain the difference between thread safety and transaction atomicity?
- [ ] Can I explain the TTL implementation and which operations trigger expiration cleanup?
- [ ] Can I explain the persistence format, startup recovery, and missing TTL persistence?
- [ ] Can I discuss the complexity of vector-based list operations?
- [ ] Can I identify the major correctness and reliability limitations?
- [ ] Can I propose realistic improvements and explain their trade-offs?
- [ ] Can I describe the project accurately without claiming full Redis compatibility?

**The goal is understanding, not memorization.** Read each answer, locate the corresponding code, and practise explaining it in your own words. The strongest project interviews connect every explanation to a concrete implementation decision, a real limitation, or a trade-off in the source code.
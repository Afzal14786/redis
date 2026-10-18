# Learning Notes

This file contains the concepts I have learned, explored, and used while building my Redis-like server in C++. I created this file to keep track of the things I learn during development, understand why they are needed, and have a place to revise them later.

These notes are written from my current understanding. Some topics are concepts I can explain confidently, while others are things I have learned enough to use but still want to explore further. I will keep updating this file as the project grows.

---

## Table of Contents

1. C++ String Conversion
2. C++ Standard Library and Containers
3. Header Files and Project Structure
4. Constructors, Access Specifiers, and Static Members
5. Exceptions and Input Validation
6. Linux, POSIX, and File Descriptors
7. Socket Programming
8. TCP and Client-Server Communication
9. Threads and Concurrency
10. Mutexes, Atomics, and Shared Data
11. Data Structures Used in the Database
12. Command Parsing and RESP
13. Redis-Like Commands and Responses
14. Time-to-Live (TTL) and Expiration
15. File I/O and Persistence
16. Error Handling and Debugging
17. Build System and Makefile
18. Performance and Time Complexity
19. What I Still Want to Improve

---

## 1. C++ String Conversion

### What is `std::stoi()`?

`std::stoi()` is a function from the C++ standard library that converts a string into an integer. It is declared in the `<string>` header.

I used it in `main.cpp` to convert the port number supplied through the command line into an integer.

```cpp
std::stoi(argv[1]);
```

For example:

```cpp
std::string value = "6378";
int port = std::stoi(value);
```

Here, the string `"6378"` is converted into the integer `6378`. This is useful because command-line arguments are received as strings, but the server needs the port as a numeric value.

### How does `std::stoi()` work?

- It skips leading whitespace.
- It reads the numeric portion from left to right.
- It stops when it reaches a character that cannot be part of the number.
- It supports bases from 2 to 36, with base 10 as the default.
- It can throw an exception if the string cannot be converted or if the result is outside the range of an `int`.

One important detail is that `std::stoi("123abc")` can return `123`, because it stops at the first invalid character. If I need to make sure the entire string is valid, I can use the optional position argument and check whether all characters were consumed.

```cpp
std::size_t position = 0;
int port = std::stoi(argv[1], &position);

if (position != std::string(argv[1]).size()) {
    // The input contains extra characters.
}
```

I also need to validate that the resulting port is within the valid TCP port range before using it.

### What is `std::stod()`?

`std::stod()` converts a string into a `double`, which is a floating-point numeric type.

```cpp
double value = std::stod("12.75");
```

The name is commonly understood as “string to double.” It is declared in `<string>`. I have come across it while learning string conversion, although my server's port argument uses `std::stoi()` because a port is an integer, not a decimal value.

### Difference between `std::stoi()` and `std::stod()`

| Function | Converts to | Example |
|---|---|---|
| `std::stoi()` | `int` | `"6378"` → `6378` |
| `std::stod()` | `double` | `"12.75"` → `12.75` |

Both functions can throw exceptions when conversion fails or the value is out of range. Both can also stop at the first character that is not part of the numeric value, so validating the entire input is important when accepting user-provided values.

---

## 2. C++ Standard Library and Containers

The C++ standard library provides ready-made types and functions so I do not need to implement every common operation from scratch.

### `std::string`

I use `std::string` to store command names, keys, values, client requests, and responses. It is more convenient than manually managing character arrays for most string operations.

In the server, I also create a string from the exact number of bytes returned by `recv()`:

```cpp
std::string request(buffer, bytes);
```

This matters because network data is received as a number of bytes, and I should use the number of bytes actually received rather than assume the buffer is a complete null-terminated message.

### `std::vector`

A vector is a dynamic array that can grow or shrink. I use vectors for command arguments, lists stored in the database, and the collection of client threads.

```cpp
std::vector<std::string> values;
values.push_back("hello");
```

A vector provides efficient access by index. Appending to the end is usually amortized O(1), while inserting or removing at the beginning is O(n), because other elements may need to move.

### `std::map` and `std::unordered_map`

Associative containers store values associated with keys. They are useful for implementing database stores and tracking expiration deadlines.

- `std::map` keeps keys ordered and typically provides O(log n) lookup, insertion, and deletion.
- `std::unordered_map` uses hashing and provides average-case O(1) lookup, insertion, and deletion, although worst-case operations can be O(n).

The exact performance and ordering depend on which container is used in the relevant part of the implementation.

### Iterators and common operations

Some container operations I have encountered include:

- `find()` — searches for a key or element.
- `erase()` — removes an element.
- `insert()` — inserts an element.
- `push_back()` — adds an element to the end of a vector.
- `begin()` and `end()` — provide iterator positions used to traverse a container.

Understanding iterators is important because database operations often need to find an item, check whether it exists, and then update or remove it.

### `auto`, references, pointers, and `const`

- `auto` lets the compiler determine a variable's type from its initializer.
- A reference (`&`) is another name for an existing object.
- A pointer (`*`) stores an object's address and can be used to access it indirectly.
- `const` indicates that a value or object should not be modified through that particular name or access path.

I need to pay attention to references and pointers when passing objects to threads or sharing objects between functions. For example, the server's worker thread captures the shared command handler by reference, so the handler must remain alive until the workers are finished.

---

## 3. Header Files and Project Structure

I separated the project into header files (`.h`) and implementation files (`.cpp`) to keep declarations and implementations organized.

```markdown
include/
├── RedisCommandHandler.h
├── RedisDatabase.h
└── RedisServer.h

src/
├── main.cpp
├── RedisCommandHandler.cpp
├── RedisDatabase.cpp
└── RedisServer.cpp
```

### What does `#include` do?

The `#include` directive brings declarations from another header into the current source file. For example:

```cpp
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
```

The standard library headers provide C++ types and functions. The system headers provide declarations for operating-system interfaces, such as socket functions and `close()`.

Some headers I encountered:

| Header | Main purpose |
|---|---|
| `<iostream>` | Standard input and output streams |
| `<string>` | `std::string` and string conversion functions |
| `<vector>` | Dynamic arrays |
| `<map>` / `<unordered_map>` | Associative containers |
| `<thread>` | C++ thread support |
| `<mutex>` | Mutexes and lock management |
| `<atomic>` | Atomic types and operations |
| `<chrono>` | Time points, durations, and clocks |
| `<fstream>` | File input and output streams |
| `<sys/socket.h>` | Socket APIs such as `socket()`, `recv()`, and `send()` |
| `<netinet/in.h>` | Internet address structures and protocol constants |
| `<arpa/inet.h>` | Address conversion functions |
| `<unistd.h>` | POSIX functions such as `close()` |
| `<signal.h>` | Signal handling interfaces |

### Header declarations and source definitions

A header usually declares a class and its public methods. The corresponding `.cpp` file contains the method implementations.

For example, a class can declare `run()` in `RedisServer.h`, while `RedisServer.cpp` defines what `run()` actually does. This separation makes the project easier to navigate and maintain.

Header guards or `#pragma once` help prevent a header from being included more than once in a single compilation unit. I should use the approach already established in the project consistently.

---

## 4. Constructors, Access Specifiers, and Static Members

### Constructors

A constructor is a special class function that runs when an object is created. It is used to initialize the object.

The `RedisServer` constructor initializes server-related state such as the configured port, the socket descriptor, and the `running` flag. It also sets up the signal-handling arrangement used by the project.

### `public` and `private`

- `public` members can be accessed from outside the class through its public interface.
- `private` members can be accessed only by the class's own member functions and permitted friends.

This allows a class to expose operations that other parts of the program need while keeping internal implementation details protected.

### `static` members and the singleton database

The database uses a `getInstance()` function that returns the same database object instead of creating a separate database for every client.

A common singleton implementation uses a function-local static object:

```cpp
RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}
```

The local static object is initialized once. In modern C++, initialization of a function-local static is thread-safe. Returning a reference avoids making a separate database copy.

The singleton provides one shared database for the server's command handlers. It does not automatically make every database operation thread-safe; synchronization is still needed when multiple threads access shared data.

---

## 5. Exceptions and Input Validation

An exception is a mechanism for reporting an error that interrupts normal control flow. In C++, `try` contains code that might throw, and `catch` handles a matching exception.

For example, `std::stoi()` can throw an exception if the input is invalid or the value is too large. My `main.cpp` handles conversion errors when reading the port argument.

I also learned that successful conversion does not necessarily mean the input is completely valid. For example, a conversion can accept the numeric prefix of `"6378abc"`. For command-line input, I should validate both the conversion and the permitted port range.

Input validation is also important when processing commands. Each command expects a particular number of arguments. The command handler checks these counts and returns an error when the supplied arguments do not match the command's requirements.

---

## 6. Linux, POSIX, and File Descriptors

### What is a file descriptor?

A file descriptor is a small integer used by a process to refer to an open resource managed by the operating system. Regular files, sockets, and several other resources can be accessed through file descriptors.

In this project, the server socket and each accepted client socket are represented by file descriptors.

### The `close()` function

The POSIX `close()` function closes a file descriptor:

```cpp
close(client_socket);
```

I use it to close a client connection after its worker thread finishes. The server also closes its listening socket during shutdown.

The function is declared through `<unistd.h>` on POSIX systems. Closing a socket releases the descriptor for reuse by the process and updates the operating system's handling of that connection.

I should avoid closing the same descriptor multiple times or continuing to use it after it has been closed.

### Return values matter

System calls can fail. For example, `socket()`, `bind()`, `listen()`, `accept()`, and `recv()` have return values that indicate success or failure. Checking those results is important because a server must not assume that an operating-system operation always succeeds.

---

## 7. Socket Programming

A socket is an endpoint that a program uses to communicate with another program, locally or across a network.

My server uses the Berkeley/POSIX socket API to accept client connections over TCP.

### The server socket lifecycle

The main steps are:

1. `socket()` creates a socket.
2. `bind()` associates it with a local IP address and port.
3. `listen()` marks it as a socket that can accept incoming connections.
4. `accept()` waits for a client connection and returns a new socket descriptor for that client.
5. `recv()` receives bytes from the client.
6. `send()` transmits response bytes.
7. `close()` closes the connection when it is finished.

The listening socket and client socket serve different purposes. The listening socket accepts new connections, while the accepted client socket is used to communicate with that particular client.

### Important socket constants and functions

- `AF_INET` selects the IPv4 address family.
- `SOCK_STREAM` requests a stream-oriented socket, used here with TCP.
- `INADDR_ANY` allows the server to bind to available local IPv4 interfaces.
- `htons()` converts a 16-bit value from host byte order to network byte order. It is used when setting the port in the socket address.
- `SO_REUSEADDR` is a socket option that can help with rebinding a local address in common restart scenarios.

The project uses `setsockopt()` to set `SO_REUSEADDR`, although its return value should also be checked.

### Why is `close()` different from `shutdown()`?

`close()` releases a file descriptor. The socket API's `shutdown()` function controls communication directions on a socket, such as disabling further sends or receives. They are not interchangeable.

In my project, `RedisServer::shutdown()` is a custom C++ method that stops the server, saves data, and closes the listening socket. It is different from the POSIX socket function named `shutdown()`.

---

## 8. TCP and Client-Server Communication

TCP provides a reliable, ordered byte stream between two endpoints. It does not automatically preserve the boundaries of application commands.

For example, if a client sends one command, the server cannot assume that one call to `recv()` will always return exactly that command. The data might arrive in multiple chunks, or several writes from the client might be available together.

My current server reads bytes into a buffer and treats each received chunk as a request. This is enough for basic tests, but it is an important limitation: the implementation does not yet provide complete TCP message framing or robust handling of pipelined commands.

### `recv()` and `send()`

`recv()` receives bytes from a connected socket and returns the number of bytes received. A return value of zero indicates an orderly peer shutdown; a negative value indicates an error.

`send()` attempts to transmit bytes to a connected socket. A successful call is not guaranteed to send the entire response, so robust network code should handle partial sends and relevant errors. My current implementation calls `send()` once and does not handle partial sends.

These details helped me understand that network programming needs more than just calling `send()` and `recv()`: the server must also handle framing, disconnections, and errors.

---

## 9. Threads and Concurrency

### What is a thread?

A thread is an execution path within a process. Multiple threads in the same process share the process's memory, so they can access the same objects.

My server creates a worker thread for each accepted client connection. This lets the main server thread continue accepting connections while worker threads process client requests.

### `std::thread`

The C++ `<thread>` library provides `std::thread` for starting a new thread of execution. In my server, a worker uses the shared command handler to process the client's requests.

Because the command handler is captured by reference, the handler must remain alive until all worker threads that use it have finished.

### `join()` and `detach()`

- `join()` waits for a thread to finish.
- `detach()` separates a thread from the `std::thread` object that created it. The detached thread continues running independently.

The server keeps worker threads in a vector and joins them after its accept loop ends. The periodic persistence thread in `main.cpp` is detached and sleeps for 300 seconds (five minutes) between dump operations.

A detached thread requires special care because it does not automatically stop when the function that created it returns. The persistence thread currently has no explicit stop mechanism, so its lifetime and interaction with shutdown are important areas to improve.

### Concurrency and parallelism

Concurrency means multiple tasks can make progress during overlapping periods. Parallelism means tasks are actually executing at the same time, typically on different CPU cores.

Threads can help a server handle multiple clients, especially when some work waits for network input or output. However, creating one thread per client can consume significant resources when the number of clients grows.

### Blocking operations

A worker may block while waiting for client data in `recv()`. This means a worker can remain occupied even when it is not actively processing a command. During shutdown, joining such a worker can take time if the client socket remains open and no mechanism wakes the worker.

---

## 10. Mutexes, Atomics, and Shared Data

### Why do I need a mutex?

Because multiple client threads can execute database commands at the same time, they can try to read or modify the same data concurrently.

A mutex protects shared state by allowing only one thread at a time to hold the relevant lock. My database uses a mutex to guard individual database operations.

### `std::lock_guard`

`std::lock_guard` is an RAII helper that locks a mutex when the guard is created and unlocks it automatically when the guard goes out of scope.

This is safer than manually calling `lock()` and `unlock()` in every code path, because the lock is released when the guard is destroyed.

### Thread safety does not automatically mean atomic workflows

The database locks individual operations. That does not mean a sequence of separate commands becomes one indivisible transaction. Another client may run a command between two separate operations.

The persistence methods also acquire the database mutex. As a result, dumping or loading data can block other database operations while the lock is held.

### `std::atomic<bool>`

An atomic variable allows certain operations on that variable to occur without a data race. The server uses an atomic `running` flag to communicate whether its main accept loop should continue.

However, making one flag atomic does not make every other piece of shared server state thread-safe. For example, the server socket descriptor and the global server pointer still require careful coordination around shutdown.

### Signal handling

The server registers a handler for `SIGINT`, which is commonly generated by pressing Ctrl+C in a terminal. The current handler calls the server's custom shutdown method.

This is a learning point because POSIX signal handlers have strict safety rules. Logging with iostreams, locking mutexes, saving the database, and calling complex shutdown logic from a signal handler are not generally async-signal-safe. A safer design is to make the handler set a suitable signal-safe flag or notify a controlled shutdown path, then perform cleanup in normal program execution.

---

## 11. Data Structures Used in the Database

My database stores three kinds of values: strings, lists, and hashes. It also has a separate map for expiration deadlines.

### Separate stores

The implementation has separate containers for each data type:

- `kv_store` for string values.
- `list_store` for lists of strings.
- `hash_store` for hashes, which associate fields with values.
- `expiry_map` for key expiration deadlines.

Separating the stores makes it straightforward to implement commands for each type. However, the current design does not enforce a single data type per key name in every operation. For example, setting a string does not automatically remove a same-named list or hash. The `TYPE` command checks the stores in a fixed order, while `KEYS` can return duplicate key names when a name exists in more than one store.

### Lists and vectors

A list is stored using a vector of strings. This makes indexing straightforward, and adding to the right side is efficient on average.

The trade-off is that inserting or removing at the left side requires shifting elements. Therefore, operations like left push and left pop can take O(n) time in this implementation.

### Hashes

A hash maps field names to values. This lets the database look up a field without scanning every field when using an appropriate associative container.

Commands such as `HSET`, `HGET`, `HDEL`, `HGETALL`, `HKEYS`, and `HVALS` operate on the hash store.

### Helper functions

A helper such as `removeKeyFromAllStoresLocked()` removes a key from the string, list, hash, and expiration stores. The `Locked` suffix indicates that the helper is expected to be called while the database mutex is already held.

This avoids repeating the same deletion logic in several methods and helps keep data and expiration metadata consistent.

### Returning values

Returning a container by value creates a result that the caller can use independently. It can be useful for avoiding exposing internal database storage outside the mutex-protected operation. References can avoid copies, but returning references to shared data requires careful synchronization and lifetime management.

---

## 12. Command Parsing and RESP

### Why parsing is needed

The client sends bytes, but the database needs structured command arguments such as a command name, key, and value. The command handler therefore parses the request before dispatching it to the correct operation.

The parser supports RESP arrays of bulk strings and also a whitespace-separated fallback format. RESP stands for Redis Serialization Protocol.

### RESP-style requests

A RESP array begins with `*`, followed by the number of elements. Each bulk string begins with `$`, followed by the byte length of the string.

For example, a conceptual representation of `SET name Afzal` is:

```bash
*3\r\n
$3\r\n
SET\r\n
$4\r\n
name\r\n
$5\r\n
Afzal\r\n
```

The `\r\n` sequence is carriage return followed by line feed. RESP uses this delimiter in its framing.

The implementation validates parts of the array and bulk-string format, including lengths and expected CRLF delimiters. It is a limited RESP implementation rather than full Redis protocol support.

### Whitespace fallback

The parser also supports a simpler whitespace-separated request. This is convenient for basic testing, but whitespace splitting does not preserve arguments that contain spaces in the same way that properly framed bulk strings can.

### Why command names are converted to uppercase

The command handler converts the command name to uppercase before dispatching it. This allows a command such as `set` to be handled like `SET`.

The conversion uses `std::toupper()`. When calling character-classification functions with a potentially signed `char`, converting to `unsigned char` first avoids undefined behavior for negative character values.

### Protocol limitations

The server currently processes a received chunk as a command and does not correctly support arbitrary TCP fragmentation or multiple pipelined commands in one chunk. A more complete implementation needs a connection-level input buffer and a parser that can identify complete frames before processing them.

---

## 13. Redis-Like Commands and Responses

I implemented commands for strings, lists, hashes, expiration, and basic server behavior.

### String and general commands

Examples include:

- `PING` — returns a PONG response.
- `ECHO` — returns the supplied argument.
- `SET` — stores a string value.
- `GET` — retrieves a string value.
- `KEYS` — returns known keys.
- `TYPE` — reports the type found for a key.
- `DEL` / `UNLINK` — removes a key.
- `EXPIRE` — sets a time-to-live.
- `RENAME` — changes a key name.
- `FLUSHALL` — clears all stores.

These commands helped me understand how command parsing, argument validation, database operations, and protocol responses work together.

### List commands

The implementation includes operations such as `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LINDEX`, `LSET`, `LRANGE`, `LREM`, and `LGET`.

List commands helped me understand indexing, insertion and removal costs, negative indices, and how empty or missing lists should be handled.

### Hash commands

The implementation includes `HSET`, `HGET`, `HEXISTS`, `HDEL`, `HGETALL`, `HKEYS`, `HVALS`, `HLEN`, and `HMSET`.

Hash commands helped me understand field-value storage and the difference between creating a new field and updating an existing field.

### RESP-style responses

The command handler constructs responses using protocol markers. Some examples are:

- `+OK\r\n` — a simple success response.
- `+PONG\r\n` — the response to PING.
- `:1\r\n` — an integer response.
- `$-1\r\n` — a null bulk-string response in the protocol subset used here.
- `-ERR ...\r\n` — an error response.

It is important to return a response in the expected format because the client relies on that format to interpret the result.

### Argument validation

Different commands require different argument counts. Some commands check for an exact number of arguments, while others accept a minimum number and ignore additional arguments. Recording this behavior helps me understand both the intended command syntax and the current limitations of the implementation.

---

## 14. Time-to-Live (TTL) and Expiration

### What is TTL?

TTL means Time To Live. It specifies how long a key should remain available before it expires.

My database uses `expiry_map` to associate a key with an expiration deadline. The `EXPIRE` command sets this deadline.

### `std::chrono::steady_clock`

`std::chrono` provides types for working with durations and time points. `std::chrono::steady_clock` is suitable for measuring elapsed time because it is monotonic and is not intended to jump when the system's wall clock changes.

The database can store a deadline calculated from the current steady-clock time plus the requested duration, then compare the current time with that deadline during later operations.

### Lazy expiration

Lazy expiration means an expired key is removed when the database checks it, rather than requiring a separate timer for every key.

The project has helper functions to check a single key and to scan all keys for expiration. Operations such as `GET` and `TYPE` check the requested key, while `KEYS` performs a full expiration cleanup.

Expiration is not currently applied consistently across all operations. For example, list and hash operations generally do not purge expired keys first, and `dump()` does not remove all expired entries before writing the file. This is an important behavior to understand when testing the implementation.

### TTL and overwriting keys

When `SET` replaces a string value, the implementation removes an existing expiration entry for that key. This means the new value does not keep the old TTL. Deletion helpers also remove expiration metadata so that an old deadline does not remain after a key is removed.

### Current TTL limitation

The expiration deadlines are based on `steady_clock`, and the current persistence format does not save TTL information. When data is loaded after a restart, the stored values can return without their previous expiration deadlines. Supporting TTL persistence would require saving suitable expiration information and restoring it safely.

---

## 15. File I/O and Persistence

### What is persistence?

Persistence means saving data so it can be restored later, including after a server process restarts.

My server uses a file named `dump.my_rdb` to save the database. At startup, `main.cpp` attempts to load the file. The database can also be dumped periodically and during shutdown-related paths.

### File streams

C++ provides file streams such as:

- `std::ifstream` for reading a file.
- `std::ofstream` for writing a file.

The database's `load()` method reads records from the dump file. The `dump()` method writes records representing strings, lists, and hashes.

### Current dump format

The file uses record markers:

- `K` for a string key and value.
- `L` for a list key and its values.
- `H` for a hash key and its field-value data.

Hash fields and values are represented using a colon separator. The format is simple to read and implement, but whitespace-separated values and colon-separated hash entries introduce limitations when data contains those characters.

### Periodic persistence

The main program starts a detached thread that sleeps for 300 seconds (five minutes) and then writes a dump. This means periodic saving is not the same as saving every command immediately: a process failure between dumps can lose recent changes.

The current design also saves during shutdown-related paths. Because multiple persistence paths may interact with the same database, synchronization is important.

### Current persistence limitations

- TTL deadlines are not saved or restored.
- `dump()` does not purge all expired keys before writing.
- The format does not robustly encode arbitrary strings containing whitespace or delimiters.
- The code checks whether the output file opened, but does not fully verify every write afterward.
- The detached persistence thread has no explicit stop mechanism.
- Loading clears the existing stores and expiration metadata before reading records, so malformed or incomplete input can leave only partially restored data.

These limitations help me identify what I would need to improve before treating the project as a reliable production database.

---

## 16. Error Handling and Debugging

One lesson from this project is that successful compilation does not prove that the program behaves correctly.

Errors can occur while creating sockets, binding ports, accepting connections, receiving requests, parsing commands, converting strings, reading files, and writing files.

I need to check return values and handle invalid input rather than assume every operation succeeds. I also need to test behavior when the input is incomplete, malformed, out of range, or refers to a missing key.

Useful cases to test include:

- Starting the server with the default port and with a custom port.
- Supplying an invalid port argument.
- Sending a valid command and a command with missing arguments.
- Requesting a key that does not exist.
- Testing list indices at and outside the valid range.
- Expiring a key and checking it after the deadline.
- Restarting the server and verifying which data is restored.
- Disconnecting a client while the server is waiting for input.
- Connecting multiple clients and sending commands concurrently.

A useful debugging habit is to reproduce the issue with a small test, inspect the return values and state involved, make one change at a time, and rerun the test.

---

## 17. Build System and Makefile

A Makefile describes how the project's source files are compiled and linked into an executable.

I use:

```bash
make clean && make
```

`make clean` removes build outputs specified by the clean rule, and `make` builds the project using the targets and dependencies in the Makefile.

Separating source files into different compilation units helps organize the project. The build system must compile the relevant `.cpp` files and link them together. Since the project uses threads, the build configuration also needs to link with the platform's thread support where required.

---

## 18. Performance and Time Complexity

Time complexity helps me estimate how an operation's work grows as the amount of data increases.

Some examples from this implementation:

| Operation | Approximate complexity | Reason |
|---|---:|---|
| Vector access by index | O(1) | Direct indexed access |
| `push_back()` on a vector | Amortized O(1) | Usually appends at the end |
| Insert at the beginning of a vector | O(n) | Existing elements may shift |
| Remove the first vector element | O(n) | Remaining elements may shift |
| Scan a vector for a value | O(n) | Elements may need to be checked one by one |
| Scan all keys for expiration | O(n) | Each tracked key may need to be examined |
| Ordered-map lookup | O(log n) | Tree-based search |
| Hash-map lookup | Average O(1) | Uses a hash table, with O(n) worst-case behavior |

The actual complexity depends on the container used by a specific operation and on how many elements it processes.

This project taught me that selecting a data structure affects command performance. For example, a vector is convenient for indexed list operations, but it is not ideal for frequent insertions and removals at the beginning.

Concurrency also affects performance. A mutex protects shared data, but a long operation such as writing a complete database dump while holding the lock can delay other commands.

---

## 19. What I Still Want to Improve

I want to keep this section as a list of topics to revisit as I improve the project and my understanding.

- [ ] Validate the complete command-line port input and enforce the valid port range.
- [ ] Improve TCP request framing so fragmented requests and multiple commands are handled correctly.
- [ ] Handle partial writes and errors returned by `send()`.
- [ ] Improve graceful shutdown without doing complex work inside a signal handler.
- [ ] Add a controlled way to stop the periodic persistence thread.
- [ ] Make key-type behavior consistent so a key does not unintentionally exist in multiple stores.
- [ ] Apply expiration checks consistently across string, list, hash, and persistence operations.
- [ ] Decide how TTL information should be saved and restored.
- [ ] Make persistence robust against malformed input and file write failures.
- [ ] Add automated tests for commands, edge cases, expiration, persistence, and concurrent clients.
- [ ] Review whether a thread-per-client design is appropriate for larger numbers of connections.
- [ ] Explore a buffered protocol parser and a more complete RESP implementation.
- [ ] Revisit database data structures and their time complexity as the project grows.

---

## Final Note

Building this project has helped me connect C++ concepts with operating-system and networking concepts. I am not treating every implementation detail as finished or production-ready. I want to understand why each part works, recognize its limitations, and improve it step by step.

I will continue updating this file whenever I learn a new concept, solve a problem, or change an important design decision. The goal is not only to make the server work, but also to understand the code well enough to explain it clearly in an interview.
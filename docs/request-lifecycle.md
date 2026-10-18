# Request Lifecycle

## 1. Overview

This document explains how a client request travels through the Redis-like server, from the initial TCP communication to command execution and response delivery.

The request lifecycle involves three main components:

- `RedisServer`: Receives client data and sends responses.
- `RedisCommandHandler`: Parses and dispatches commands.
- `RedisDatabase`: Executes data operations on the shared in-memory database.

The following example traces a simple `SET` command through these components.

## 2. Example Request

Suppose the client sends:

```bash
SET username Afzal
```

The purpose of this command is to store the string value `Afzal` under the key `username`.

The request travels through the server's networking layer, command-processing layer, and database layer.

## 3. Step 1 — Accepting the Client Connection

**File:** `src/RedisServer.cpp`

The server creates a TCP socket, binds it to the configured port, and starts listening for incoming connections.

The following call accepts a client connection:

```cpp
int client_socket = accept(server_socket, nullptr, nullptr);
```

When a connection is accepted, the server receives a client socket descriptor. This descriptor is used for communication with that specific client.

The server then creates a worker thread:

```cpp
threads.emplace_back([client_socket, &cmdHandler]() {
    // Handle client communication.
});
```

Each accepted connection is handled by a separate worker thread.

This design allows the server to manage multiple client connections concurrently instead of processing every connection within a single client-handling loop.

## 4. Step 2 — Receiving Request Data

**File:** `src/RedisServer.cpp`

Inside the worker thread, the server receives data using:

```cpp
int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
```

The `recv()` call attempts to read data from the client socket into a fixed-size buffer.

If `recv()` returns zero or a negative value, the current implementation exits the receive loop and closes the client socket.

Otherwise, the received bytes are copied into a string:

```cpp
std::string request(buffer, bytes);
```

Using the returned byte count ensures that the request string contains the bytes actually received, rather than relying on a null terminator.

### Important TCP detail

TCP provides a byte stream, not application-level message boundaries. One `recv()` call does not necessarily correspond to one complete command.

The current server passes each received chunk directly to the command handler. It does not accumulate incomplete requests or explicitly split multiple pipelined requests received together.

This is an important limitation of the current implementation.

## 5. Step 3 — Parsing the Command

**File:** `src/RedisCommandHandler.cpp`

The server forwards the received request to:

```cpp
std::string response = cmdHandler.processCommand(request);
```

The `processCommand()` method invokes `parseRespCommands()` to convert the request into command tokens.

For the example:

```bash
SET username Afzal
```

The whitespace-based fallback produces tokens equivalent to:

```cpp
{
    "SET",
    "username",
    "Afzal"
}
```

The parser also supports RESP arrays containing bulk strings. However, the current parser does not implement the complete RESP protocol.

If parsing produces an empty token list, `processCommand()` returns an error response.

## 6. Step 4 — Identifying and Dispatching the Command

**File:** `src/RedisCommandHandler.cpp`

After parsing, `processCommand()` normalizes the command name to uppercase:

```cpp
std::transform(
    tokens[0].begin(),
    tokens[0].end(),
    tokens[0].begin(),
    [](unsigned char c) {
        return std::toupper(c);
    }
);
```

This allows the command dispatcher to recognize command names without depending on their original letter casing.

The method then checks the command against the supported command handlers.

For the example request, the command name is `SET`, so the SET handler is selected.

If the command is not recognized, the dispatcher returns an unknown-command error.

## 7. Step 5 — Executing the Database Operation

**Files:** `src/RedisCommandHandler.cpp`, `src/RedisDatabase.cpp`

The SET handler validates that the request contains at least three tokens and then calls the database's string-setting method.

Conceptually, the operation is:

```cpp
db.set("username", "Afzal");
```

The database's `set()` method acquires the database mutex, writes the value into `kv_store`, and removes any existing expiration entry for that key.

The database then releases the mutex when the method returns.

This synchronization protects the affected database data from simultaneous access by other threads using the same mutex.

### Result

The in-memory string store now contains the mapping:

```bash
username -> Afzal
```

This value is stored in memory. It is not immediately written to the persistence file by the SET operation itself.

## 8. Step 6 — Formatting the Response

**File:** `src/RedisCommandHandler.cpp`

After the database operation succeeds, the command handler constructs the response in the protocol format expected by the client.

For the example SET command, the successful response is:

```bash
+OK\r\n
```

Here:

- `+` indicates a RESP simple string.
- `OK` indicates successful completion.
- `\r\n` terminates the response according to RESP formatting.

The handler returns this response as a C++ string to the server's worker thread.

## 9. Step 7 — Sending the Response

**File:** `src/RedisServer.cpp`

The worker thread sends the response using:

```cpp
send(
    client_socket,
    response.c_str(),
    response.size(),
    0
);
```

The response travels through the client's TCP connection.

The worker then returns to its receive loop and waits for additional data from the same client.

If the client disconnects or a receive error occurs, the loop ends and the worker closes the client socket.

**Current limitation:** the implementation does not check the result of `send()` or retry partial sends. A successful-looking command execution therefore does not guarantee that the entire response reached the client.

## 10. Complete Request Sequence

The lifecycle can be summarized as follows:

1. A client connects to the server.
2. `accept()` returns a client socket descriptor.
3. The server creates a worker thread for that connection.
4. `recv()` reads request bytes from the client.
5. The worker constructs a request string.
6. `processCommand()` invokes the command parser.
7. The dispatcher identifies the command and selects its handler.
8. The handler calls the corresponding database method.
9. The database performs the operation under its mutex.
10. The command handler constructs the protocol response.
11. The worker calls `send()` to return the response.
12. The worker waits for more client data or closes the connection when its receive loop ends.

## 11. Error Handling

The current implementation handles several basic error cases:

| Situation | Current behavior |
|---|---|
| Empty or invalid command parsing result | Returns an error response from the command handler. |
| Unknown command | Returns an unknown-command error. |
| Database key does not exist | The response depends on the specific command; for example, GET returns a null response. |
| Client disconnects | The worker exits its receive loop and closes the client socket. |
| `recv()` returns a negative value | The worker exits the receive loop; the current code does not distinguish retryable errors from other failures. |
| Partial response send | Not explicitly handled. |

## 12. Concurrency During Request Processing

Multiple worker threads may process commands at the same time.

The worker threads share a `RedisCommandHandler` object. Each call to `processCommand()` creates its own local command tokens, while database operations access the shared singleton database.

The database mutex protects operations that acquire it. However, synchronization of individual database methods does not automatically make a sequence of multiple operations atomic.

For example, a handler that performs several database calls may allow another worker to execute an operation between those calls.

This distinction is important when evaluating concurrency correctness.

## 13. Relationship to Persistence

Normal command execution modifies the in-memory database. Persistence is handled separately by the application.

The current application:

- Loads `dump.my_rdb` before starting the server.
- Attempts to dump the database periodically from a background thread.
- Attempts additional dumps during server shutdown and after the server's accept loop ends.

Consequently, a successful SET response confirms the command's in-memory operation, but it does not mean that the value has already been persisted to disk.

## 14. Summary

The request lifecycle separates network communication, command processing, and data management.

`RedisServer` handles client sockets, `RedisCommandHandler` interprets and executes commands through the database interface, and `RedisDatabase` manages the underlying data structures.

Understanding this flow is essential for debugging command execution, analyzing concurrency, improving protocol handling, and explaining the server's architecture during a software engineering interview.
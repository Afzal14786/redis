# Error Handling

## 1. Overview

Error handling is an important part of a server because incoming requests may be invalid, clients may disconnect unexpectedly, and operating-system operations may fail.

In this project, error handling is distributed across three main components:

- **`RedisServer`:** Handles socket creation, binding, listening, accepting connections, and receiving client data.
- **`RedisCommandHandler`:** Handles request parsing, command validation, argument validation, and command-related errors.
- **`RedisDatabase`:** Performs storage operations and returns results that the command handler uses to construct responses.

The current implementation handles several common errors, but some failures are not handled comprehensively. This document records the existing behavior and identifies potential improvements separately.

## 2. Error-Handling Strategy

The server uses different mechanisms depending on the operation.

| Mechanism | Purpose |
|---|---|
| Error response strings | Communicate invalid commands or arguments to a client |
| Boolean return values | Indicate whether operations such as deletion, expiration, loading, or dumping succeeded |
| Integer return values | Represent operation results such as counts or success indicators |
| Exception handling | Handle exceptions such as invalid port parsing in `main.cpp` |
| System-call return values | Detect failures in socket operations such as `socket()`, `bind()`, `listen()`, `accept()`, and `recv()` |

These mechanisms are not interchangeable. For example, a command returning zero may indicate that a key was absent rather than that an internal error occurred.

## 3. Command-Level Error Handling

### 3.1 Unknown Commands

When a request contains a command that is not recognized by the command handler, the handler returns an error response.

Example:

```bash
UNKNOWNCOMMAND
```

Illustrative response:

```bash
-ERR unknown command\r\n
```

The exact message should be verified against the current implementation.

**Why this matters:** Unknown-command handling prevents unsupported commands from being silently treated as successful operations.

### 3.2 Incorrect Argument Counts

Commands validate their argument counts before accessing the corresponding tokens.

For example, `ECHO` requires exactly two tokens, while `SET` requires at least three.

A request such as:

```bash
ECHO
```

does not provide the required message argument.

Similarly:

```bash
HSET user name
```

does not provide the value required by `HSET`.

The handler returns an error for invalid argument counts where that command implements validation.

**Current limitation:** Argument validation is not uniform. Some commands require an exact number of tokens, some require a minimum number, and others ignore additional tokens.

### 3.3 Invalid Integer Arguments

Several commands require integer arguments, including:

- `EXPIRE`
- `LREM`
- `LINDEX`
- `LSET`
- `LRANGE`

The handler parses these values as integers. For commands whose implementation checks complete input consumption, values with trailing nonnumeric characters are rejected.

For example:

```bash
EXPIRE session abc
```

contains an invalid integer for the TTL.

A malformed integer should produce a command error rather than being treated as a valid TTL.

**Current limitation:** Verify each command's conversion and error path individually. Do not assume that all integer arguments use identical validation.

### 3.4 Malformed RESP Requests

The parser supports RESP arrays of bulk strings and validates the supported format.

It checks structural details such as:

- Expected delimiters.
- Declared lengths.
- Input boundaries.
- Correct CRLF termination.
- Unexpected trailing data after a parsed request.

Malformed RESP input is rejected by the parser.

This validation is important because the declared length of a bulk string determines how many bytes belong to an argument.

**Current limitation:** Rejecting malformed input is not the same as implementing the complete RESP protocol. The parser supports only a subset of RESP features.

### 3.5 Whitespace-Separated Input

The parser also supports a whitespace-separated fallback format.

For example:

```bash
SET username Afzal
```

can be split into command tokens.

However, whitespace-separated input cannot reliably represent arbitrary arguments containing spaces as one value. The command handler may also ignore extra tokens for some commands.

For commands that need values containing spaces, the supported RESP bulk-string format is more appropriate.

## 4. Database Operation Results

The database often reports outcomes using return values rather than exceptions.

### 4.1 Missing Keys

A missing key does not always represent an internal failure.

Examples include:

- `GET` when no string value exists.
- `TYPE` when no matching key exists.
- `DEL` when the requested key is absent.
- `LLEN` when a list does not exist.
- `HLEN` when a hash does not exist.

The result depends on the command. A missing key may lead to a null response, a zero count, a `none` type, or an error for commands that require an existing object.

The command handler determines how database results are represented in the protocol.

### 4.2 Deletion Results

The database's `del()` method returns a boolean indicating whether the key existed in at least one store before deletion.

The command handler converts that result into an integer-style response.

A false result does not necessarily indicate an internal error; it can simply mean the key was not present.

### 4.3 Expiration Results

The database's `expire()` method returns false when the requested key does not exist.

A positive TTL sets an expiration deadline. A zero or negative TTL causes immediate removal.

The handler converts the result into a protocol response.

One implementation detail is that expiration cleanup is not triggered by every database operation. An expired key can remain physically present until a cleanup-triggering operation accesses or removes it.

### 4.4 Persistence Results

The `dump()` and `load()` methods return boolean results.

- `dump()` returns false if the output file cannot be opened.
- `load()` returns false if the input file cannot be opened.

However, the current implementation does not comprehensively validate every read or write operation after opening the file.

Consequently, a successful return does not guarantee that every record was safely written or read without an I/O error.

See `docs/persistence-and-expiration.md` for the persistence format and behavior.

## 5. Network-Level Error Handling

Network errors are primarily handled in `src/RedisServer.cpp`.

### 5.1 Socket Creation

The server creates a TCP socket using `socket()`.

If socket creation fails, the method logs an error and returns from `run()`.

The server cannot accept client connections without a valid listening socket.

### 5.2 Socket Options

The server configures `SO_REUSEADDR`.

The current implementation does not check the return value of `setsockopt()`. A failure to apply this option is therefore not handled explicitly.

### 5.3 Binding

The server binds the socket to the configured port and `INADDR_ANY`.

If `bind()` fails, the implementation reports the error and returns from `run()`.

A possible cause is that the port is already in use or cannot be bound under the current system conditions.

### 5.4 Listening

The server calls `listen()` with a backlog of 10.

If `listen()` fails, the implementation reports the error and returns from `run()`.

The backlog controls the queue of pending connections; it does not limit the total number of clients the application can eventually serve.

### 5.5 Accepting Connections

The server repeatedly calls `accept()` while its `running` flag is true.

If `accept()` fails, the server logs the error when it is still considered to be running, then exits the accept loop.

This allows the loop to stop after an error or shutdown-related interruption.

### 5.6 Receiving Client Data

Each worker thread calls `recv()` to receive bytes from its client socket.

When `recv()` returns zero or a negative value, the worker leaves its receive loop and closes the client socket.

A return value of zero normally indicates that the peer has closed its sending side. A negative result indicates an error, but the current implementation does not comprehensively distinguish error causes or retry transient errors.

### 5.7 Sending Responses

The server calls `send()` to transmit a command response.

The current implementation does not check the result comprehensively and does not retry partial writes.

This means a response may not be fully transmitted even if the call sends some bytes successfully.

A robust implementation should track how many bytes have been sent and continue until the entire response is delivered or a terminal error occurs.

## 6. Port Parsing in `main.cpp`

The server accepts an optional port argument.

The code uses `std::stoi()` inside a `try`/`catch` block. If conversion throws an exception, the program prints an error and returns a nonzero exit status.

For example, a clearly nonnumeric argument can trigger the exception handler.

However, `std::stoi()` can accept a numeric prefix followed by other characters unless the code checks how much of the input was consumed. The current code also does not explicitly validate that the resulting port falls within the valid TCP port range.

These are validation improvements to consider; they are not implemented guarantees.

## 7. Error Handling During Shutdown

The server's shutdown behavior also has error-handling and lifecycle concerns.

The current implementation:

1. Sets the `running` flag to false.
2. Dumps the database to `dump.my_rdb`.
3. Closes the listening socket when its descriptor is not `-1`.
4. Prints shutdown-related messages.

The return value of the shutdown dump is not used to comprehensively recover from persistence failures.

In addition, worker threads may remain blocked in `recv()` on client sockets. The server joins worker threads after leaving the accept loop, so shutdown can wait for those threads to finish.

The signal handler also invokes operations such as logging, database persistence, and socket shutdown. These operations are not generally safe to execute directly from a POSIX signal handler.

A safer design would use the signal handler only to notify the main execution flow, then perform logging, persistence, and orderly resource cleanup in normal thread context.

## 8. Known Error-Handling Limitations

The following issues are worth tracking separately from the existing behavior.

| Area | Current limitation | Potential improvement |
|---|---|---|
| Argument validation | Rules vary across commands | Centralize or standardize validation where appropriate |
| Integer parsing | Validation details vary by command | Use consistent parsing and range checks |
| RESP parsing | Supports only a subset of RESP | Expand protocol support and add parser tests |
| TCP framing | Each `recv()` result is treated as a request | Buffer input and parse complete commands |
| Socket options | `setsockopt()` result is ignored | Check and report failures |
| Socket cleanup | Some early error paths may leave the socket open | Use a consistent resource-management strategy |
| Sending responses | Partial writes are not handled | Send until complete or report an error |
| Receive errors | Negative results are not fully classified | Handle disconnects, interruptions, and terminal errors distinctly |
| Persistence | Stream errors after opening are not comprehensively checked | Check write, flush, and read state |
| Shutdown | Worker threads may block on client reads | Introduce a coordinated client-connection shutdown strategy |
| Signal handling | Complex operations execute inside the handler | Defer cleanup to normal execution context |
| Port validation | Full-string consumption and valid range are not checked | Validate the complete input and TCP port range |

These improvements should be considered independently and tested before being incorporated into the working implementation.

## 9. Testing Error Paths

Error handling should be tested intentionally rather than only through successful commands.

Recommended test cases include:

1. Send an unknown command.
2. Omit required arguments from `SET`, `ECHO`, `HSET`, and `LRANGE`.
3. Provide too many arguments to commands that require exact counts.
4. Supply invalid integers to `EXPIRE`, `LINDEX`, and `LRANGE`.
5. Send malformed RESP arrays and incorrect bulk-string lengths.
6. Disconnect a client while the server is waiting for input.
7. Start the server on a port that is already in use.
8. Try invalid and out-of-range port arguments.
9. Use a directory or unwritable location where a dump file is expected.
10. Restart after a persistence operation and verify the resulting database state.
11. Send requests in fragmented TCP writes to test request framing.
12. Send multiple commands rapidly through a single connection to investigate the current pipelining limitation.

For each test, record the request, expected behavior, actual response, and any relevant server log output.

## 10. Summary

Error handling is distributed across the network server, command handler, and database. The project uses protocol error responses, return values, exception handling, and system-call checks to handle different failure conditions.

The implementation already covers several basic failure cases, including unknown commands, malformed supported RESP requests, incorrect argument counts for validated commands, and major socket setup failures.

The main areas for improvement are consistent argument validation, robust TCP framing, complete send handling, persistence I/O verification, resource cleanup, and safe shutdown coordination.

Documenting these behaviors provides a clear basis for future improvements and gives useful material for explaining defensive programming and server reliability during an SDE interview.
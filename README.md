
# REDIS Server in C++

Building my own REDIS-inspired server from scratch using C++.

This project is an educational implementation of a lightweight, in-memory key-value server. It explores how a server accepts client connections, processes commands, stores data, and sends responses over TCP/IP.

> **Project Status:** Work in Progress. The goal is to build a minimum working version first, then gradually improve command support, reliability, and REDIS protocol compatibility.

---

## Table of Contents

- [Overview](#overview)
- [Project Goals](#project-goals)
- [Features](#features)
- [Architecture](#architecture)
- [Core Concepts](#core-concepts)
- [Supported Commands](#supported-commands)
- [Requirements](#requirements)
- [Build and Run](#build-and-run)
- [Usage Examples](#usage-examples)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [Current Scope and Limitations](#current-scope-and-limitations)
- [Future Improvements](#future-improvements)
- [Developer Information](#developer-information)

---

## Overview

REDIS is an in-memory data store known for its performance, key-value storage model, and support for multiple data structures.

This project recreates selected concepts behind a REDIS-like server using **C++**. It is an opportunity to understand how a server works internally, from accepting a TCP connection to parsing a command, performing a database operation, and returning a response.

The project focuses on:

- TCP/IP socket programming.
- In-memory data storage.
- Command parsing and dispatching.
- RESP-style request and response handling.
- C++ data structures and object-oriented design.
- Concurrent access and thread safety.
- Key expiration and persistence.

**Note:** This is a REDIS-inspired educational project, not a complete or fully compatible replacement for the official REDIS server.

## Project Goals

The primary goals of this project are:

1. **Learn socket programming:** Understand how clients and servers communicate using TCP/IP.
2. **Build a server from scratch:** Implement the basic server lifecycle, including listening for connections, receiving requests, and sending responses.
3. **Design a command-processing system:** Separate command validation, execution, and response generation.
4. **Implement data structures:** Support key-value storage and list operations, with additional data types as the project evolves.
5. **Understand concurrency:** Learn how mutexes and synchronization protect shared data.
6. **Explore RESP:** Understand the protocol formats used for communicating with REDIS clients.
7. **Implement persistence:** Explore saving data to disk and loading it when the server restarts.
8. **Write maintainable C++:** Organize the code into components with clear responsibilities.

---

## Features

The project is being developed incrementally. The following list describes the intended scope; individual features should be verified against the current implementation.

- **TCP Server:** Accepts client connections over a configured TCP port.
- **Command Processing:** Parses incoming commands and routes them to appropriate handlers.
- **In-Memory Database:** Stores and retrieves data using C++ data structures.
- **String Operations:** Supports basic operations such as `SET` and `GET`.
- **Key Management:** Includes operations such as `KEYS`, `TYPE`, `DEL`, `EXPIRE`, and `RENAME`.
- **List Operations:** Includes commands such as `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LINDEX`, `LSET`, `LREM`, and `LRANGE`.
- **RESP-Style Responses:** Uses protocol response formats such as simple strings, integers, bulk strings, and arrays.
- **Concurrency Protection:** Uses synchronization mechanisms such as mutexes to protect shared database state.
- **Key Expiration:** Explores setting a time-to-live for stored keys.
- **Persistence:** Explores saving database data to a local file and restoring it when the server starts.
- **Modular Architecture:** Separates networking, command handling, and database responsibilities.

---

## Architecture

The server is organized around three main components.

### 1. RedisServer

`RedisServer` manages the networking layer.

**Responsibilities:**

- Create and configure the server socket.
- Bind the socket to a port.
- Listen for incoming connections.
- Accept client connections.
- Receive data from clients.
- Send responses back to clients.

### 2. RedisCommandHandler

`RedisCommandHandler` processes commands received from clients.

**Responsibilities:**

- Parse incoming command data.
- Identify the command name.
- Validate command arguments.
- Route commands to their corresponding handlers.
- Call the appropriate database methods.
- Format database results into protocol responses.

Separating individual command handlers keeps the command dispatcher readable and makes the system easier to extend.

### 3. RedisDatabase

`RedisDatabase` manages the data stored by the server.

**Responsibilities:**

- Store and retrieve string values.
- Manage list and hash data where implemented.
- Delete and rename keys.
- Track key expiration.
- Protect shared database structures using synchronization.
- Save and load data for persistence.

### Request Lifecycle

A typical request follows this flow:

```text
             +-------------------+
             |      Client       |
             +-------------------+
                       |
                       | TCP Request
                       v
             +-------------------+
             |    RedisServer    |
             +-------------------+
                       |
                       | Request Data
                       v
             +------------------------+
             |  RedisCommandHandler   |
             +------------------------+
                       |
                       | Command Execution
                       v
             +-------------------+
             |   RedisDatabase   |
             +-------------------+
                       |
                       | Operation Result
                       v
             +------------------------+
             | RESP-Style Response    |
             +------------------------+
                       |
                       v
                     Client
```

---

## Core Concepts

### 1. C++ and Standard Library

C++ provides the language features and standard library utilities used to implement the server.

Some important components include:

| Component | Purpose |
|---|---|
| `std::string` | Represents keys, values, and command data. |
| `std::vector` | Stores parsed arguments and ordered list elements. |
| `std::unordered_map` | Provides key-based data storage. |
| `std::mutex` | Protects shared data from unsafe concurrent access. |
| `std::lock_guard` | Manages mutex locking through RAII. |
| `std::chrono` | Supports time-based expiration. |
| `std::istringstream` | Helps parse input data. |
| `std::ostringstream` | Helps construct response strings. |

### 2. TCP/IP Socket Programming

TCP provides a reliable byte stream between the client and server.

The server listens on a port, accepts incoming connections, reads data from clients, and sends response data back.

One important detail is that **TCP does not preserve application message boundaries**. A single request can arrive across multiple reads, or multiple requests can arrive together. Correct message framing is therefore important for reliable request processing.

### 3. Command Dispatching

Command dispatching determines which operation should execute for a given request.

For example, when a client sends:

```text
SET name Afzal
```

The command handler identifies `SET`, validates the arguments, invokes the corresponding database operation, and returns the appropriate response.

Keeping command-specific logic in separate functions makes it easier to add commands without creating one large conditional block.

### 4. RESP-Style Protocol

REDIS clients communicate using the RESP protocol. This project explores RESP-style parsing and response formatting.

Common response forms include:

| Response Type | Example | Purpose |
|---|---|---|
| Simple string | `+OK\r\n` | Returns a status message. |
| Error | `-ERR message\r\n` | Reports an error. |
| Integer | `:1\r\n` | Returns an integer result. |
| Bulk string | `$3\r\ncat\r\n` | Returns a string with its byte length. |
| Null bulk string | `$-1\r\n` | Represents a null result in the basic RESP form. |
| Array | `*2\r\n...` | Returns a sequence of elements. |

Full RESP compatibility requires correct parsing and formatting beyond these examples.

### 5. Concurrency and Mutexes

When multiple client requests access shared database structures, synchronization is needed to prevent unsafe concurrent reads and writes.

A mutex can protect a critical section so that only one thread at a time executes the protected code.

### 6. Key Expiration

Key expiration allows a stored key to become invalid after a specified duration.

For example:

```text
SET session data
EXPIRE session 60
```

The intended behavior is for the key to expire after 60 seconds.

Recording an expiration deadline alone is not sufficient. The database must also check and enforce expiration when the key is accessed or through another cleanup mechanism.

### 7. Persistence

Persistence allows stored data to survive a server restart.

A basic persistence workflow is:

1. Save database data to a file.
2. Stop the server.
3. Start the server again.
4. Load the saved data.
5. Verify that previously stored values are available.

File errors and shutdown behavior must be handled carefully to avoid overwriting valid data with an empty or incomplete snapshot.

---

## Supported Commands

The following tables describe the basic command scope. A command should be considered supported only when its implementation and behavior have been tested.

### Connection and Utility Commands

| Command | Description | Example |
|---|---|---|
| `PING` | Checks whether the server responds. | `PING` |
| `ECHO` | Returns a supplied message. | `ECHO hello` |
| `FLUSHALL` | Removes all stored data. Use carefully. | `FLUSHALL` |

### String and Key Commands

| Command | Description | Example |
|---|---|---|
| `SET` | Stores a string value under a key. | `SET name Afzal` |
| `GET` | Retrieves a string value. | `GET name` |
| `KEYS` | Lists stored keys; pattern matching may be limited. | `KEYS *` |
| `TYPE` | Returns the data type associated with a key. | `TYPE name` |
| `DEL` | Deletes a key. | `DEL name` |
| `UNLINK` | May be implemented as a deletion alias in this project; this does not guarantee REDIS's asynchronous deletion behavior. | `UNLINK name` |
| `EXPIRE` | Sets a key's time-to-live in seconds. | `EXPIRE session 60` |
| `RENAME` | Renames an existing key. | `RENAME old_name new_name` |

### List Commands

Lists store ordered string elements and can be used for simple queues, task lists, and ordered collections.

| Command | Description | Example |
|---|---|---|
| `LPUSH` | Adds an element to the beginning of a list. | `LPUSH fruits apple` |
| `RPUSH` | Adds an element to the end of a list. | `RPUSH fruits orange` |
| `LPOP` | Removes and returns the first element. | `LPOP fruits` |
| `RPOP` | Removes and returns the last element. | `RPOP fruits` |
| `LLEN` | Returns the number of elements in a list. | `LLEN fruits` |
| `LINDEX` | Retrieves an element by index. | `LINDEX fruits 0` |
| `LSET` | Replaces an element at a specified index. | `LSET fruits 0 pear` |
| `LREM` | Removes matching elements according to a count. | `LREM fruits 1 apple` |
| `LRANGE` | Returns a range of elements; the stop index is inclusive. | `LRANGE fruits 0 -1` |

For list commands that support negative indexes, `-1` refers to the last element. For example, `LRANGE fruits 0 -1` is the usual form for requesting the entire list.

### Hash Commands

Hash operations are part of the planned scope where applicable. Consider them implemented only after the corresponding database methods, command handlers, and tests are complete.

| Command | Description | Example |
|---|---|---|
| `HSET` | Sets a field in a hash. | `HSET user:1 name Alice` |
| `HGET` | Retrieves one field. | `HGET user:1 name` |
| `HEXISTS` | Checks whether a field exists. | `HEXISTS user:1 name` |
| `HDEL` | Deletes a field. | `HDEL user:1 name` |
| `HGETALL` | Retrieves all fields and values. | `HGETALL user:1` |
| `HKEYS` | Retrieves all field names. | `HKEYS user:1` |
| `HVALS` | Retrieves all field values. | `HVALS user:1` |
| `HLEN` | Returns the number of fields. | `HLEN user:1` |
| `HMSET` | Sets multiple fields, if implemented. | `HMSET user:1 name Alice city Delhi` |

---

## Requirements

- A C++ compiler compatible with the C++ standard used by the project.
- `make` for the current Makefile-based build process.
- Linux or another POSIX-like environment for the current socket implementation.
- The companion custom client, if using the project's own client executable.

Check the `Makefile` and source code for exact compiler requirements, dependencies, and executable names.

---

## Build and Run

### 1. Clone the Repository

```bash
git clone https://github.com/Afzal14786/redis.git
cd redis
```

### 2. Build the Server

Run:

```bash
make clean && make
```

This cleans previous build artifacts and builds the project using the repository's `Makefile`.

### 3. Start the Server

Run the executable produced by the build process. For example, if the executable is named `redis_server`:

```bash
./redis_server
```

Replace `redis_server` with the actual executable name produced by the project if it differs.

The current local setup uses port `6378` by default. Check the source code or configuration if the port changes.

### 4. Run the Custom Client

The companion client repository is:

https://github.com/Afzal14786/redis_cli

Clone and build the client according to its own instructions.

In the current local setup, the client executable is named `my_redis_cli`. If it accepts a host and port as arguments, the command may resemble:

```bash
./my_redis_cli 127.0.0.1 6378
```

Check the client's argument handling to confirm the exact invocation.

---

## Usage Examples

Once the server and client are running, try the following commands individually.

### String and Key Operations

```text
PING
ECHO hello
SET name Afzal
GET name
TYPE name
KEYS *
RENAME name username
GET username
DEL username
GET username
```

Expected behavior at a high level:

- `PING` returns a successful ping response.
- `ECHO hello` returns the supplied message.
- `SET` stores a value, and `GET` retrieves it.
- `TYPE` reports the key's data type.
- `RENAME` changes the key name.
- `DEL` removes the key.
- The final `GET` should return a null result because the key was deleted.

### List Operations

```text
RPUSH fruits apple
RPUSH fruits banana
LPUSH fruits orange
LLEN fruits
LRANGE fruits 0 -1
LINDEX fruits 0
LSET fruits 0 pear
LPOP fruits
RPOP fruits
```

Check that:

- Push operations report the resulting list length.
- `LLEN` returns the current list length.
- `LRANGE` returns the requested elements in the correct order.
- `LINDEX` retrieves the expected element.
- `LSET` updates the requested element.
- Pop operations return and remove the expected elements.

### Expiration

```text
SET session data
EXPIRE session 5
GET session
```

Run `GET session` again after the five-second time-to-live has elapsed. The key should no longer be returned if expiration is correctly enforced.

### Persistence

1. Store a few keys.
2. Shut down the server using the supported shutdown process.
3. Restart the server.
4. Check whether the stored values have been restored.

Persistence should be tested explicitly. A successful `SET` response alone does not prove that the data has been written to disk.

---

## Testing

Use the following checklist while developing the server.

- [ ] The server starts and listens on the expected port.
- [ ] The client connects successfully.
- [ ] `PING` returns a valid response.
- [ ] `ECHO` returns the supplied message.
- [ ] `SET` and `GET` work for existing keys.
- [ ] `GET` handles missing keys correctly.
- [ ] `DEL` reports whether a key was deleted.
- [ ] `TYPE` returns the expected data type.
- [ ] List push and pop operations work correctly.
- [ ] `LLEN`, `LINDEX`, `LSET`, `LREM`, and `LRANGE` behave correctly.
- [ ] Missing or invalid arguments return errors instead of hanging.
- [ ] Expired keys are not returned.
- [ ] Data is restored after a restart when persistence is enabled.
- [ ] Multiple clients can access the database without corrupting shared state.

---

## Project Structure

The exact file layout may change as the project evolves. Conceptually, the responsibilities are separated as follows:

```text
├── concepts.txt
├── Concepts_UseCases.md
├── dump.my_rdb
├── include
│   ├── RedisCommandHandler.h
│   ├── RedisDatabase.h
│   └── RedisServer.h
├── LEARNING.md
├── Makefile
├── my_redis_server
├── README.md
└── src
    ├── main.cpp
    ├── RedisCommandHandler.cpp
    ├── RedisDatabase.cpp
    └── RedisServer.cpp
```

Refer to the actual repository files for current filenames and directory structure.

---

## Current Scope and Limitations

This is a learning project and is not intended to be production-ready or fully REDIS-compatible.

Potential limitations while development continues include:

- Some commands may not yet be implemented or fully tested.
- Only a subset of command arguments and options may be supported.
- Whitespace-based parsing may limit values containing spaces unless quoting or RESP array parsing handles them correctly.
- TCP reads can split or combine requests, so robust message framing is important.
- Expiration must be enforced consistently across supported data types.
- Persistence requires careful error handling and safe shutdown behavior.
- `UNLINK` may act as a simple alias for `DEL` rather than using REDIS's asynchronous deletion semantics.
- Authentication, access control, replication, clustering, and production-grade operational features are outside the current basic scope.

---

## Future Improvements

- Complete and test the basic command set.
- Improve RESP parsing and support values containing spaces and binary data.
- Add tests for malformed requests and boundary conditions.
- Strengthen multi-client concurrency and shutdown handling.
- Improve persistence reliability and recovery behavior.
- Complete hash commands and additional data types.
- Add clearer logging and error reporting.
- Document design decisions, trade-offs, and implementation details.
- Improve compatibility with standard REDIS client behavior.

---

## Developer Information

**Developer:** Md Afzal Ansari

- **GitHub:** https://github.com/afzal14786
- **LinkedIn:** https://linkedin.com/in/0x4f5a4c
- **LeetCode:** https://leetcode.com/u/0x4f5a4c/
- **Email:** mdafzal14777@gmail.com

---

Built as a hands-on project to learn C++, networking, data structures, and server-side system design.

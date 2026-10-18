# Redis — Technical Documentation

<div align="center">
  <img src="../assets/redis.svg" alt="Redis project logo" width="180" />

  <h3>A Redis-Inspired In-Memory Key-Value Store Built with C++</h3>

  <p>
    Technical documentation covering architecture, protocol design,
    command execution, concurrency, persistence, and expiration.
  </p>
</div>

---

## Overview

This documentation explains the internal design and implementation of a Redis-inspired server written in C++.

The project implements a TCP server that accepts client requests, parses Redis-style commands, executes operations against an in-memory database, and returns protocol-formatted responses. It supports string, list, and hash data structures, key expiration, and file-based persistence.

The purpose of this documentation is to explain not only **what the system does**, but also **how it works, why it is designed this way, and what trade-offs or limitations exist in the current implementation**.

## Documentation Index

The documents are organized by system responsibility, from the overall architecture to individual command behavior and operational concerns.

### 1. System Architecture

Understand the major components and how they interact.

| Document | Description |
|---|---|
| [Architecture](./architecture.md) | High-level system architecture, component responsibilities, and interactions between the server, command handler, and database. |
| [Request Lifecycle](./request-lifecycle.md) | The journey of a client request from TCP connection and parsing through command execution and response transmission. |
| [Concurrency and Threading](./concurrency-and-threading.md) | Client worker threads, shared resources, mutex protection, background persistence, and concurrency-related limitations. |

### 2. Protocol and Command Execution

Understand how clients communicate with the server and how requests are interpreted.

| Document | Description |
|---|---|
| [Protocol and Command Format](./protocol-and-command-format.md) | Supported request formats, RESP parsing, command normalization, response encoding, and protocol limitations. |
| [Command Reference](./command-reference.md) | Supported commands, syntax, argument rules, responses, and implementation-specific behavior. |
| [Error Handling](./error-handling.md) | Command validation, malformed requests, database operation results, socket failures, and known error-handling limitations. |

### 3. Storage and Data Lifecycle

Understand how data is stored, expires, and survives server restarts.

| Document | Description |
|---|---|
| [Persistence and Expiration](./persistence-and-expiration.md) | Database dump and load operations, the persistence file format, TTL management, expiration cleanup, and related limitations. |


### 4. Project Interview Preparation

Prepare to explain the entire project confidently in SDE interviews, from its architecture and implementation choices to its limitations and future improvements.

| Document | Description |
|---|---|
| [Project Interview Questions](./project-interview-questions.md) | A comprehensive collection of project-related interview questions with detailed answers covering system architecture, C++ design, TCP networking, command parsing, data structures, concurrency, TTL and expiration, persistence, error handling, performance, design trade-offs, limitations, and potential improvements. |

---

## System at a Glance

The project separates network communication, command processing, and data storage into distinct components.

```markdown
                    ┌──────────────────────┐
                    │      Redis Client    │
                    └──────────┬───────────┘
                               │
                               │ TCP
                               ▼
                    ┌──────────────────────┐
                    │     RedisServer      │
                    │  Accept / Receive /  │
                    │        Send          │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │ RedisCommandHandler  │
                    │ Parse / Validate /   │
                    │ Dispatch / Respond   │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │    RedisDatabase     │
                    │                      │
                    │   String Store       │
                    │   List Store         │
                    │   Hash Store         │
                    │   Expiration Map     │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │    dump.my_rdb       │
                    │  File Persistence    │
                    └──────────────────────┘
```

### Core components

- **`RedisServer`** — Manages TCP connections and client communication.
- **`RedisCommandHandler`** — Parses requests, validates command arguments, dispatches operations, and formats responses.
- **`RedisDatabase`** — Manages in-memory strings, lists, hashes, expiration metadata, and persistence operations.
- **`main.cpp`** — Configures the port, loads persisted data, starts the periodic persistence thread, and runs the server.

For the complete explanation, start with [Architecture](./architecture.md).

---

## Features

- TCP server for client-server communication.
- Redis-style command processing.
- String, list, and hash data structures.
- Key expiration using time-to-live (TTL).
- File-based database persistence.
- Concurrent client handling using worker threads.
- Mutex-protected database operations.
- RESP array-of-bulk-strings request parsing.
- Redis-style simple strings, bulk strings, arrays, integer responses, and errors.

These are features of the current implementation; they do not imply complete compatibility with official Redis.

## Project Structure

The primary source files are organized as follows:

```markdown
redis/
├── assets/
│   └── redis.svg
├── docs/
│   ├── README.md
│   ├── architecture.md
│   ├── request-lifecycle.md
│   ├── concurrency-and-threading.md
│   ├── persistence-and-expiration.md
│   ├── protocol-and-command-format.md
│   ├── command-reference.md
│   └── error-handling.md
├── include/
│   ├── RedisCommandHandler.h
│   ├── RedisDatabase.h
│   └── RedisServer.h
├── src/
│   ├── main.cpp
│   ├── RedisCommandHandler.cpp
│   ├── RedisDatabase.cpp
│   └── RedisServer.cpp
├── Makefile
└── README.md
```

This tree reflects the documented files and previously reviewed source structure. Additional project files may exist in the repository.

---

## Suggested Reading Order

For a first-time reader or interviewer, the following sequence provides a logical progression.

1. **[Architecture](./architecture.md)** — Understand the system's components and responsibilities.
2. **[Request Lifecycle](./request-lifecycle.md)** — Follow a request through the system.
3. **[Protocol and Command Format](./protocol-and-command-format.md)** — Understand request parsing and response encoding.
4. **[Command Reference](./command-reference.md)** — Explore the supported operations.
5. **[Concurrency and Threading](./concurrency-and-threading.md)** — Learn how multiple clients share the server and database.
6. **[Persistence and Expiration](./persistence-and-expiration.md)** — Understand data durability and TTL behavior.
7. **[Error Handling](./error-handling.md)** — Review failure paths, edge cases, and opportunities for improvement.

---

## Important Design Considerations

The documentation describes the current implementation, including limitations that are useful to understand when evaluating the system.

- **TCP framing:** A TCP read does not necessarily correspond to a complete command. The current request-handling approach does not fully solve stream framing.
- **Protocol compatibility:** The parser supports a subset of RESP rather than the complete Redis protocol.
- **Concurrency:** Database operations use mutex protection, but this does not make arbitrary sequences of commands atomic.
- **Expiration:** Cleanup is not consistently performed by every operation.
- **Persistence:** The dump format does not preserve TTL deadlines and does not comprehensively validate all file I/O errors.
- **Key types:** Separate data stores can contain the same key name, unlike Redis's usual mutually exclusive key-type model.
- **Shutdown:** Signal handling and worker-thread shutdown have limitations that are documented in the relevant sections.

These are documented engineering trade-offs and potential improvement areas, not claims of full Redis compatibility.

## Scope and Goals

This project provides an opportunity to explore several systems-programming concepts:

- TCP socket programming.
- Parsing and protocol design.
- Data structures and algorithmic complexity.
- Threading, synchronization, and shared-state management.
- In-memory storage design.
- TTL and expiration strategies.
- File persistence and recovery.
- Defensive programming and error handling.

The documentation is intended to support code review, debugging, future development, and technical interview preparation.

---

## Contributing to the Documentation

When documenting a new feature or modifying an existing one:

1. Describe the actual implementation before proposing improvements.
2. Explain the responsibilities of the relevant classes and functions.
3. Include request and response examples where useful.
4. Document edge cases, complexity, and concurrency implications.
5. Separate current behavior from proposed changes.
6. Update this index when adding or renaming a documentation file.

The goal is to keep the documentation aligned with the source code as the project evolves.

---

<div align="center">
  <sub>Technical documentation for a Redis-inspired C++ server.</sub>
</div>
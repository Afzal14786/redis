# Redis Server in C++

<p align="center">
  <img src="assets/redis.svg" alt="Redis-inspired C++ server" width="180" />
</p>

A Redis-inspired, in-memory data server built from scratch in C++.

**Project status:** Work in Progress

## Overview

This project explores how a database server works internally—from accepting TCP connections and processing commands to managing data, handling concurrent clients, and saving data to disk.

It is an educational project inspired by Redis, **not a complete or fully Redis-compatible replacement**.

## Features

- TCP client-server communication using POSIX sockets
- String, list, and hash data structures
- Command parsing and RESP-style responses
- Key expiration using TTL
- Concurrent client handling with C++ threads
- Mutex-protected database operations
- File-based persistence and data loading

## Getting Started

### Build

```bash
git clone https://github.com/Afzal14786/redis.git
cd redis
make clean && make
```

### Run

The server uses port `6378` by default.

```bash
./my_redis_server
```

To specify a custom port:

```bash
./my_redis_server 6380
```

### Try Some Commands

Use the [companion client](https://github.com/Afzal14786/redis_cli) to connect and try:

```text
PING
SET name Afzal
GET name
LPUSH fruits apple
LRANGE fruits 0 -1
HSET user:1 name Afzal
HGET user:1 name
```

Refer to the client repository for its build and connection instructions.

## Project Structure

```markdown
.
├── assets/       # Images and other assets
├── docs/         # Detailed project documentation
├── include/      # C++ header files
├── src/          # Server, command handler, and database
├── LEARNING.md   # Concepts learned during development
├── Makefile
└── README.md
```

## Documentation

Explore [`docs/`](./docs/README.md) for architecture, request lifecycle, concurrency, persistence, and project interview questions.

See [`LEARNING.md`](./LEARNING.md) for the concepts I learn while building the project.

## Current Limitations

This project supports a subset of Redis-like commands and RESP-style communication. It does not provide full Redis protocol compatibility. TCP message framing, graceful shutdown, consistent expiration handling, and persistence reliability are areas for improvement.

## Developer

**Md Afzal Ansari**

- [GitHub](https://github.com/Afzal14786)
- [LinkedIn](https://linkedin.com/in/0x4f5a4c)
- [LeetCode](https://leetcode.com/u/0x4f5a4c/)

---

*Built to learn C++, networking, data structures, concurrency, and database server fundamentals.*
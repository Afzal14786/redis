
# Concepts & Use Cases

This document explains the main concepts, classes, and commands in this Redis-inspired key-value server. It is a project-development note, separate from the main `README.md`.

> **Project scope:** The current goal is to complete a small working version first. Advanced Redis compatibility, extra options, and edge-case refinements can be added later. A command listed as planned should not be considered implemented yet.

---

## 1. Core Concepts

1. **TCP/IP and socket programming**  
   The server listens on a TCP port, accepts client connections, receives command data, and sends responses back over sockets.

2. **Concurrency and multithreading**  
   The server may handle multiple client connections. Shared database state must be accessed safely when requests run concurrently.

3. **Mutexes and synchronization**  
   A mutex protects shared data structures from simultaneous unsafe access. For example, database methods can lock `db_mutex` while reading or modifying stored data.

4. **Data structures**  
   - **Hash maps:** Store keys and string values, and can also index lists or hashes by key.
   - **Vectors/lists:** Hold ordered collections of values and command tokens.
   - **Strings:** Represent command names, keys, values, and protocol data.

5. **RESP protocol parsing**  
   Redis clients communicate using RESP. The command handler parses requests and formats responses such as simple strings (`+OK\r\n`), errors (`-ERR ...\r\n`), integers (`:1\r\n`), bulk strings (`$3\r\nfoo\r\n`), and arrays (`*2\r\n...`).

6. **File I/O and persistence**  
   Persistence saves database data to a file and loads it again when the server starts. Loading and saving should be checked carefully so an empty or failed load does not unintentionally overwrite valid saved data.

7. **Signal handling and shutdown**  
   Shutdown handling can be used to stop the server cleanly and save state before exiting.

8. **Command dispatch and response formatting**  
   `RedisCommandHandler` identifies the command, routes it to the correct handler, and returns the handler's response to the server.

9. **Singleton pattern**  
   `RedisDatabase::getInstance()` provides access to the shared database instance used by command handlers.

10. **Bitwise/compound assignment (`|=`)**  
    For boolean values, `erased |= condition` accumulates whether any operation succeeded while still evaluating each condition.

11. **C++ standard library**  
    Common facilities include `std::string`, `std::vector`, `std::unordered_map`, `std::mutex`, `std::lock_guard`, `std::chrono`, `std::istringstream`, and `std::ostringstream`.

---

## 2. Main Classes

### `RedisServer`

Responsible for the networking layer: creating/listening on a socket, accepting clients, receiving requests, and sending responses.

### `RedisCommandHandler`

Responsible for parsing commands, validating their arguments, routing them to command-specific handlers, and formatting protocol responses.

### `RedisDatabase`

Responsible for storing and retrieving data. The current design separates string values, lists, and hashes into different stores and includes expiration metadata for keys.

### Typical request flow

```text
Client
  |
  | TCP request (RESP command)
  v
RedisServer
  |
  | request data
  v
RedisCommandHandler
  |
  | route to command handler
  v
RedisDatabase
  |
  | result
  v
RESP response -> RedisServer -> Client
```

---

## 3. Commands and Use Cases

The examples below show the intended basic command syntax. The current implementation may support only the arguments described here; Redis options and advanced behavior are out of scope for the first version.

### 3.1 Common commands

| Command | Purpose | Example |
|---|---|---|
| `PING` | Check whether the server responds. | `PING` |
| `ECHO` | Return a supplied message; useful for a basic request/response test. | `ECHO hello` |
| `FLUSHALL` | Clear stored data. Use carefully because it removes all keys. | `FLUSHALL` |

### 3.2 String and key operations

| Command | Purpose | Example |
|---|---|---|
| `SET` | Store a string value under a key. | `SET course cpp` |
| `GET` | Retrieve the string value for a key. | `GET course` |
| `KEYS` | Return the keys currently stored in the database. Pattern matching may not be supported in the basic implementation. | `KEYS *` |
| `TYPE` | Report the stored type, such as string, list, hash, or none. | `TYPE course` |
| `DEL` / `UNLINK` | Delete a key. The current basic implementation handles one key per command. | `DEL course` |
| `EXPIRE` | Set a key's time-to-live in seconds. Expiration must also be enforced when the key is accessed. | `EXPIRE course 60` |
| `RENAME` | Rename an existing key. Expiration metadata should move with the key. | `RENAME old_name new_name` |

### 3.3 List operations

Lists store ordered string elements. They can be used for simple queues, task lists, or ordered collections.

| Command | Purpose | Example |
|---|---|---|
| `LLEN` | Return the number of elements in a list. | `LLEN fruits` |
| `LPUSH` | Insert an element at the beginning of a list. | `LPUSH fruits apple` |
| `RPUSH` | Insert an element at the end of a list. | `RPUSH fruits orange` |
| `LPOP` | Remove and return the first element. | `LPOP fruits` |
| `RPOP` | Remove and return the last element. | `RPOP fruits` |
| `LREM` | Remove matching elements according to a count and direction. | `LREM fruits 1 apple` |
| `LINDEX` | Retrieve the element at an index. | `LINDEX fruits 0` |
| `LSET` | Replace the element at an index. | `LSET fruits 0 pear` |
| `LRANGE` | Retrieve a range of elements; the stop index is inclusive. | `LRANGE fruits 0 -1` |

For `LINDEX`, `LSET`, and `LRANGE`, negative indexes are intended to count backward from the end of the list if implemented. `LRANGE key 0 -1` is the usual form for requesting the entire list.

### 3.4 Hash operations — planned

Hashes store field/value pairs under one key, making them useful for representing simple objects such as user profiles. These commands are documented as future scope and should only be marked implemented after their handlers and database methods are complete.

| Command | Purpose | Example |
|---|---|---|
| `HSET` | Set a field in a hash. | `HSET user:1 name Alice` |
| `HGET` | Retrieve one field. | `HGET user:1 name` |
| `HEXISTS` | Check whether a field exists. | `HEXISTS user:1 name` |
| `HDEL` | Delete a field. | `HDEL user:1 name` |
| `HGETALL` | Retrieve all fields and values. | `HGETALL user:1` |
| `HKEYS` / `HVALS` | Retrieve all field names or all field values. | `HKEYS user:1` |
| `HLEN` | Return the number of fields. | `HLEN user:1` |
| `HMSET` | Set multiple fields in one command, if included in the planned implementation. | `HMSET user:1 name Alice city Delhi` |

---

## 4. Building and Testing

Use the project's build instructions and executable names. For the current local setup, the server listens on port `6378` by default and the custom client is run from its own project directory.

### Basic string/key test

Run the server, then connect with the custom client:

```text
PING
ECHO hello
SET course cpp
GET course
TYPE course
KEYS *
DEL course
GET course
```

Expected behavior at a high level:

- `PING` returns `PONG`.
- `ECHO hello` returns `hello`.
- `SET course cpp` returns `OK`.
- `GET course` returns `cpp`.
- `TYPE course` reports the key's type.
- `DEL course` returns an integer indicating whether a key was deleted.
- The final `GET course` returns a null result because the key was deleted.

### Basic list test

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

Check that push commands report the resulting list length, `LLEN` reports the current length, and list-reading commands return correctly formatted results.

### Expiration test

```text
SET session:1 data
EXPIRE session:1 5
GET session:1
```

Wait longer than five seconds and then run `GET session:1` again. The key should no longer be returned once expiration is enforced by the database.

### Persistence test

1. Store a few values.
2. Shut down the server using its supported shutdown method.
3. Restart it.
4. Check whether the values were restored.

Persistence should be tested separately from command handling because a successful command response does not by itself prove that the data was saved correctly.

---

## 5. Current Development Priorities

1. Finish the basic string/key and list database methods.
2. Ensure every command handler returns its response to `processCommand()` and the server sends that response to the client.
3. Test valid commands, missing arguments, missing keys, and empty lists.
4. Verify expiration and persistence behavior.
5. Implement hash operations after the current scope is stable.
6. Improve Redis compatibility and edge cases after the minimum working version is complete.

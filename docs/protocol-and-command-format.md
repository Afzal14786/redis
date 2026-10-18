# Protocol and Command Format

## 1. Overview

The protocol defines how a client communicates with the server by sending commands and receiving responses.

In this project, the communication flow consists of three main steps:

1. **Request parsing:** The server receives bytes from a client and converts the request into a sequence of command tokens.
2. **Command processing:** `RedisCommandHandler` identifies the command and invokes the corresponding database operation.
3. **Response formatting:** The command handler converts the result into a response string that the client can interpret.

The project supports a subset of Redis-style commands and response formats. Although it recognizes RESP arrays of bulk strings, it does not implement the complete Redis protocol.

## 2. Protocol-Related Components

The protocol implementation is distributed across two main components.

### 2.1 `RedisServer`

File: `src/RedisServer.cpp`

The server is responsible for network communication.

Its responsibilities include:

- Accepting incoming TCP connections.
- Receiving request bytes using `recv()`.
- Passing the received data to `RedisCommandHandler::processCommand()`.
- Sending the resulting response using `send()`.
- Closing the client connection when communication ends.

The server does not interpret the meaning of individual Redis commands itself. It delegates command processing to the command handler.

### 2.2 `RedisCommandHandler`

Files:
- `include/RedisCommandHandler.h`
- `src/RedisCommandHandler.cpp`

The command handler is responsible for interpreting requests and generating responses.

Its responsibilities include:

- Parsing incoming request data.
- Extracting the command name and arguments.
- Normalizing command names to uppercase.
- Validating command argument counts.
- Executing the appropriate command logic.
- Formatting results using Redis-style response prefixes.
- Returning error responses for invalid or unsupported commands.

## 3. Request Parsing

### 3.1 `parseRespCommands()`

The `parseRespCommands()` function converts the incoming request string into a vector of strings.

For example, the request:

```bash
SET username Afzal
```

can be represented internally as:

```cpp
{"SET", "username", "Afzal"}
```

The command handler can then use the first element as the command name and the remaining elements as arguments.

### 3.2 RESP Array-of-Bulk-Strings Format

The parser supports RESP arrays containing bulk strings.

For example, the following request represents the command `SET username Afzal`:

```bash
*3\r\n
$3\r\n
SET\r\n
$8\r\n
username\r\n
$5\r\n
Afzal\r\n
```

The components mean:

- `*3` — the request contains three array elements.
- `$3` — the next bulk string contains three bytes.
- `SET` — the command name.
- `$8` — the next bulk string contains eight bytes.
- `username` — the key.
- `$5` — the next bulk string contains five bytes.
- `Afzal` — the value.
- `\r\n` — carriage-return and line-feed delimiters.

The parser extracts the bulk-string contents and produces the same token vector:

```cpp
{"SET", "username", "Afzal"}
```

This lets the command handler process a RESP-formatted request without needing separate command logic for each request format.

**Implementation detail:** The parser validates the structure of the supported RESP request format, including lengths, bounds, and expected CRLF delimiters. It also rejects malformed RESP input and trailing data that does not belong to the parsed request.

### 3.3 Whitespace-Separated Command Format

The parser also supports a simpler, whitespace-separated representation:

```bash
SET username Afzal
```

The parser splits the input into tokens using whitespace.

This format is convenient for manual testing and simple clients, but it has limitations. In particular, it cannot reliably represent arguments containing spaces as a single argument.

For example:

```bash
SET greeting Hello World
```

is interpreted as four tokens:

```cpp
{"SET", "greeting", "Hello", "World"}
```

The handler's `SET` implementation uses the third token as the value and ignores additional tokens. Consequently, the stored value is `Hello`, not `Hello World`.

RESP bulk strings can represent spaces inside an argument because the argument length is explicitly specified.

## 4. Request Processing Flow

Consider the command:

```bash
SET username Afzal
```

The request follows this sequence:

1. A client connects to the TCP server.
2. The server accepts the connection.
3. The server receives bytes from the client using `recv()`.
4. The received bytes are passed to `RedisCommandHandler::processCommand()`.
5. The command handler parses the request into tokens.
6. The command name is normalized to uppercase.
7. The handler validates the arguments and identifies the `SET` operation.
8. The handler calls the database's `set()` method.
9. The database stores the key-value pair.
10. The handler creates the response `+OK\r\n`.
11. The server sends the response to the client.

The database and command handler are separate from the socket-handling logic, which helps keep networking, command interpretation, and storage responsibilities distinct.

## 5. Command Name Normalization

The command handler converts the command name to uppercase before dispatching it.

For example:

```bash
SET username Afzal
set username Afzal
Set username Afzal
```

All three forms resolve to the `SET` command.

This behavior applies to command names. It does not imply that keys, values, or hash fields are converted to uppercase.

The handler uses the normalized command name to select the relevant command implementation.

## 6. Command Argument Validation

Each command performs its own argument-count validation.

The current implementation does not use one universal argument-validation rule. Depending on the command, it may require an exact number of tokens or only a minimum number.

Examples:

| Command | Argument-count behavior |
|---|---|
| `PING` | Ignores the supplied token count and returns `PONG` |
| `ECHO` | Requires exactly two tokens |
| `SET` | Requires at least three tokens; extra tokens are ignored |
| `GET` | Requires at least two tokens; extra tokens are ignored |
| `DEL` / `UNLINK` | Requires a key token; deletes only the first supplied key |
| `EXPIRE` | Requires exactly three tokens |
| `LINDEX` | Requires exactly three tokens |
| `LRANGE` | Requires exactly four tokens |
| `HSET` | Requires exactly four tokens |
| `HMSET` | Requires a key and complete field-value pairs |

These behaviors describe the current project, not necessarily the behavior of official Redis.

Argument validation helps prevent handlers from accessing missing tokens, but the validation policy is not uniform across every command.

## 7. Response Format

The command handler returns responses as strings. These strings use several Redis-style response prefixes.

### 7.1 Simple Strings

A simple string begins with `+` and ends with CRLF.

Example:

```bash
+OK\r\n
```

Used for successful operations such as `SET`, `FLUSHALL`, and `RENAME`, as well as `PING`:

```bash
+PONG\r\n
```

### 7.2 Errors

An error response begins with `-` and ends with CRLF.

Example:

```bash
-ERR unknown command\r\n
```

The exact error message depends on the command handler's implementation.

### 7.3 Integer Responses

An integer response begins with `:`.

Example:

```bash
:1\r\n
```

Integer responses are used by operations such as `DEL`, `EXISTS`-style checks where implemented, and hash/list operations that return counts or status values.

For this project's `DEL`, the result is `:1\r\n` if the key existed and was deleted, or `:0\r\n` otherwise. The current `DEL` implementation deletes only one key per command.

### 7.4 Bulk Strings

A bulk string starts with `$`, followed by the byte length, CRLF, the string content, and a final CRLF.

Example:

```bash
$5\r\n
Afzal\r\n
```

A bulk string is useful for returning values that may contain spaces or other characters.

### 7.5 Null Bulk Strings

A missing value can be represented by:

```bash
$-1\r\n
```

For example, `LPOP` and `RPOP` return a null bulk string when the requested list does not exist or has no remaining element.

### 7.6 Arrays

An array starts with `*`, followed by the number of elements and CRLF.

Example:

```bash
*2\r\n
$5\r\n
hello\r\n
$5\r\n
world\r\n
```

Array responses are used by commands that return collections, such as `LGET`, `LRANGE`, `HGETALL`, `HKEYS`, and `HVALS`.

The individual array elements are encoded according to the response construction used by the relevant handler.

## 8. Example Command and Response Pairs

The following examples illustrate common request and response patterns.

| Command | Example response | Meaning |
|---|---|---|
| `PING` | `+PONG\r\n` | Server responds to a ping |
| `SET username Afzal` | `+OK\r\n` | Key-value pair stored |
| `GET username` | `$5\r\nAfzal\r\n` | Returns the stored value |
| `TYPE username` | `+string\r\n` | Reports the key's type |
| `DEL username` | `:1\r\n` | One key deleted |
| `EXPIRE username 60` | `:1\r\n` | Expiration configured successfully |
| `LLEN tasks` | `:0\r\n` | List length, if the list is missing or empty |
| Unknown command | `-ERR unknown command\r\n` | Command is unsupported |

These examples show response shapes; the exact response depends on the actual stored data and the command implementation.

## 9. Important Protocol Limitations

The current protocol implementation has several limitations that matter when using standard Redis clients or handling concurrent network traffic.

### 9.1 TCP Does Not Preserve Command Boundaries

TCP is a byte stream. It does not guarantee that one `recv()` call corresponds to one complete command.

In `RedisServer::run()`, each received buffer is passed directly to the command handler as one request. If a command arrives across multiple reads, the first chunk may be incomplete. If multiple commands arrive in one read, the parser may treat the combined data as one request.

A robust implementation needs an input buffer and protocol-aware framing so it can wait for complete commands and process multiple commands separately.

### 9.2 No Command Pipelining

Although RESP supports sending multiple commands without waiting for each response, the current parser expects a single request representation and rejects trailing data after the parsed request.

The server therefore does not implement complete Redis-style command pipelining.

### 9.3 Partial RESP Support

The parser supports RESP arrays of bulk strings, but the project does not implement every RESP feature or response type.

For example, compatibility with all standard Redis clients cannot be assumed solely because some RESP syntax is supported.

### 9.4 Whitespace Fallback Is Not a Complete Protocol

Whitespace-separated parsing is convenient for basic testing, but it is not sufficient for arbitrary string values. Values containing spaces or protocol-sensitive characters should be represented using a supported RESP request.

### 9.5 Socket Send Handling

The server calls `send()` once for the generated response and does not handle partial writes. A successful call is not guaranteed to transmit every response byte.

A more robust implementation would continue sending until the complete response has been transmitted or an error occurs.

## 10. Potential Improvements

Possible improvements to the protocol layer include:

1. Implement a persistent per-client input buffer.
2. Parse complete commands from the buffer instead of treating each `recv()` call as a complete request.
3. Support multiple RESP commands in one connection buffer.
4. Add more complete RESP validation and response-type support.
5. Standardize argument validation and error messages.
6. Handle partial socket writes.
7. Add protocol tests for malformed requests, empty arguments, spaces inside values, incomplete requests, and multiple commands received together.
8. Document the exact supported request and response formats for client authors.

These are potential improvements rather than descriptions of the current implementation.

## 11. Summary

The protocol layer connects network communication with command execution. `RedisServer` receives and sends bytes, `RedisCommandHandler` parses and dispatches commands, and `RedisDatabase` performs the storage operations.

The project supports a subset of RESP arrays of bulk strings, a whitespace-separated fallback, and several Redis-style response formats. Its most important protocol limitation is that the server treats each `recv()` result as a complete request rather than implementing TCP stream framing.

Understanding this distinction is important both for debugging the client-server communication and for explaining the project's protocol design and limitations in an SDE interview.
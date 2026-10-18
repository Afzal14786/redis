# Command Reference

## 1. Overview

This document describes the commands supported by the Redis-like server implemented in C++.

The command handler is responsible for parsing requests, validating arguments, executing command logic, and generating responses. The database layer performs the underlying string, list, hash, deletion, and expiration operations.

### Supported command categories

| Category | Commands |
|---|---|
| Connection and utility | `PING`, `ECHO`, `FLUSHALL` |
| String operations | `SET`, `GET`, `KEYS`, `TYPE`, `DEL`, `UNLINK`, `EXPIRE`, `RENAME` |
| List operations | `LPUSH`, `RPUSH`, `LGET`, `LLEN`, `LPOP`, `RPOP`, `LREM`, `LINDEX`, `LSET`, `LRANGE` |
| Hash operations | `HSET`, `HGET`, `HEXISTS`, `HDEL`, `HGETALL`, `HKEYS`, `HVALS`, `HLEN`, `HMSET` |

The command names are normalized to uppercase before dispatch. The server supports a subset of Redis-style commands and does not implement every Redis feature.

## 2. How to Read This Reference

Each command includes:

- **Syntax:** The command and its arguments.
- **Purpose:** What the command does.
- **Argument rules:** How the current implementation validates the request.
- **Response:** The general response format.
- **Implementation notes:** Important behavior or limitations.

The syntax examples use a whitespace-separated representation for readability. The parser also supports RESP arrays of bulk strings.

The response examples show Redis-style response formats. Verify exact error text and collection ordering against the current source code when writing automated tests.

---

## 3. Connection and Utility Commands

### 3.1 `PING`

**Syntax**

```bash
PING
```

**Purpose**

Checks whether the server responds to a request.

**Response**

```bash
+PONG\r\n
```

**Argument rules**

The current handler ignores the number of supplied tokens and returns `PONG`.

**Implementation notes**

This is a basic server-response check. It does not perform a database operation or verify that every other subsystem is functioning correctly.

### 3.2 `ECHO`

**Syntax**

```bash
ECHO <message>
```

**Purpose**

Returns the supplied message.

**Argument rules**

Requires exactly two tokens: the command and one message argument.

**Implementation notes**

The handler uses the second token as the message. To represent a message containing spaces as one argument, use the supported RESP bulk-string format rather than relying on whitespace splitting.

### 3.3 `FLUSHALL`

**Syntax**

```bash
FLUSHALL
```

**Purpose**

Removes all stored strings, lists, hashes, and expiration metadata.

**Response**

```bash
+OK\r\n
```

**Implementation notes**

The database's `flushAll()` operation clears all three data stores and the expiration map under the database mutex.

This operation affects the entire database, not just one key. It should be used carefully when testing.

---

## 4. String and Key Commands

### 4.1 `SET`

**Syntax**

```bash
SET <key> <value>
```

**Purpose**

Stores a string value under the specified key.

**Example**

```bash
SET username Afzal
```

**Response**

```bash
+OK\r\n
```

**Argument rules**

Requires at least three tokens. Additional tokens are ignored by the handler.

**Implementation notes**

- The value is taken from the third token.
- Setting a key clears that key's existing expiration metadata.
- The current implementation writes to the string store but does not remove a same-named entry from the list or hash store.
- Consequently, a key name may exist in multiple stores, which can lead to behavior that differs from Redis.

### 4.2 `GET`

**Syntax**

```bash
GET <key>
```

**Purpose**

Retrieves the string value associated with a key.

**Example**

```bash
GET username
```

**Example response**

```bash
$5\r\nAfzal\r\n
```

**Argument rules**

Requires at least two tokens. Additional tokens are ignored.

**Implementation notes**

The database checks the requested key's expiration before returning its string value. A missing or expired string does not produce a stored value.

Check the handler's exact missing-key response when writing client tests.

### 4.3 `KEYS`

**Syntax**

```bash
KEYS
```

**Purpose**

Returns the keys currently found in the database's string, list, and hash stores.

**Argument rules**

The handler ignores the supplied tokens; no key pattern is required.

**Implementation notes**

- The database removes expired keys before collecting the results.
- Keys are collected from each store separately.
- If the same name exists in more than one store, duplicate names can appear.
- Pattern matching, such as Redis's `KEYS user:*`, is not implemented by the current handler.

This is a simplified key-listing command, not a full implementation of Redis `KEYS`.

### 4.4 `TYPE`

**Syntax**

```bash
TYPE <key>
```

**Purpose**

Reports the detected data type of a key.

**Possible response values**

```bash
+string\r\n
+list\r\n
+hash\r\n
+none\r\n
```

**Argument rules**

Requires at least two tokens. Additional tokens are ignored.

**Implementation notes**

The database checks stores in this order:

1. String store
2. List store
3. Hash store

It returns the first matching type. Expiration is checked for the requested key before the type is determined.

If a same-named key exists in multiple stores, the reported type reflects this lookup order rather than a single mutually exclusive Redis key type.

### 4.5 `DEL`

**Syntax**

```bash
DEL <key>
```

**Purpose**

Deletes a key from the database.

**Example response when a key exists**

```bash
:1\r\n
```

**Example response when a key does not exist**

```bash
:0\r\n
```

**Argument rules**

The handler requires a key token and processes only the first supplied key.

**Implementation notes**

- The database checks whether the key exists in any store.
- It then removes the key from the string, list, and hash stores, along with its expiration metadata.
- The database deletion method does not first purge the requested key if it has expired. Therefore, an expired entry that has not yet been cleaned up can affect the reported deletion result.

Unlike official Redis, this implementation does not delete multiple keys through one `DEL` request.

### 4.6 `UNLINK`

**Syntax**

```bash
UNLINK <key>
```

**Purpose**

Deletes a key using the same database deletion operation used by `DEL`.

**Response**

Returns an integer-style response indicating whether a key was deleted.

**Argument rules**

The handler processes only the first supplied key.

**Implementation notes**

Despite the name, the current implementation does not implement Redis's asynchronous memory-reclamation behavior for `UNLINK`. It behaves like the project's `DEL` command.

### 4.7 `EXPIRE`

**Syntax**

```bash
EXPIRE <key> <seconds>
```

**Purpose**

Sets a time-to-live (TTL) for a key.

**Example**

```bash
EXPIRE username 60
```

**Response**

An integer-style response indicates whether the expiration operation succeeded.

**Argument rules**

Requires exactly three tokens. The seconds argument must be a valid integer, and the handler checks that the complete argument was consumed during parsing.

**Implementation notes**

- A positive value sets an expiration deadline.
- Zero or a negative value causes immediate removal of the key.
- A missing key produces a failure result.
- Expiration deadlines use `std::chrono::steady_clock`.
- Expiration metadata is not persisted in the current dump format.

Expiration cleanup is not performed by every database operation, so an expired key may remain physically present until an operation triggers cleanup.

### 4.8 `RENAME`

**Syntax**

```bash
RENAME <old_key> <new_key>
```

**Purpose**

Renames a key.

**Argument rules**

Requires at least three tokens. Additional tokens are ignored.

**Response**

A successful rename returns a simple-string response; failures depend on the handler's error logic.

**Implementation notes**

- The database moves matching entries from the old key name to the new key name across the stores.
- Existing destination entries may be overwritten in the corresponding store.
- If the source has an expiration entry, its TTL is transferred.
- If the source has no TTL but the destination already has one, the destination TTL can remain.
- The implementation does not purge an expired source key before attempting the rename.
- If the old and new names are identical, the database reports whether data exists under that name.

These details describe the current implementation and should not be assumed to match every edge case of official Redis `RENAME`.

---

## 5. List Commands

Lists are stored as sequences of strings. The current implementation uses a C++ vector for each list.

### 5.1 `LPUSH`

**Syntax**

```bash
LPUSH <key> <value1> [value2 ...]
```

**Purpose**

Inserts one or more values at the beginning of a list.

**Example**

```bash
LPUSH tasks first second third
```

**Argument rules**

Requires at least three tokens.

**Response**

An integer-style response reports the resulting list length.

**Implementation notes**

The handler processes supplied values in order and inserts each at the front. Repeated front insertion reverses the order of multiple supplied values relative to their original sequence.

For example, starting with an empty list, the command above produces this list order:

```bash
third, second, first
```

Because the list uses a vector, inserting at the beginning requires shifting existing elements and is O(n) per insertion.

### 5.2 `RPUSH`

**Syntax**

```bash
RPUSH <key> <value1> [value2 ...]
```

**Purpose**

Appends one or more values to the end of a list.

**Example**

```bash
RPUSH tasks first second third
```

**Argument rules**

Requires at least three tokens.

**Response**

An integer-style response reports the resulting list length.

**Implementation notes**

The handler appends values in the order supplied. Appending to the end of a vector is amortized O(1) per element.

### 5.3 `LGET`

**Syntax**

```bash
LGET <key>
```

**Purpose**

Returns the elements of a list as an array response.

**Argument rules**

Requires exactly two tokens.

**Implementation notes**

`LGET` is a project-specific command name. It is not the standard Redis command for retrieving an entire list.

The database returns a copy of the list. A missing list produces an empty vector, which the handler serializes as an array response.

### 5.4 `LLEN`

**Syntax**

```bash
LLEN <key>
```

**Purpose**

Returns the number of elements in a list.

**Argument rules**

Requires at least two tokens. Additional tokens are ignored.

**Implementation notes**

The database returns zero when the list is missing. This command does not independently purge an expired key before reading the list store.

### 5.5 `LPOP`

**Syntax**

```bash
LPOP <key>
```

**Purpose**

Removes and returns the first element of a list.

**Argument rules**

Requires at least two tokens.

**Response**

The removed value is returned as a bulk string. When no element is available, the handler returns a null bulk string:

```bash
$-1\r\n
```

**Implementation notes**

If the removal leaves the list empty, the list key is removed from the list store.

Because the list is backed by a vector, removing the first element requires shifting remaining elements and is O(n).

### 5.6 `RPOP`

**Syntax**

```bash
RPOP <key>
```

**Purpose**

Removes and returns the last element of a list.

**Argument rules**

Requires at least two tokens.

**Response**

Returns the removed value as a bulk string, or a null bulk string if the list is missing or empty.

**Implementation notes**

If the list becomes empty, its key is removed from the list store. Removing the final vector element is O(1).

### 5.7 `LREM`

**Syntax**

```bash
LREM <key> <count> <value>
```

**Purpose**

Removes matching elements from a list according to the supplied count.

**Argument rules**

Requires at least four tokens. The count must be a valid integer.

**Implementation notes**

The database supports positive, negative, and zero counts. Its implementation removes matching elements by compacting the vector in place.

The intended count semantics are:

- Positive count: remove up to that many matching elements from the beginning.
- Negative count: remove up to the corresponding number of matching elements from the end.
- Zero count: remove all matching elements.

If all elements are removed, the list key is deleted from the list store.

The operation is O(n) in the list length.

### 5.8 `LINDEX`

**Syntax**

```bash
LINDEX <key> <index>
```

**Purpose**

Retrieves the element at a specified list index.

**Argument rules**

Requires exactly three tokens.

**Implementation notes**

- Zero refers to the first element.
- Positive indices count from the beginning.
- Negative indices count from the end.
- Invalid or out-of-range indices do not identify an existing element.

Verify the exact response for a missing list or invalid index against the command handler.

### 5.9 `LSET`

**Syntax**

```bash
LSET <key> <index> <value>
```

**Purpose**

Replaces the value at a specified list index.

**Argument rules**

Requires exactly four tokens.

**Implementation notes**

The database supports negative indices. The index must resolve to an existing list element; an invalid index cannot update an element.

Check the handler's error response for a missing list or invalid index before relying on a specific message in tests.

### 5.10 `LRANGE`

**Syntax**

```bash
LRANGE <key> <start> <stop>
```

**Purpose**

Returns a range of list elements as an array response.

**Argument rules**

Requires exactly four tokens.

**Implementation notes**

- Indices can be negative to count from the end of the list.
- The stop index is inclusive.
- The database handles index normalization and range boundaries.
- A missing list or range with no valid elements produces an empty result.

For example, if the list contains `a`, `b`, `c`, and `d`, then:

```bash
LRANGE tasks 1 2
```

returns the elements `b` and `c`.

---

## 6. Hash Commands

Hashes store field-value pairs under a key. The database represents each hash using a map from field names to string values.

### 6.1 `HSET`

**Syntax**

```bash
HSET <key> <field> <value>
```

**Purpose**

Creates or updates a field in a hash.

**Argument rules**

Requires exactly four tokens.

**Response**

- Returns `:1\r\n` when a new field is created.
- Returns `:0\r\n` when an existing field is updated.

**Implementation notes**

The database creates the hash if it does not exist. Updating an existing field changes its value without increasing the field count.

### 6.2 `HGET`

**Syntax**

```bash
HGET <key> <field>
```

**Purpose**

Retrieves the value associated with a field in a hash.

**Argument rules**

Requires exactly three tokens.

**Implementation notes**

The field value is returned as a bulk string when found. Verify the exact response for a missing hash or field in the handler implementation.

### 6.3 `HEXISTS`

**Syntax**

```bash
HEXISTS <key> <field>
```

**Purpose**

Checks whether a hash contains a specified field.

**Argument rules**

Requires exactly three tokens.

**Response**

An integer-style response indicates whether the field exists.

**Implementation notes**

The operation checks the requested field in the specified hash; it does not create a missing hash.

### 6.4 `HDEL`

**Syntax**

```bash
HDEL <key> <field> [field2 ...]
```

**Purpose**

Removes one or more fields from a hash.

**Argument rules**

Requires at least three tokens.

**Implementation notes**

The command can process multiple supplied fields. The database removes the hash key from the hash store when its final field is deleted.

Check the handler's response construction for the exact count returned when multiple fields are supplied.

### 6.5 `HGETALL`

**Syntax**

```bash
HGETALL <key>
```

**Purpose**

Returns the fields and values of a hash.

**Argument rules**

Requires exactly two tokens.

**Response**

An array response containing field-value data.

**Implementation notes**

The precise ordering depends on the hash container and response-building logic. Do not assume that the output preserves the order in which fields were inserted.

### 6.6 `HKEYS`

**Syntax**

```bash
HKEYS <key>
```

**Purpose**

Returns the field names in a hash as an array response.

**Argument rules**

Requires exactly two tokens.

**Implementation notes**

A missing hash produces an empty result. Ordering depends on the underlying map and response serialization.

### 6.7 `HVALS`

**Syntax**

```bash
HVALS <key>
```

**Purpose**

Returns the values stored in a hash as an array response.

**Argument rules**

Requires exactly two tokens.

**Implementation notes**

A missing hash produces an empty result. The output order follows the iteration behavior of the underlying hash container rather than field insertion order.

### 6.8 `HLEN`

**Syntax**

```bash
HLEN <key>
```

**Purpose**

Returns the number of fields in a hash.

**Argument rules**

Requires exactly two tokens.

**Implementation notes**

The database returns zero for a missing hash. The current hash operations do not independently purge expired keys before accessing the hash store.

### 6.9 `HMSET`

**Syntax**

```bash
HMSET <key> <field1> <value1> [field2 value2 ...]
```

**Purpose**

Sets multiple field-value pairs in a hash.

**Example**

```bash
HMSET user name Afzal role developer
```

**Argument rules**

The request must contain a key and complete field-value pairs. Incomplete pairs are rejected.

**Response**

A successful request returns:

```bash
+OK\r\n
```

**Implementation notes**

- Missing hashes are created.
- Existing fields are updated.
- New fields are added.
- The database applies the batch while holding one mutex lock.
- The database method returns the number of newly created fields, but the command handler returns a simple success response.

The implementation avoids calling the individually locking `hash_set()` method from inside the batch operation, which would otherwise risk deadlocking on the non-recursive mutex.

---

## 7. Argument Validation Summary

This table summarizes the current handler's token-count rules.

| Command | Current rule |
|---|---|
| `PING` | Ignores token count |
| `ECHO` | Exactly 2 tokens |
| `FLUSHALL` | No token-count validation |
| `SET` | At least 3 tokens |
| `GET` | At least 2 tokens |
| `KEYS` | Ignores tokens |
| `TYPE` | At least 2 tokens |
| `DEL` / `UNLINK` | At least 2 tokens; first key only |
| `EXPIRE` | Exactly 3 tokens |
| `RENAME` | At least 3 tokens |
| `LPUSH` / `RPUSH` | At least 3 tokens |
| `LGET` | Exactly 2 tokens |
| `LLEN` | At least 2 tokens |
| `LPOP` / `RPOP` | At least 2 tokens |
| `LREM` | At least 4 tokens |
| `LINDEX` | Exactly 3 tokens |
| `LSET` | Exactly 4 tokens |
| `LRANGE` | Exactly 4 tokens |
| `HSET` | Exactly 4 tokens |
| `HGET` / `HEXISTS` | Exactly 3 tokens |
| `HDEL` | At least 3 tokens |
| `HGETALL` / `HKEYS` / `HVALS` / `HLEN` | Exactly 2 tokens |
| `HMSET` | Key plus complete field-value pairs |

These rules reflect the current code. They should be revisited if the parser or command handler is changed.

## 8. Cross-Cutting Implementation Details

### 8.1 Expiration

Only some database operations trigger expiration cleanup. In particular, string reads and type checks perform cleanup for the requested key, while `KEYS` purges expired entries across all stores.

List and hash operations generally do not independently purge expired keys. Therefore, an expired key can remain in a list or hash store until a cleanup-triggering operation accesses or removes it.

See `docs/persistence-and-expiration.md` for details.

### 8.2 Multiple Stores and Key Names

The database maintains separate string, list, and hash stores. Some operations remove a name from all stores, but `SET` does not remove same-named list or hash entries.

As a result, the same key name can exist in more than one store. Commands such as `TYPE` and `KEYS` may expose this implementation detail.

### 8.3 Concurrency

Individual database operations are protected by a mutex. This protects access to the underlying containers, but it does not make an arbitrary sequence of multiple commands one atomic transaction.

### 8.4 Persistence

The dump format stores strings, lists, and hashes but does not persist TTL deadlines. A restart can therefore change the effective expiration behavior of previously saved keys.

See `docs/concurrency-and-threading.md` and `docs/persistence-and-expiration.md` for related details.

## 9. Recommended Command Tests

Use a clean database when testing stateful commands. For example, test:

1. `PING` and `ECHO` for basic response behavior.
2. `SET`, `GET`, `TYPE`, and `DEL` for string operations.
3. `LPUSH`, `RPUSH`, `LGET`, `LINDEX`, `LRANGE`, `LPOP`, and `RPOP` for list ordering and boundaries.
4. `HSET`, `HGET`, `HEXISTS`, `HDEL`, `HGETALL`, `HKEYS`, `HVALS`, `HLEN`, and `HMSET` for hash operations.
5. `EXPIRE` with positive, zero, and negative values.
6. Malformed RESP requests, incomplete argument lists, and invalid integer arguments.
7. Requests containing spaces in values.
8. Repeated commands and commands that use the same key name across different data types.
9. Restarting the server after saving data, to verify persistence behavior.
10. Multiple commands sent quickly over one TCP connection, to investigate framing and buffering limitations.

For every test, verify both the returned response and the resulting database state.

## 10. Summary

This command reference describes the server's supported utility, string, list, and hash commands. The handler validates each command's arguments, delegates data operations to `RedisDatabase`, and formats Redis-style responses.

The most important implementation-specific differences to remember are:

- `DEL` and `UNLINK` process only the first supplied key.
- `KEYS` does not support pattern matching.
- `LGET` is a custom command for retrieving an entire list.
- `SET` does not clear same-named entries in the other data stores.
- Expiration cleanup is not performed consistently by every command.
- The server implements only a subset of RESP and Redis command behavior.

These distinctions are useful when testing the server, debugging client interactions, and explaining the design during an SDE interview.
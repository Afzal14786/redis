# Persistence and Expiration

## 1. Overview

This document explains how the Redis-like server persists its in-memory data, restores data during startup, and manages key expiration.

The implementation has two related but separate responsibilities:

- **Persistence:** Saves strings, lists, and hashes to a local file named `dump.my_rdb` and restores them when the application starts.
- **Expiration:** Associates expiration deadlines with keys and removes expired data when database operations invoke the expiration-cleanup helpers.

The persistence mechanism is file-based. The expiration mechanism uses `std::chrono::steady_clock` to track deadlines during the current process lifetime.

An important limitation is that expiration metadata is not persisted. After loading a dump, the stored data does not automatically regain its previous TTLs.

## 2. Persistence Architecture

### 2.1 Persistence File

The application uses:

```bash
dump.my_rdb
```

as its persistence file.

The path is relative to the process's current working directory. Therefore, running the executable from different directories may cause the application to read or write different files with the same name.

The file stores records representing the database's supported data types:

- `K` records for strings.
- `L` records for lists.
- `H` records for hashes.

The current format is a simple text-based representation rather than Redis's native RDB format.

### 2.2 Responsibilities of the Database

Persistence is implemented in `RedisDatabase`, primarily through:

- `dump()` — writes the current in-memory data to a file.
- `load()` — reads records from a file and populates the in-memory stores.

The server and application entry point decide when these methods are called. The database class handles the actual file operations.

## 3. Database Startup and Loading

**File:** `src/main.cpp`

Before creating and running the server, the application attempts to restore the database:

```cpp
RedisDatabase::getInstance().load("dump.my_rdb");
```

If loading succeeds, the application reports that the database was loaded. Otherwise, it reports that no dump was found or loading failed and continues startup.

### 3.1 Loading Sequence

The current `load()` implementation follows this general sequence:

1. Acquires the database mutex.
2. Attempts to open the specified file.
3. Returns `false` if the file cannot be opened.
4. Clears the existing string, list, and hash stores.
5. Clears the existing expiration metadata.
6. Reads the file line by line.
7. Identifies each record using its type marker.
8. Parses the record and inserts its data into the corresponding store.
9. Returns `true` after the read loop completes.

The existing stores are cleared only after the file has been opened successfully. Therefore, a file-open failure returns `false` without reaching the clearing step.

### 3.2 Record Parsing

The loader recognizes the record types supported by the persistence format.

| Record marker | Meaning | Destination |
|---|---|---|
| `K` | String key and value | `kv_store` |
| `L` | List key and its elements | `list_store` |
| `H` | Hash key and its fields and values | `hash_store` |

The format uses whitespace-delimited tokens. For hash entries, the field and value are represented together using a colon (`:`), and the loader splits the entry at the first colon.

Because the representation relies on whitespace delimiters and a colon separator, values containing those delimiters may not round-trip correctly without additional escaping or a more structured format.

### 3.3 Loading Limitations

The current loader has several limitations:

- It does not fully validate every malformed record.
- It does not comprehensively report read errors that occur after the file opens.
- It does not restore expiration deadlines.
- It returns `true` after the read loop even if the stream encounters an error that is not explicitly checked.
- It replaces the existing stores after a successful file open rather than merging the file with existing data.

The return value should therefore be understood as a basic indication that the file was opened and processed, not a guarantee that every record was valid and restored correctly.

## 4. Saving the Database

**File:** `src/RedisDatabase.cpp`

The `dump()` method writes the current database contents to the requested file.

### 4.1 Dumping Sequence

The current implementation follows this sequence:

1. Acquires the database mutex.
2. Opens the output file in binary mode.
3. Returns `false` if opening the file fails.
4. Writes string records.
5. Writes list records.
6. Writes hash records.
7. Returns `true` after the write operations if the file was opened successfully.

Opening the file in binary mode does not make the persistence format a binary format; the current records are written as text.

### 4.2 Consistency During a Dump

The dump operation holds the database mutex while reading and writing the stored data.

Other database methods that acquire the same mutex must wait while the dump is in progress. This prevents those operations from modifying the protected stores during the dump.

However, it also means a slow disk write can increase the latency of client commands that need the same mutex.

### 4.3 Dumping Limitations

The current implementation has important limitations:

- It does not call the all-keys expiration-cleanup helper before writing records.
- Expired entries that have not yet been removed may therefore be included in the dump.
- It does not persist TTL or expiration-deadline metadata.
- It checks whether the file opened successfully but does not comprehensively check the stream state after writing.
- It does not implement a transactional file-replacement protocol or guarantee crash-safe persistence.

Consequently, `dump()` returning `true` does not guarantee that every record was written successfully or that the file is safe against every form of interruption.

## 5. When Persistence Occurs

The application attempts to persist data through several execution paths.

### 5.1 Startup

`main.cpp` attempts to load `dump.my_rdb` before starting the server.

### 5.2 Periodic Background Persistence

A background thread in `main.cpp` sleeps for 300 seconds and then calls `dump()`.

This corresponds to a five-minute interval between attempts, assuming each iteration completes normally.

The thread is detached and does not have an explicit stop mechanism in the current implementation.

### 5.3 Shutdown and Server Exit

The server also attempts to dump the database:

- During `RedisServer::shutdown()`, when the listening socket descriptor is valid.
- At the end of `RedisServer::run()`, after the accept loop exits and the recorded worker threads have been joined.

These are separate dump attempts. A shutdown can therefore trigger more than one persistence operation.

### 5.4 Persistence Is Not Immediate

A successful write command updates the in-memory database. It does not directly trigger a disk dump.

If the application terminates unexpectedly before the next successful dump, recent changes may be absent from the persistence file.

The current design is periodic, file-based persistence rather than a write-ahead log or per-command durable storage mechanism.

## 6. Expiration Architecture

Expiration allows a key to become invalid after a specified duration.

The database maintains an `expiry_map` that associates keys with deadlines represented using `std::chrono::steady_clock`.

The use of a monotonic clock is appropriate for measuring elapsed durations because the clock is not intended to follow changes to the system's wall-clock time.

### 6.1 Setting an Expiration

The command handler for `EXPIRE` parses the supplied duration and calls the database's `expire()` method.

For a positive duration, the database calculates a deadline relative to the current monotonic time and records it in `expiry_map`.

Conceptually:

```bash
EXPIRE session 60
```

associates the key `session` with an expiration deadline approximately 60 seconds in the future.

The example illustrates the intended behavior; whether the command succeeds depends on whether the key exists when the database processes the operation.

### 6.2 Expiration Validation

The command handler validates the duration using integer parsing and checks that the entire token was consumed.

The database then:

1. Acquires the database mutex.
2. Purges the specified key if its existing expiration deadline has passed.
3. Checks whether the key exists in any supported data store.
4. Returns `false` if the key is no longer present.
5. Removes the key immediately if the requested duration is zero or negative.
6. Otherwise, records the new expiration deadline and returns `true`.

At the command-handler layer, the resulting boolean is formatted as a RESP integer response.

### 6.3 Zero and Negative Durations

The current implementation treats zero and negative durations as immediate deletion.

For example:

```bash
EXPIRE session 0
```

attempts to remove the key immediately if it exists.

The behavior is implemented by the database rather than by waiting for a background timer.

## 7. Expiration Cleanup

The database implements two important cleanup helpers:

- `purgeExpiredKeyLocked()` — checks and removes an expired key.
- `purgeAllExpiredLocked()` — checks the expiration map and removes all expired keys it finds.

These helpers remove expired data from the relevant stores and clear the associated expiration metadata. Their names ending in `Locked` indicate that callers are expected to hold the database mutex.

### 7.1 Single-Key Cleanup

When a method invokes `purgeExpiredKeyLocked()`, it checks the requested key's deadline.

If the deadline has passed, the helper removes the key from the string, list, and hash stores and erases its expiration entry.

This is a form of lazy expiration: cleanup occurs when an operation that invokes the helper accesses the key, rather than through a dedicated timer for every key.

### 7.2 Full Cleanup

The `purgeAllExpiredLocked()` helper iterates over expiration metadata and removes entries whose deadlines have passed.

In the current implementation, `keys()` invokes this helper before collecting keys.

The `dump()` method does not invoke it. As a result, a dump can contain entries that are already expired but have not yet been cleaned up.

## 8. Which Operations Perform Expiration Cleanup?

Expiration behavior differs between database methods.

| Operation | Expiration behavior |
|---|---|
| `get()` | Purges the requested key before retrieving a string. |
| `keys()` | Purges all expired keys before collecting keys. |
| `type()` | Purges the requested key before checking its type. |
| `expire()` | Purges the requested key before checking existence and applying a new deadline. |
| `set()` | Clears the key's existing expiration entry when writing the string value. |
| `del()` | Does not purge first; checks whether data exists, then removes the key from all stores and the expiration map. |
| `rename()` | Does not purge the source key before renaming. |
| List operations | Do not invoke expiration cleanup. |
| Hash operations | Do not invoke expiration cleanup. |
| `dump()` | Does not purge all expired entries before writing. |
| `load()` | Clears expiration metadata and restores data without restoring TTLs. |

This table describes the current implementation. It does not imply that all these behaviors match Redis's complete expiration semantics.

### 8.1 Consequences of Lazy Cleanup

A key can be past its expiration deadline but still physically present in a data store if no operation has cleaned it up.

For example, a list key with an expired TTL can still be accessible through a list operation that does not invoke expiration cleanup.

Likewise, `del()` can report that it removed an existing key even if its expiration deadline has already passed, because it checks for stored data before removing it.

The distinction is between a key's logical expiration deadline and the actual removal of its stored data.

## 9. Interaction Between Persistence and Expiration

Persistence and expiration use related data structures but have different lifecycles.

### 9.1 TTLs Are Not Persisted

The dump format stores strings, lists, and hashes, but not the expiration deadlines in `expiry_map`.

During loading, the database explicitly clears the existing expiration map and does not recreate deadlines from the file.

Therefore, after a restart, loaded keys do not retain their previous TTLs.

### 9.2 Expired Data May Be Persisted

Because `dump()` does not purge all expired keys before writing, an expired entry can be saved if it remains in memory.

If the application later loads that record, the data can reappear without its previous expiration deadline.

This is an important limitation of the current persistence and expiration design.

### 9.3 Replacing Data and Expiration Metadata

The current `set()` implementation writes to `kv_store` and removes the expiration entry for the same key.

However, it does not remove a same-named entry from the list or hash stores. Since the database uses separate stores for different types, the same key name can exist in more than one store.

Expiration cleanup and deletion use a helper that removes a key from all stores. Other operations have different semantics, as described in the database implementation.

## 10. Current Limitations

The current persistence and expiration implementation has these limitations:

1. TTLs are not saved or restored.
2. Expired entries can remain accessible through operations that do not invoke expiration cleanup.
3. Dumps may include expired entries that have not been purged.
4. File parsing and post-open read/write error handling are limited.
5. The text-based format does not fully support arbitrary values containing its delimiters.
6. Periodic persistence can lose changes made after the last successful dump.
7. Persistence holds the database mutex while writing, which can delay concurrent commands.
8. Loading does not provide transactional rollback if a malformed file is only partially processed.
9. The detached persistence thread does not have an explicit shutdown lifecycle.
10. The current persistence format is a project-specific format, not Redis's native RDB format.

These are areas for future development rather than guarantees that the implementation already provides.

## 11. Potential Improvements

Possible future improvements include:

- Persisting expiration metadata in a well-defined format.
- Validating all records and detecting stream errors during loading.
- Checking write errors and using safer file-replacement strategies.
- Defining clear semantics for keys that exist in multiple type stores.
- Applying expiration cleanup consistently across operations that access keys.
- Designing a coordinated shutdown mechanism for the persistence thread.
- Reducing database lock contention by creating a consistent snapshot before performing slow disk writes.
- Adding tests for expiration boundaries, restart behavior, malformed dump files, and interrupted persistence.
- Documenting which persistence guarantees the server can and cannot provide.

These improvements should be implemented and tested incrementally.

## 12. Summary

The server stores strings, lists, and hashes in memory and periodically persists them to `dump.my_rdb`. During startup, it attempts to restore records from that file.

Expiration deadlines are tracked separately using `std::chrono::steady_clock`. Expired data is removed when methods invoke the appropriate cleanup helpers, but not every database operation does so.

The most significant interaction between the two systems is that TTLs are not persisted and expired entries may be dumped before they are cleaned up. Understanding this behavior is essential when reasoning about restart consistency, expiration correctness, and persistence guarantees.
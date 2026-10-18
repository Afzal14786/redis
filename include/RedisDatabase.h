
#ifndef REDIS_DATABASE_H
#define REDIS_DATABASE_H

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @class RedisDatabase
 * @brief Manages the in-memory data storage for the Redis-like server.
 *
 * RedisDatabase is responsible for storing, retrieving, updating, and
 * deleting data. It provides operations for string values, lists, hashes,
 * key expiration, and database persistence.
 *
 * The database maintains separate containers for different data types:
 * - kv_store: Stores string key-value pairs.
 * - list_store: Stores lists of strings.
 * - hash_store: Stores hashes containing field-value pairs.
 * - expiry_map: Stores expiration timestamps for keys.
 *
 * The class follows the Singleton design pattern so that the server can
 * access a shared database instance through getInstance().
 *
 * A mutex is provided to protect shared database state from concurrent
 * access. Individual methods must acquire this mutex where required;
 * their implementations determine the exact locking behavior.
 */
class RedisDatabase {
public:
    /**
     * @brief Returns the shared instance of the database.
     *
     * Provides access to the single RedisDatabase instance used by the
     * application.
     *
     * @return A reference to the shared RedisDatabase instance.
     *
     * @note The constructor and destructor are private, and copying is
     *       disabled to enforce the Singleton design.
     */
    static RedisDatabase& getInstance();

    // Persistence operations

    /**
     * Writes the current string, list, and hash data to a dump file.
     *
     * The dump uses K, L, and H record types for strings, lists, and hashes,
     * respectively.
     *
     * @param filename Path to the destination dump file.
     * @return true if the file was opened successfully; false otherwise.
     *
     * @note Expiration deadlines are not persisted.
     * @note Expired entries may be written if they have not been purged.
     * @note A successful return does not guarantee that every write succeeded.
     */

    bool dump(const std::string& filename);

    
    /**
     * @brief Loads database contents from the custom text-based dump format.
     *
     * Opens the specified file and, if opening succeeds, clears the string,
     * list, and hash stores, along with all existing expiration metadata.
     * It then reads the file line by line and parses K, L, and H records.
     *
     * @param filename Source file path.
     * @return false if the file cannot be opened; true after the reading
     *         loop completes.
     *
     * @note The dump format does not store TTLs. Loaded keys therefore do
     *       not regain their original expiration deadlines.
     * @note The loader uses whitespace-delimited tokens and splits hash
     *       tokens at the first colon. Data containing spaces or ambiguous
     *       colons may not be restored correctly.
     * @note Malformed records and stream errors are not fully validated.
     */

    bool load(const std::string& filename);

    // General key and string operations

    /**
     * @brief Removes all data from the database.
     *
     * Clears the string, list, and hash stores, along with all expiration
     * metadata. The operation acquires db_mutex while clearing the data.
     *
     * @return Always returns true after clearing the database.
     */

    bool flushAll();

    /**
     * @brief Stores a string value under a key.
     *
     * Creates or replaces the entry in kv_store and removes any
     * expiration deadline associated with the key.
     *
     * @param key The key under which the value is stored.
     * @param value The string value to store.
     *
     * @note This method does not remove same-named entries from
     *       list_store or hash_store.
     */

    void set(const std::string& key, const std::string& value);

    /**
     * @brief Retrieves a string value associated with a key.
     *
     * The retrieved value is written to the output parameter.
     *
     * @param key The key whose value is requested.
     * @param value Output parameter that receives the retrieved value.
     *
     * @return true if the value is retrieved successfully; false if the
     *         requested string key cannot be retrieved.
     */
    bool get(const std::string& key, std::string& value);

    /**
     * @brief Retrieves the keys currently stored in the database.
     *
     * @return A vector containing the keys returned by the implementation.
     *
     * @note The order of keys is not guaranteed.
     */
    std::vector<std::string> keys();

    /**
     * @brief Determines the data type associated with a key.
     *
     * @param key The key whose type is requested.
     *
     * @return A string describing the key's data type, such as "string",
     *         "list", or "hash", or "none" if no applicable key exists.
     *
     * @note The exact behavior for expired keys and keys present in
     *       multiple internal stores depends on the implementation.
     */
    std::string type(const std::string& key);

    /**
     * @brief Determines the data type associated with a key.
     *
     * Removes the key from all stores if its expiration deadline has
     * passed, then checks the string, list, and hash stores in that order.
     *
     * @param key The key whose type is requested.
     * @return "string", "list", "hash", or "none".
     *
     * @note If the same name exists in multiple stores, the first
     *       matching store in the check order determines the returned type.
     */

    bool del(const std::string& key);

    /**
     * @brief Sets an expiration duration for a key.
     *
     * Checks and purges the key if an earlier expiration deadline
     * has already passed. If the key still exists, sets a new deadline.
     *
     * @param key The key whose lifetime is to be limited.
     * @param seconds The expiration duration in seconds.
     * @return true if the key exists after initial expiration cleanup
     *         and the expiration operation is applied; false if missing
     *         or already expired.
     *
     * @note Zero or negative durations delete the key immediately.
     * @note Expiration is shared across strings, lists, and hashes.
     */
    bool expire(const std::string& key, int seconds);

    /**
     * @brief Renames an existing key across the data stores.
     *
     * Moves matching entries from kv_store, list_store, and hash_store
     * to new_key, overwriting destination entries in the corresponding
     * stores. If old_key has expiration metadata, that deadline is
     * transferred to new_key.
     *
     * @param old_key The current key name.
     * @param new_key The requested new key name.
     * @return true if data existed under old_key and was moved, or if
     *         old_key and new_key are identical and the key exists;
     *         false otherwise.
     *
     * @note Expiration is not checked before the rename.
     * @note If old_key has no TTL, an existing TTL for new_key is not
     *       explicitly cleared.
     * @note If old_key and new_key are identical, the operation does
     *       not modify the stores.
     */

    bool rename(const std::string& old_key, const std::string& new_key);

    
    /*
     * List operations
     *
     * Each operation acquires db_mutex before accessing list_store.
     * These methods do not invoke expiration-cleanup helpers. An expired
     * list can therefore remain accessible until another operation
     * purges the key from the database.
     *
     * dump() serializes list data but does not persist expiration
     * deadlines. load() restores the list data without its original TTL.
     */

    /**
     * @brief Retrieves all elements in a list.
     *
     * Returns a copy of the list associated with the specified key.
     * The elements retain their order within the returned vector.
     *
     * @param key The key identifying the list.
     *
     * @return A vector containing the list elements, or an empty vector
     *         if the list cannot be found or contains no elements.
     *
     * @note Returning a vector by value means modifications to the
     *       returned vector do not directly modify the stored list.
     */
    std::vector<std::string> list_get(const std::string& key);

    /**
     * @brief Returns the number of elements in a list.
     *
     * @param key The key identifying the list.
     *
     * @return The list length, or 0 if the list is not found or is empty.
     */
    std::size_t list_len(const std::string& key);

    /**
     * @brief Inserts an element at the beginning of a list.
     *
     * @param key The key identifying the list.
     * @param value The element to insert.
     *
     * @return The resulting list length.
     *
     * @note Whether a missing list is created is determined by the
     *       implementation.
     */
    std::size_t list_l_push(const std::string& key, const std::string& value);

    /**
     * @brief Inserts an element at the end of a list.
     *
     * @param key The key identifying the list.
     * @param value The element to insert.
     *
     * @return The resulting list length.
     *
     * @note Whether a missing list is created is determined by the
     *       implementation.
     */
    std::size_t list_r_push(const std::string& key, const std::string& value);

    /**
     * @brief Removes and retrieves the first element of a list.
     *
     * @param key The key identifying the list.
     * @param value Output parameter that receives the removed element.
     *
     * @return true if an element is removed; false if the list is missing
     *         or empty.
     */
    bool list_l_pop(const std::string& key, std::string& value);

    /**
     * @brief Removes and retrieves the last element of a list.
     *
     * @param key The key identifying the list.
     * @param value Output parameter that receives the removed element.
     *
     * @return true if an element is removed; false if the list is missing
     *         or empty.
     */
    bool list_r_pop(const std::string& key, std::string& value);

    /**
     * @brief Removes matching elements from a list.
     *
     * @param key The key identifying the list.
     * @param count Controls the number and direction of removals:
     *              positive values search from the head, negative values
     *              search from the tail, and zero requests removal of
     *              all matching elements.
     * @param value The element value to match.
     *
     * @return The number of elements removed.
     */
    int list_lrem(const std::string& key, int count, const std::string& value);

    /**
     * @brief Retrieves an element at a specified list index.
     *
     * @param key The key identifying the list.
     * @param index The requested index. Negative indexes may refer to
     *              positions counted from the end of the list.
     * @param value Output parameter that receives the element.
     *
     * @return true if the element is found; false if the list is missing
     *         or the index is out of range.
     *
     * @note Exact negative-index handling is determined by the
     *       implementation.
     */
    bool list_lindex(const std::string& key, int index, std::string& value);

    /**
     * @brief Updates an element at a specified list index.
     *
     * @param key The key identifying the list.
     * @param index The index of the element to update. Negative-index
     *              behavior depends on the implementation.
     * @param value The new element value.
     *
     * @return true if the element is updated; false if the list is missing
     *         or the index is invalid.
     */
    bool list_lset(const std::string& key, int index, const std::string& value);

    /**
     * @brief Retrieves an inclusive range of list elements.
     *
     * @param key The key identifying the list.
     * @param start The starting index.
     * @param stop The ending index, inclusive.
     *
     * @return A vector containing the selected elements, or an empty
     *         vector if the list is missing or no elements match the range.
     *
     * @note Negative-index and out-of-range handling depend on the
     *       implementation.
     */
    std::vector<std::string> list_range(const std::string& key, int start, int stop);


    /*
     * Hash operations
     *
     * Each operation acquires db_mutex before accessing hash_store.
     * These methods do not invoke expiration-cleanup helpers. An expired
     * hash can therefore remain accessible until another operation
     * purges the key from the database.
     *
     * dump() serializes hash data but does not persist expiration
     * deadlines. load() restores the hash data without its original TTL.
     */

    /**
     * @brief Inserts or updates a field in a hash.
     *
     * @param key The key identifying the hash.
     * @param field The field to insert or update.
     * @param value The value associated with the field.
     *
     * @return true if a new field is created; false if the field already
     *         exists and is updated, according to the method's contract.
     *
     * @note Whether a missing hash is created is determined by the
     *       implementation.
     *
     * Redis command: HSET
     */
    bool hash_set(const std::string& key, const std::string& field, const std::string& value);

    /**
     * @brief Retrieves the value associated with a hash field.
     *
     * @param key The key identifying the hash.
     * @param field The field whose value is requested.
     * @param value Output parameter that receives the field value.
     *
     * @return true if the field exists and its value is retrieved;
     *         false if the hash or field cannot be found.
     *
     * Redis command: HGET
     */
    bool hash_get(const std::string& key, const std::string& field, std::string& value);

    /**
     * @brief Checks whether a field exists in a hash.
     *
     * @param key The key identifying the hash.
     * @param field The field to check.
     *
     * @return true if the field exists; otherwise false.
     *
     * Redis command: HEXISTS
     */
    bool hash_exists(const std::string& key, const std::string& field);

    /**
     * @brief Deletes a field from a hash.
     *
     * @param key The key identifying the hash.
     * @param field The field to delete.
     *
     * @return true if the field is deleted; otherwise false.
     *
     * @note Whether an empty hash is removed from hash_store is
     *       determined by the implementation.
     *
     * Redis command: HDEL
     */
    bool hash_del(const std::string& key, const std::string& field);

    /**
     * @brief Retrieves all field-value pairs from a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return A copy of the hash's field-value pairs, or an empty map
     *         if the hash is not found or contains no fields.
     *
     * @note The iteration order of the returned unordered_map is
     *       unspecified.
     *
     * Redis command: HGETALL
     */
    std::unordered_map<std::string, std::string> hash_getall(const std::string& key);

    /**
     * @brief Retrieves all field names from a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return A vector of field names, or an empty vector if the hash
     *         is not found or contains no fields.
     *
     * @note The order of returned field names is not guaranteed.
     *
     * Redis command: HKEYS
     */
    std::vector<std::string> hash_keys(const std::string& key);

    /**
     * @brief Retrieves all values from a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return A vector of field values, or an empty vector if the hash
     *         is not found or contains no fields.
     *
     * @note The order of returned values is not guaranteed.
     *
     * Redis command: HVALS
     */
    std::vector<std::string> hash_vals(const std::string& key);

    /**
     * @brief Counts the fields in a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return The number of fields, or 0 if the hash is not found
     *         or contains no fields.
     *
     * Redis command: HLEN
     */
    std::size_t hash_len(const std::string& key);

    /**
     * @brief Inserts or updates multiple field-value pairs in a hash.
     *
     * @param key The key identifying the hash.
     * @param fields The field-value pairs to process.
     * @return The number of fields newly created by this operation.
     *
     * @note Existing fields are updated but are not included in the
     *       returned count.
     * @note The entire batch is protected by one mutex lock.
     *
     * Redis command: HMSET
     */

    std::size_t hash_mset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields);

private:
    /**
     * @brief Constructs the database instance.
     *
     * Private access prevents callers from constructing independent
     * RedisDatabase objects.
     */
    RedisDatabase() = default;

    /**
     * @brief Destroys the database instance.
     *
     * Private access restricts direct destruction by callers.
     */
    ~RedisDatabase() = default;

    // Disable copying so the Singleton instance cannot be duplicated.
    RedisDatabase(const RedisDatabase&) = delete;
    RedisDatabase& operator=(const RedisDatabase&) = delete;


    /*
     * Private helper functions.
     */
    
     /*
     * Removes a key from every data store and its expiration metadata.
     *
     * A key may be represented in the string, list, or hash store.
     * Removing it from all three stores keeps the database consistent
     * even if the same key accidentally exists in multiple stores.
     *
     * IMPORTANT:
     * The caller must already hold db_mutex before calling this helper.
     * This function does not acquire the mutex itself.
     *
     * @param key The key whose data and expiration metadata must be removed.
     */
    void removeKeyFromAllStoresLocked(const std::string& key);

    /*
     * Checks whether one key has passed its expiration deadline.
     *
     * If the key has an expiration deadline and the current steady-clock
     * time has reached or passed that deadline, its data and expiration
     * metadata are removed. Keys without an expiration entry are left
     * unchanged.
     *
     * IMPORTANT:
     * The caller must already hold db_mutex before calling this helper.
     * This function does not acquire the mutex itself.
     *
     * @param key The key whose expiration status must be checked.
     */
    void purgeExpiredKeyLocked(const std::string& key);

    /*
     * Removes every expired key from the database.
     *
     * Scans expiry_map and removes expired keys from the string, list,
     * and hash stores. Expiration entries are erased safely while
     * iterating.
     *
     * Currently, keys() invokes this helper to remove expired entries
     * before collecting key names. dump() does not invoke this helper,
     * so expired entries may be written to a dump file.
     *
     * IMPORTANT:
     * The caller must already hold db_mutex before calling this helper.
     * This function does not acquire the mutex itself.
     */
    void purgeAllExpiredLocked();

    /**
     * @brief Synchronizes access to shared database state.
     *
     * Methods that access or modify shared containers must use this mutex
     * appropriately to avoid data races.
     */
    std::mutex db_mutex;

    /**
     * @brief Stores string key-value pairs.
     *
     * Each key maps to a single string value.
     */
    std::unordered_map<std::string, std::string> kv_store;

    /**
     * @brief Stores lists of string elements.
     *
     * Each key maps to a vector containing the list's elements in order.
     */
    std::unordered_map<std::string, std::vector<std::string>> list_store;

    /**
     * @brief Stores hashes containing field-value pairs.
     *
     * The outer key identifies the hash. The inner map stores each
     * field and its associated string value.
     */
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hash_store;

    /**
     * @brief Stores expiration timestamps for keys.
     *
     * Each entry associates a key with a steady-clock time point used
     * to represent its expiration deadline.
     *
     * @note The implementation determines when expiration is checked
     *       and how expired keys are removed or treated.
     */
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;

};

#endif
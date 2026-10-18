#ifndef REDIS_DATABASE_H
#define REDIS_DATABASE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

class RedisDatabase {
public:
    // Get the singleton instance
    static RedisDatabase& getInstance();
    // Persistance : Dump / load the database from a file
    bool dump(const std::string &filename);
    bool load(const std::string &filename);

    /**
     * @brief Removes all keys and their associated data from the database.
     * @return true if the operation succeeds; false otherwise.
     */
    bool flushAll();

    /**
     * @brief Stores a string value under the specified key.
     * @param key The key under which the value will be stored.
     * @param value The string value to store.
     * @return void.
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Retrieves the string value associated with a key.
     * @param key The key whose value is to be retrieved.
     * @param value Output parameter that receives the retrieved value.
     * @return true if the key exists and its value is retrieved; false otherwise.
     */
    bool get(const std::string& key, std::string& value);

    /**
     * @brief Returns all keys currently present in the database.
     * @return A vector containing the keys stored in the database.
     */
    std::vector<std::string> keys();

    /**
     * @brief Determines the data type associated with a key.
     * @param key The key whose data type is to be determined.
     * @return The type name (e.g., "string", "list", or "hash"),
     *         or "none" if the key does not exist.
     */
    std::string type(const std::string& key);

    /**
     * @brief Deletes a key and its associated data from the database.
     * @param key The key to delete.
     * @return true if the key was deleted; false if the key did not exist.
     */
    bool del(const std::string& key);

    /**
     * @brief Sets an expiration time for a key.
     * @param key The key whose expiration time is to be set.
     * @param seconds The time in seconds until the key expires.
     * @return true if the expiration was set successfully; false if the key
     *         does not exist or the operation fails.
     */
    bool expire(const std::string& key, int seconds);

    /**
     * @brief Renames an existing key.
     * @param old_key The current name of the key.
     * @param new_key The new name to assign to the key.
     * @return true if the key was renamed successfully; false if the old key
     *         does not exist or the operation fails.
     */
    bool rename(const std::string& old_key, const std::string& new_key);


    /**
     * @brief List operations
     */

    /**
     * @brief Returns the number of elements in a list.
     * @param key The key associated with the list.
     * @return The number of elements in the list, or 0 if the key does not exist.
     */
    size_t list_len(const std::string& key);

    /**
     * @brief Inserts an element at the beginning of a list.
     * @param key The key associated with the list.
     * @param value The element to insert.
     * @return The resulting length of the list.
     */
    size_t list_l_push(const std::string& key, const std::string& value);

    /**
     * @brief Inserts an element at the end of a list.
     * @param key The key associated with the list.
     * @param value The element to insert.
     * @return The resulting length of the list.
     */
    size_t list_r_push(const std::string& key, const std::string& value);

    /**
     * @brief Removes and retrieves the first element of a list.
     * @param key The key associated with the list.
     * @param value Output parameter that receives the removed element.
     * @return true if an element was removed; false if the key does not exist
     *         or the list is empty.
     */
    bool list_l_pop(const std::string& key, std::string& value);

    /**
     * @brief Removes and retrieves the last element of a list.
     * @param key The key associated with the list.
     * @param value Output parameter that receives the removed element.
     * @return true if an element was removed; false if the key does not exist
     *         or the list is empty.
     */
    bool list_r_pop(const std::string& key, std::string& value);

    /**
     * @brief Removes matching elements from a list.
     * @param key The key associated with the list.
     * @param count Determines how many matching elements to remove and the
     *              direction of the search:
     *              positive removes from the head, negative from the tail,
     *              and zero removes all matching elements.
     * @param value The element value to match and remove.
     * @return The number of elements removed.
     */
    int list_lrem(const std::string& key, int count, const std::string& value);

    /**
     * @brief Retrieves an element at a specified index in a list.
     * @param key The key associated with the list.
     * @param index The zero-based index of the element; negative indexes count
     *              from the end, where -1 represents the last element.
     * @param value Output parameter that receives the retrieved element.
     * @return true if the element was found; false if the key does not exist
     *         or the index is out of range.
     */
    bool list_lindex(const std::string& key, int index, std::string& value);

    /**
     * @brief Updates the element at a specified index in a list.
     * @param key The key associated with the list.
     * @param index The zero-based index of the element; negative indexes count
     *              from the end, where -1 represents the last element.
     * @param value The new value to assign to the element.
     * @return true if the element was updated; false if the key does not exist
     *         or the index is out of range.
     */
    bool list_lset(const std::string& key, int index, const std::string& value);

    /**
     * @brief Retrieves a range of elements from a list.
     * @param key The key associated with the list.
     * @param start The starting index, inclusive; negative indexes count
     *              from the end of the list.
     * @param stop The ending index, inclusive; negative indexes count
     *              from the end of the list.
     * @return A vector containing the selected elements, or an empty vector
     *         if the key does not exist or the range contains no elements.
     */
    std::vector<std::string> list_range(const std::string& key, int start, int stop);


    /**
     * @brief Insert or update a field in a hash.
     *
     * Creates the hash if the key does not exist.
     * If the field already exists, its value is updated.
     *
     * @param key   The key identifying the hash.
     * @param field The field to insert or update.
     * @param value The value associated with the field.
     *
     * @return true if the field was newly created.
     *         false if the field already existed and was updated.
     *
     * Redis command: HSET
     */
    bool hash_set(const std::string& key, const std::string& field, const std::string& value);


    /**
     * @brief Retrieve the value associated with a hash field.
     *
     * Searches for the hash key and then the requested field.
     *
     * @param key   The key identifying the hash.
     * @param field The field whose value is requested.
     * @param value Output parameter that receives the field value.
     *
     * @return true if the field exists and its value was retrieved;
     *         false if the key or field does not exist.
     *
     * Redis command: HGET
     */
    bool hash_get(const std::string& key, const std::string& field, std::string& value);


    /**
     * @brief Check whether a field exists inside a hash.
     *
     * A hash can exist while the requested field does not.
     * This function checks for the field itself, not just the key.
     *
     * @param key   The key identifying the hash.
     * @param field The field to check.
     *
     * @return true if the field exists; otherwise false.
     *
     * Redis command: HEXISTS
     */
    bool hash_exists(const std::string& key, const std::string& field);


    /**
     * @brief Delete a field from a hash.
     *
     * Removes the requested field if it exists.
     * If the hash becomes empty, remove its key from hash_store.
     *
     * @param key   The key identifying the hash.
     * @param field The field to delete.
     *
     * @return true if a field was removed; otherwise false.
     *
     * Redis command: HDEL (single-field version)
     */
    bool hash_del(const std::string& key, const std::string& field);


    /**
     * @brief Retrieve every field-value pair from a hash.
     *
     * Returns the pairs so the command handler can serialize them
     * as a RESP array.
     *
     * @param key The key identifying the hash.
     *
     * @return A collection of field-value pairs.
     *         Returns an empty collection if the key does not exist.
     *
     * Redis command: HGETALL
     */
    std::unordered_map<std::string, std::string> hash_getall(const std::string& key);


    /**
     * @brief Retrieve all field names from a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return A collection of field names, or an empty collection
     *         if the hash does not exist.
     *
     * Redis command: HKEYS
     */
    std::vector<std::string> hash_keys(const std::string& key);


    /**
     * @brief Retrieve all values stored in a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return A collection of field values, or an empty collection
     *         if the hash does not exist.
     *
     * Redis command: HVALS
     */
    std::vector<std::string> hash_vals(const std::string& key);


    /**
     * @brief Count the fields contained in a hash.
     *
     * @param key The key identifying the hash.
     *
     * @return The number of fields in the hash.
     *         Returns 0 if the key does not exist.
     *
     * Redis command: HLEN
     */
    size_t hash_len(const std::string& key);


    /**
     * @brief Insert or update multiple field-value pairs in a hash.
     *
     * Processes multiple pairs in one operation.
     * The input should contain complete field-value pairs.
     *
     * @param key    The key identifying the hash.
     * @param fields A collection of field-value pairs to insert or update.
     *
     * @return The number of newly created fields, if implemented
     *         with Redis-compatible HMSET/HSET semantics.
     *
     * Redis command: HMSET
     */
    size_t hash_mset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields);

private:
    RedisDatabase() = default;
    ~RedisDatabase() = default;

    RedisDatabase(const RedisDatabase&) = delete;
    RedisDatabase& operator=(const RedisDatabase&) = delete;
    std::mutex db_mutex;
    std::unordered_map<std::string, std::string> kv_store;
    std::unordered_map<std::string, std::vector<std::string>> list_store;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hash_store;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;

};

#endif
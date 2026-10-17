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
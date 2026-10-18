#include "../include/RedisDatabase.h"

#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <iterator>

RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}

/**
 * implementation of common commmands
 * 
 */

bool RedisDatabase::flushAll() {
    std::lock_guard<std::mutex> lock(db_mutex);
    kv_store.clear();
    list_store.clear();
    hash_store.clear();
    return true;
}

/**
 * implementation of key value functions
 */

void RedisDatabase::set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    kv_store[key] = value;
}

bool RedisDatabase::get(const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    auto expire = expiry_map.find(key);

    if (expire != expiry_map.end() && std::chrono::steady_clock::now() >= expire->second) {
        kv_store.erase(key);
        list_store.erase(key);
        hash_store.erase(key);
        expiry_map.erase(expire);

        return false;
    }

    auto it = kv_store.find(key);
    if (it != kv_store.end()) {
        value = it->second;
        return true;
    }
    return false;
}

std::vector<std::string> RedisDatabase::keys() {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::string> result;
    for (const auto &pairs : kv_store) {
        result.push_back(pairs.first);
    }

    for (const auto &pairs : list_store) {
        result.push_back(pairs.first);
    }

    for (const auto &pairs : hash_store) {
        result.push_back(pairs.first);
    }

    return result;
}

std::string RedisDatabase::type(const std::string &key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    if (kv_store.find(key) != kv_store.end())
        return "string";

    if (list_store.find(key) != list_store.end())
        return "list";

    if (hash_store.find(key) != hash_store.end())
        return "hash";
    else return "none";
}

bool RedisDatabase::del(const std::string &key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    bool erased = false;

    erased |= kv_store.erase(key) > 0;
    erased |= list_store.erase(key) > 0;
    erased |= hash_store.erase(key) > 0;
    return erased;
}

bool RedisDatabase::expire(const std::string &key, int seconds) {
    std::lock_guard<std::mutex> lock(db_mutex);
    bool is_exists = (kv_store.find(key) != kv_store.end()) || (list_store.find(key) != list_store.end()) || (hash_store.find(key) != hash_store.end());
    if (!is_exists) return false;

    expiry_map[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
}

bool RedisDatabase::rename(const std::string &old_key, const std::string &new_key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    bool found = false;

    /**
     * rename the key value store 
     */
    auto it_kv = kv_store.find(old_key);
    if (it_kv != kv_store.end()) {
        kv_store[new_key] = it_kv->second;
        kv_store.erase(it_kv);
        found = true;
    }

    /**
     * rename the list value store
     */

    auto it_list = list_store.find(old_key);
    if (it_list != list_store.end()) {
        list_store[new_key] = it_list->second;
        list_store.erase(it_list);
        found = true;
    }


    /**
     * rename the hash value store
     */

    auto it_hash = hash_store.find(old_key);
    if (it_hash != hash_store.end()) {
        hash_store[new_key] = it_hash->second;
        hash_store.erase(it_hash);
        found = true;
    }

    /**
     * expire map
     */

    auto it_expire = expiry_map.find(old_key);
    if (it_expire != expiry_map.end()) {
        expiry_map[new_key] = it_expire->second;
        expiry_map.erase(it_expire);
    }
    
    return found;
}


// list operations

// LIST_LEN

/**
 * @brief Returns the number of elements stored in a list.
 *
 * Searches for the given key in list_store. If the key exists,
 * returns the number of elements in its vector. Otherwise,
 * returns 0.
 *
 * The mutex protects list_store from concurrent modifications.
 *
 * @param key The key identifying the list.
 * @return Number of elements, or 0 if the key does not exist.
 *
 * @complexity
 * Time:  O(1) average, assuming hash-based lookup.
 * Space: O(1) auxiliary space.
 */
size_t RedisDatabase::list_len(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it != list_store.end()) {
        return it->second.size();
    }

    return 0;
}


// LIST_L_PUSH

/**
 * @brief Inserts an element at the beginning of a list.
 *
 * Accesses the list associated with key. If the key does not
 * exist, list_store[key] creates an empty list first.
 *
 * The new element is inserted at index 0. Because the underlying
 * container is std::vector, existing elements may need to shift
 * one position to the right.
 *
 * @param key   The key identifying the list.
 * @param value The element to insert.
 * @return The updated length of the list.
 *
 * @complexity
 * Time:  O(n), where n is the current list length.
 * Space: O(n) auxiliary storage in the worst case if the vector
 *        reallocates its internal storage.
 */
size_t RedisDatabase::list_l_push(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto& list = list_store[key];
    list.insert(list.begin(), value);

    return list.size();
}


// LIST_R_PUSH

/**
 * @brief Inserts an element at the end of a list.
 *
 * Uses push_back() to append the new value. If the key does not
 * exist, an empty list is created automatically.
 *
 * The updated list length is returned to the command handler.
 *
 * @param key   The key identifying the list.
 * @param value The element to append.
 * @return The updated length of the list.
 *
 * @complexity
 * Time:  O(1) amortized; O(n) in the worst case when reallocation
 *        moves the existing elements.
 * Space: O(1) auxiliary space amortized; O(n) additional storage
 *        may be needed during reallocation.
 */
size_t RedisDatabase::list_r_push(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto& list = list_store[key];

    list.push_back(value);

    return list.size();
}


// LIST_L_POP

/**
 * @brief Removes and returns the first element of a list.
 *
 * Finds the list and checks whether it contains any elements.
 * If it does, copies the first element into value and removes it.
 *
 * The key is erased when its list becomes empty, matching the
 * expected behavior of a REDIS-style list.
 *
 * @param key   The key identifying the list.
 * @param value Output parameter that receives the removed value.
 * @return true if an element was removed; false if the list
 *         does not exist or is empty.
 *
 * @complexity
 * Time:  O(n), because vector::erase(begin()) shifts remaining
 *        elements to the left.
 * Space: O(1) auxiliary space, excluding the returned string.
 */
bool RedisDatabase::list_l_pop(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end() || it->second.empty()) {
        return false;
    }

    auto& list = it->second;

    value = list.front();
    list.erase(list.begin());

    if (list.empty()) {
        list_store.erase(it);
    }

    return true;
}


// LIST_R_POP

/**
 * @brief Removes and returns the last element of a list.
 *
 * Finds the list and checks whether it contains any elements.
 * If it does, copies the last element into value and removes it
 * using pop_back().
 *
 * The key is erased when its list becomes empty.
 *
 * @param key   The key identifying the list.
 * @param value Output parameter that receives the removed value.
 * @return true if an element was removed; false if the list
 *         does not exist or is empty.
 *
 * @complexity
 * Time:  O(1), excluding the cost of copying the returned string.
 * Space: O(1) auxiliary space, excluding the output string.
 */
bool RedisDatabase::list_r_pop(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end() || it->second.empty()) {
        return false;
    }

    auto& list = it->second;

    value = list.back();
    list.pop_back();

    if (list.empty()) {
        list_store.erase(it);
    }

    return true;
}


// LIST_LREM

/**
 * @brief Removes matching elements from a list.
 *
 * The count argument determines which matching elements are removed:
 *
 * count > 0 : Remove up to count matches, scanning from head to tail.
 * count < 0 : Remove up to abs(count) matches, scanning from tail
 *             to head. Earlier matching elements are retained.
 * count == 0: Remove every matching element.
 *
 * This implementation compacts the vector in place rather than
 * repeatedly erasing elements. Repeated vector erasure can make
 * the worst-case runtime O(n^2).
 *
 * For a negative count, the function first counts occurrences,
 * determines how many must remain, and then compacts the vector
 * while retaining the earliest matching occurrences.
 *
 * The key is erased if all list elements are removed.
 *
 * @param key   The key identifying the list.
 * @param count Number and direction of removals.
 * @param value The value to remove.
 * @return Number of elements removed.
 *
 * @complexity
 * Time:  O(n), where n is the original list length.
 * Space: O(1) auxiliary space, excluding container internals.
 */
int RedisDatabase::list_lrem(const std::string& key, int count, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end()) {
        return 0;
    }

    auto& list = it->second;

    if (count == 0) {
        auto new_end = std::remove(list.begin(), list.end(), value);

        int removed = static_cast<int>(
            std::distance(new_end, list.end())
        );

        list.erase(new_end, list.end());

        if (list.empty()) {
            list_store.erase(it);
        }

        return removed;
    }

    if (count > 0) {
        int removed = 0;
        auto write = list.begin();

        for (auto read = list.begin(); read != list.end(); ++read) {
            if (*read == value && removed < count) {
                ++removed;
                continue;
            }

            if (write != read) {
                *write = std::move(*read);
            }

            ++write;
        }

        list.erase(write, list.end());

        if (list.empty()) {
            list_store.erase(it);
        }

        return removed;
    }

    // Convert to a wider signed type before negation. This avoids
    // overflow if count is INT_MIN.
    const long long requested = -static_cast<long long>(count);

    // Count all matching elements.
    size_t occurrences = 0;

    for (const auto& element : list) {
        if (element == value) {
            ++occurrences;
        }
    }

    // Remove the last requested occurrences by retaining only the
    // earlier matches. If requested exceeds occurrences, retain none.
    const size_t matches_to_keep =
        requested >= static_cast<long long>(occurrences)
            ? 0
            : occurrences - static_cast<size_t>(requested);

    size_t matches_kept = 0;
    int removed = 0;
    auto write = list.begin();

    for (auto read = list.begin(); read != list.end(); ++read) {
        if (*read == value) {
            if (matches_kept >= matches_to_keep) {
                ++removed;
                continue;
            }

            ++matches_kept;
        }

        if (write != read) {
            *write = std::move(*read);
        }

        ++write;
    }

    list.erase(write, list.end());

    if (list.empty()) {
        list_store.erase(it);
    }

    return removed;
}


// LIST_LINDEX

/**
 * @brief Retrieves the element at a specified list index.
 *
 * A non-negative index counts from the beginning:
 *   0 = first element, 1 = second element, and so on.
 *
 * A negative index counts backward from the end:
 *  -1 = last element, -2 = second-to-last element, and so on.
 *
 * The function returns false if the key does not exist or the
 * resolved index is outside the valid range.
 *
 * @param key   The key identifying the list.
 * @param index Requested element index.
 * @param value Output parameter that receives the element.
 * @return true if the element exists; otherwise false.
 *
 * @complexity
 * Time:  O(1) for index calculation and vector access, excluding
 *        the cost of copying the selected string.
 * Space: O(1) auxiliary space, excluding the output string.
 */
bool RedisDatabase::list_lindex(const std::string& key, int index, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end()) {
        return false;
    }

    const auto& list = it->second;
    const long long length = static_cast<long long>(list.size());

    long long resolved_index = index;

    if (resolved_index < 0) {
        resolved_index += length;
    }

    if (resolved_index < 0 || resolved_index >= length) {
        return false;
    }

    value = list[static_cast<size_t>(resolved_index)];

    return true;
}

// LIST_LSET

/**
 * @brief Replaces the element at a specified list index.
 *
 * Supports both positive and negative indexes.
 *
 * Unlike a push operation, LSET does not create a missing list
 * or append an element. The key must exist and the resolved
 * index must refer to an existing element.
 *
 * @param key   The key identifying the list.
 * @param index Index of the element to replace.
 * @param value New value to assign.
 * @return true if the element was replaced; otherwise false.
 *
 * @complexity
 * Time:  O(1) for index calculation and vector access, excluding
 *        the cost of assigning the string.
 * Space: O(1) auxiliary space, excluding string assignment needs.
 */
bool RedisDatabase::list_lset(const std::string& key, int index, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end()) {
        return false;
    }

    auto& list = it->second;
    const long long length = static_cast<long long>(list.size());

    long long resolved_index = index;

    if (resolved_index < 0) {
        resolved_index += length;
    }

    if (resolved_index < 0 || resolved_index >= length) {
        return false;
    }

    list[static_cast<size_t>(resolved_index)] = value;

    return true;
}



// LIST_RANGE

/**
 * @brief Returns a range of elements from a list.
 *
 * Both start and stop are inclusive.
 *
 * Negative indexes count backward from the end:
 *  -1 = last element, -2 = second-to-last element.
 *
 * Indexes are normalized as follows:
 * - A negative index is adjusted by adding the list length.
 * - A start index below zero after adjustment is clamped to zero.
 * - A stop index beyond the end is clamped to the last element.
 * - If the key is missing, the range is invalid, or start is
 *   beyond the list, an empty vector is returned.
 *
 * The selected elements are copied into a result vector. The
 * database mutex remains locked while the range is copied, so
 * other database operations cannot modify the list during this
 * operation.
 *
 * @param key   The key identifying the list.
 * @param start Starting index, inclusive.
 * @param stop  Ending index, inclusive.
 * @return A vector containing the requested elements, or an
 *         empty vector if no elements match.
 *
 * @complexity
 * Time:  O(k), where k is the number of returned elements,
 *        excluding the cost of copying their string contents.
 * Space: O(k) for the returned vector and copied strings.
 */
std::vector<std::string> RedisDatabase::list_range(const std::string& key, int start, int stop) {
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = list_store.find(key);

    if (it == list_store.end()) {
        return {};
    }

    const auto& list = it->second;

    if (list.empty()) {
        return {};
    }

    const long long length = static_cast<long long>(list.size());

    long long first = start;
    long long last = stop;

    // Convert negative indexes to positions relative to the end.
    if (first < 0) {
        first += length;
    }

    if (last < 0) {
        last += length;
    }

    // Clamp the start index to the beginning of the list.
    if (first < 0) {
        first = 0;
    }

    // Clamp the stop index to the last valid element.
    if (last >= length) {
        last = length - 1;
    }

    // No valid range exists if start is beyond the end or
    // the normalized start is greater than the normalized stop.
    if (first >= length || first > last || last < 0) {
        return {};
    }

    std::vector<std::string> result;

    result.reserve(static_cast<size_t>(last - first + 1));

    for (long long i = first; i <= last; ++i) {
        result.push_back(list[static_cast<size_t>(i)]);
    }

    return result;
}

// hash operations

/*
 * HSET — Set the value of a field inside a hash.
 *
 * Parameters:
 *   key   : Name of the hash.
 *   field : Field whose value should be created or updated.
 *   value : Value to store for the field.
 *
 * Returns:
 *   true  -> The field did not previously exist and was created.
 *   false -> The field already existed and its value was updated.
 *
 * Thread safety:
 *   db_mutex protects hash_store from concurrent access.
 */
bool RedisDatabase::hash_set(const std::string& key, const std::string& field, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Access the hash associated with this key.
    // If the key does not exist, this creates an empty hash.
    auto& hash = hash_store[key];

    // Check whether the field already exists.
    auto it = hash.find(field);

    if (it != hash.end()) {
        // The field exists, so update its value.
        it->second = value;

        // No new field was created.
        return false;
    }

    // The field is new, so insert it with the given value.
    hash.emplace(field, value);

    // A new field was created.
    return true;
}


/*
 * HGET — Retrieve the value associated with a hash field.
 *
 * Parameters:
 *   key   : Name of the hash.
 *   field : Field whose value should be retrieved.
 *   value : Output parameter. Receives the stored value if found.
 *
 * Returns:
 *   true  -> The hash and field exist; value contains the result.
 *   false -> The hash or field does not exist.
 *
 * Note:
 *   The caller must check the return value before using value.
 */
bool RedisDatabase::hash_get(const std::string& key, const std::string& field, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Search for the hash without creating a new entry.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        // The requested hash does not exist.
        return false;
    }

    // Search for the requested field inside the hash.
    auto field_it = it->second.find(field);

    if (field_it == it->second.end()) {
        // The hash exists, but the field does not.
        return false;
    }

    // Copy the stored field value into the output parameter.
    value = field_it->second;

    return true;
}


/*
 * HEXISTS — Check whether a field exists in a hash.
 *
 * Parameters:
 *   key   : Name of the hash.
 *   field : Field to check.
 *
 * Returns:
 *   true  -> The field exists.
 *   false -> The hash or field does not exist.
 *
 * This method only checks existence; it does not modify the data.
 */
bool RedisDatabase::hash_exists(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Find the hash first.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return false;
    }

    // Return whether the requested field exists in this hash.
    return it->second.find(field) != it->second.end();
}


/*
 * HDEL — Delete one field from a hash.
 *
 * Parameters:
 *   key   : Name of the hash.
 *   field : Field to delete.
 *
 * Returns:
 *   true  -> A field was deleted.
 *   false -> The hash or field did not exist.
 *
 * Redis-style behavior:
 *   When the last field is removed, the hash key itself should
 *   no longer exist. This implementation removes the empty hash.
 *
 * Note:
 *   This method deletes one field. The command handler can call
 *   it repeatedly to support HDEL key field1 field2 field3.
 */
bool RedisDatabase::hash_del(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Look up the hash.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return false;
    }

    // erase(field) returns the number of fields removed:
    // 1 if the field existed, or 0 if it did not.
    size_t deleted_count = it->second.erase(field);

    if (deleted_count == 0) {
        return false;
    }

    // A hash with no remaining fields should be removed.
    if (it->second.empty()) {
        hash_store.erase(it);
    }

    return true;
}


/*
 * HGETALL — Retrieve every field and value in a hash.
 *
 * Parameters:
 *   key : Name of the hash.
 *
 * Returns:
 *   A copy of the hash's field-value pairs.
 *   An empty map if the hash does not exist.
 *
 * Returning a copy means callers can iterate over the result
 * without holding db_mutex after this method returns.
 *
 * Note:
 *   Because hash_store uses an unordered_map, iteration order
 *   is unspecified.
 */
std::unordered_map<std::string, std::string> RedisDatabase::hash_getall(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Find the hash without creating one.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return {};
    }

    // Return a copy of all fields and their values.
    return it->second;
}


/*
 * HKEYS — Retrieve all field names from a hash.
 *
 * Parameters:
 *   key : Name of the hash.
 *
 * Returns:
 *   A vector containing all field names.
 *   An empty vector if the hash does not exist.
 *
 * Note:
 *   Field order is unspecified because the underlying container
 *   is an unordered_map.
 */
std::vector<std::string> RedisDatabase::hash_keys(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    std::vector<std::string> fields;

    // Find the hash.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return fields;
    }

    // Collect each field name.
    fields.reserve(it->second.size());

    for (const auto& pair : it->second) {
        fields.push_back(pair.first);
    }

    return fields;
}


/*
 * HVALS — Retrieve all values stored in a hash.
 *
 * Parameters:
 *   key : Name of the hash.
 *
 * Returns:
 *   A vector containing all field values.
 *   An empty vector if the hash does not exist.
 *
 * Important:
 *   Values are returned in the same iteration order as their
 *   fields in this unordered_map, but that order is unspecified.
 */
std::vector<std::string> RedisDatabase::hash_vals(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    std::vector<std::string> values;

    // Find the hash.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return values;
    }

    // Collect each field's value.
    values.reserve(it->second.size());

    for (const auto& pair : it->second) {
        values.push_back(pair.second);
    }

    return values;
}


/*
 * HLEN — Count the fields in a hash.
 *
 * Parameters:
 *   key : Name of the hash.
 *
 * Returns:
 *   The number of fields in the hash.
 *   Zero if the hash does not exist.
 */
size_t RedisDatabase::hash_len(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Find the hash.
    auto it = hash_store.find(key);

    if (it == hash_store.end()) {
        return 0;
    }

    // The size of the inner map is the number of hash fields.
    return it->second.size();
}

/*
 * HMSET — Set multiple fields and values in a hash.
 *
 * Parameters:
 *   key           : Name of the hash.
 *   fields_values : Vector of field-value pairs to store.
 *
 * Returns:
 *   The number of fields that were newly created.
 *
 * Behavior:
 *   - New fields are inserted.
 *   - Existing fields have their values overwritten.
 *   - Existing fields do not increase the returned count.
 *
 * Thread safety:
 *   The entire batch is protected by one mutex lock, so another
 *   thread cannot modify hash_store halfway through this batch.
 *
 * Note:
 *   We update the map directly instead of calling hash_set().
 *   hash_set() acquires db_mutex itself, and locking the same
 *   non-recursive mutex again in this method could deadlock.
 */
size_t RedisDatabase::hash_mset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields_values) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Access the hash, creating it if necessary.
    auto& hash = hash_store[key];

    size_t created_count = 0;

    // Process each field-value pair.
    for (const auto& pair : fields_values) {
        const std::string& field = pair.first;
        const std::string& value = pair.second;

        // Look for the field before deciding whether to insert
        // a new field or update an existing one.
        auto it = hash.find(field);

        if (it == hash.end()) {
            // This field is new.
            hash.emplace(field, value);
            ++created_count;
        } else {
            // This field already exists; overwrite its value.
            it->second = value;
        }
    }

    return created_count;
}

/**
 * Memory -> File --- dump()
 * File -> Memory --- load()
 * 
 * K = key value
 * L = List
 * H = Hash
 */

bool RedisDatabase::dump(const std::string &filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return false;
    for (const auto &kv : kv_store) {
        ofs << "K " << kv.first << " " << kv.second << "\n"; 
    }

    for (const auto &kv : list_store) {
        ofs << "L " << kv.first;
        for (const auto &item : kv.second) {
            ofs << " " << item;
        }
        ofs << "\n";
    }

    for (const auto &kv : hash_store) {
        ofs << "H " << kv.first;
        for (const auto &field_values : kv.second) {
            ofs << " " << field_values.first << ":" << field_values.second;
        }
        ofs << "\n";
    }

    return true;
}

/**
 * 
 * How the data structures stores the data
 * 
 * Key Value Pair (K)
 * 
 * kv_store["name"] = "Afzal"
 * kv_store["city"] = "Bombay"
 * 
 * List Store (L)
 * 
 * list_store["fruits"] = {"apple", "banana", "orange", "grapes"}
 * list_store["names"] = {"afzal", "meenal", "rabbani", "haider", "saloni", "firoz"}
 * 
 * Hash (H)
 * 
 * hash_store["user:100"] = {
 *      {"name", "afzal"},
 *      {"age" : "20"},
 *      {"city" : "Bombay"},
 *      {"email" : "mdafzal14777@gmail.com"}
 * };
 * 
 * hash_store["user:200"] = {
 *      {"name" : "Rohit"},
 *      {"age" : "25"},
 *      {"city" : "Delhi"},
 *      {"email" : "rohit@gmail.com"}
 * };
 * 
 */

bool RedisDatabase::load(const std::string &filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::ifstream ifs(filename, std::ios::binary);

    if (!ifs) return false;
    kv_store.clear();
    list_store.clear();
    hash_store.clear();

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        char type;
        if (!(iss >> type))
            continue;
        if (type == 'K') {
            std::string key, value;
            iss >> key >> value;
            kv_store[key] = value;
        } else if (type == 'L') {
            std::string key;
            iss >> key;
            std::string item;
            std::vector<std::string> list;
            while (iss >> item) {
                list.push_back(item);
            }
            list_store[key] = list;
        } else if (type == 'H') {
            std::string key;
            iss >> key;
            std::unordered_map<std::string, std::string> hash;
            std::string pair;
            while (iss >> pair) {
                auto pos = pair.find(':');
                if (pos != std::string::npos) {
                    std::string field = pair.substr(0, pos);
                    std::string value = pair.substr(pos+1);
                    hash[field] = value;
                }
            }
            hash_store[key] = hash;
        }
    }

    return true;
}
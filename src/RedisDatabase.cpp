#include "../include/RedisDatabase.h"

#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <iterator>

/**
 * @brief Returns the shared RedisDatabase instance.
 *
 * A function-local static object is created the first time this
 * function is called. Subsequent calls return a reference to the
 * same object.
 *
 * Since C++11, initialization of a function-local static variable
 * is thread-safe.
 *
 * @return Reference to the single RedisDatabase instance.
 */
RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}


/*
 * Removes a key from all supported data stores and its expiration map.
 *
 * We remove the key from all three stores rather than checking its type
 * first. This guarantees that no stale copy remains if the same key
 * accidentally exists in more than one store.
 *
 * Expiration metadata is also removed because it must never outlive the
 * key it belongs to. Otherwise, a future key with the same name could
 * accidentally inherit an old expiration deadline.
 *
 * The caller must hold db_mutex before calling this function.
 */
void RedisDatabase::removeKeyFromAllStoresLocked(const std::string& key) {
    kv_store.erase(key);
    list_store.erase(key);
    hash_store.erase(key);

    // Remove the TTL so deleted keys cannot leave stale expiration data.
    expiry_map.erase(key);
}

/*
 * Removes a single key if its expiration deadline has been reached.
 *
 * steady_clock is used because it measures elapsed time independently
 * of system-clock changes, such as manually changing the computer's
 * date or synchronizing the clock.
 *
 * A key is expired when:
 *
 *     current_time >= expiration_deadline
 *
 * If no expiration entry exists, the key has no TTL and this function
 * leaves it unchanged.
 *
 * The caller must hold db_mutex before calling this function.
 */
void RedisDatabase::purgeExpiredKeyLocked(const std::string& key) {
    // Find the key's expiration metadata without creating a new entry.
    const auto expiry_it = expiry_map.find(key);

    // No expiration entry means there is no TTL to check.
    if (expiry_it == expiry_map.end()) {
        return;
    }

    // Capture the current time and compare it with the stored deadline.
    const auto now = std::chrono::steady_clock::now();

    // A deadline equal to the current time is already expired.
    if (now >= expiry_it->second) {
        // Remove both the stored value and its expiration metadata.
        removeKeyFromAllStoresLocked(key);
    }
}

/*
 * Removes all keys whose expiration deadlines have passed.
 *
 * Instead of checking only the string store, this function iterates
 * through expiry_map. That is important because expiring keys can
 * contain strings, lists, or hashes.
 *
 * We erase expired entries using the iterator returned by erase().
 * This allows the loop to continue safely without incrementing an
 * iterator that has already been invalidated.
 *
 * The caller must hold db_mutex before calling this function.
 */
void RedisDatabase::purgeAllExpiredLocked() {
    // Capture the current time once so this cleanup pass uses one
    // consistent reference time for all expiration comparisons.
    const auto now = std::chrono::steady_clock::now();

    auto it = expiry_map.begin();

    while (it != expiry_map.end()) {
        // Keep keys whose expiration deadlines are still in the future.
        if (now < it->second) {
            ++it;
            continue;
        }

        // This key has expired. Remove it from every data store.
        kv_store.erase(it->first);
        list_store.erase(it->first);
        hash_store.erase(it->first);

        // Erase returns the next valid iterator, so the loop can
        // safely continue without skipping entries or using an
        // invalidated iterator.
        it = expiry_map.erase(it);
    }
}

/**
 * @brief Removes all stored strings, lists, and hashes.
 *
 * Acquires db_mutex to prevent other operations using the same mutex
 * from accessing the database containers during the clearing process.
 *
 * @return true after the three data stores have been cleared.
 *
 */


bool RedisDatabase::flushAll() {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Remove all stored values and their expiration metadata.
    kv_store.clear();
    list_store.clear();
    hash_store.clear();
    expiry_map.clear();

    return true;
}

/**
 * implementation of key value functions
 */

/**
 * @brief Stores a string value under the specified key.
 *
 * If the key already exists in kv_store, its string value is replaced.
 * Otherwise, a new string entry is created.
 *
 * Setting a value clears its previous expiration deadline. This follows
 * Redis-style SET behavior: a newly assigned value does not inherit the
 * old TTL.
 *
 * @param key Key under which the value is stored.
 * @param value String value to store.
 *
 * @note This method only updates kv_store and expiry_map. It does not
 *       remove a same-named entry from list_store or hash_store.
 */

void RedisDatabase::set(const std::string& key, const std::string& value) {
    // Protect the database containers from concurrent access.
    std::lock_guard<std::mutex> lock(db_mutex);

    // Insert the string key or replace its existing value.
    kv_store[key] = value;

    // A new SET operation removes the previous expiration deadline.
    // erase() is safe even when the key has no TTL.
    expiry_map.erase(key);
}

/**
 * @brief Retrieves a non-expired string value.
 *
 * First removes the key if its expiration deadline has passed.
 * Then searches kv_store for the requested string value.
 *
 * @param key Key whose value is requested.
 * @param value Output parameter populated when the string is found.
 * @return true if the non-expired string exists; otherwise false.
 */

bool RedisDatabase::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Remove the key from every store if its TTL has expired.
    // This helper assumes db_mutex is already locked.
    purgeExpiredKeyLocked(key);

    // Only return values that still exist after expiration cleanup.
    auto it = kv_store.find(key);

    if (it != kv_store.end()) {
        value = it->second;
        return true;
    }

    return false;
}

/**
 * @brief Returns keys currently present in the database stores.
 *
 * Removes expired keys before collecting names from all three stores.
 *
 * @return A vector containing the keys found in the stores.
 *
 * @note If the same name exists in multiple stores, it can still appear
 *       more than once. This change only fixes expiration handling.
 */

std::vector<std::string> RedisDatabase::keys() {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Purge expired strings, lists, and hashes before listing keys.
    purgeAllExpiredLocked();

    std::vector<std::string> result;

    for (const auto& pairs : kv_store) {
        result.push_back(pairs.first);
    }

    for (const auto& pairs : list_store) {
        result.push_back(pairs.first);
    }

    for (const auto& pairs : hash_store) {
        result.push_back(pairs.first);
    }

    return result;
}

/**
 * @brief Returns the type of a non-expired key.
 *
 * @param key Key whose type is requested.
 * @return "string", "list", "hash", or "none".
 */

std::string RedisDatabase::type(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // An expired key must behave as if it does not exist.
    purgeExpiredKeyLocked(key);

    if (kv_store.find(key) != kv_store.end()) {
        return "string";
    }

    if (list_store.find(key) != list_store.end()) {
        return "list";
    }

    if (hash_store.find(key) != hash_store.end()) {
        return "hash";
    }

    return "none";
}


/**
 * @brief Deletes a key and its expiration metadata.
 *
 * Removes the key from all supported stores, including any accidental
 * same-named entries in multiple stores.
 *
 * @param key Key to delete.
 * @return true if the key existed in at least one data store;
 *         false otherwise.
 */

bool RedisDatabase::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Determine whether actual data exists before removing anything.
    // A stale TTL entry alone does not mean that a key exists.
    const bool existed =
        kv_store.find(key) != kv_store.end() ||
        list_store.find(key) != list_store.end() ||
        hash_store.find(key) != hash_store.end();

    // Remove data and TTL metadata together.
    removeKeyFromAllStoresLocked(key);

    return existed;
}


/**
 * @brief Sets an expiration deadline for an existing key.
 *
 * @param key Key whose expiration should be set.
 * @param seconds Number of seconds until expiration.
 * @return true if the key existed when this operation was processed;
 *         false if the key was missing or already expired.
 *
 * @note A zero or negative duration deletes an existing key immediately.
 */

bool RedisDatabase::expire(const std::string& key, int seconds) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Remove the key first if an earlier TTL has already expired.
    purgeExpiredKeyLocked(key);

    // Check existence after cleanup, across all supported data types.
    const bool exists =
        kv_store.find(key) != kv_store.end() ||
        list_store.find(key) != list_store.end() ||
        hash_store.find(key) != hash_store.end();

    if (!exists) {
        return false;
    }

    // A zero or negative duration means immediate expiration.
    if (seconds <= 0) {
        removeKeyFromAllStoresLocked(key);
        return true;
    }

    // Record a new deadline measured using the monotonic clock.
    expiry_map[key] =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    return true;
}

/**
 * @brief Renames a key across the data stores.
 *
 * Moves any matching entry from kv_store, list_store, and hash_store
 * from old_key to new_key. If expiration metadata exists for old_key,
 * it is transferred to new_key as well.
 *
 * @param old_key Existing key name.
 * @param new_key Destination key name.
 * @return true if an entry was found in at least one data store;
 *         false otherwise.
 *
 * @note The current implementation can move entries from multiple
 *       stores if old_key exists in more than one. It also overwrites
 *       a destination entry in the corresponding store.
 */

bool RedisDatabase::rename(const std::string &old_key, const std::string &new_key) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Renaming a key to its own name must not erase it.
    // Check this before moving entries or modifying the maps.
    if (old_key == new_key) {
        return (
            kv_store.find(old_key) != kv_store.end() ||
            list_store.find(old_key) != list_store.end() ||
            hash_store.find(old_key) != hash_store.end()
        );
    }

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



/*
 * List operations
 *
 * Each operation locks db_mutex before accessing list_store.
 * These methods do not invoke the expiration-cleanup helpers, so an expired
 * list key can remain accessible until another operation purges that key.
 */


// LIST_LEN


/**
 * @brief Retrieves all elements of the list associated with a key.
 *
 * This function looks up the supplied key in list_store. If the key
 * exists, it returns a copy of the vector containing the list elements.
 * Otherwise, it returns an empty vector.
 *
 * The function acquires db_mutex before accessing list_store. This
 * protects the shared list data from unsynchronized concurrent access
 * by other database operations that use the same mutex.
 *
 * The lock_guard automatically releases the mutex when the function
 * returns, including when a return statement exits the function.
 *
 * @param key The key identifying the list to retrieve.
 *
 * @return A copy of the list elements in their stored order, or an
 *         empty vector if the key does not exist or the list is empty.
 */
std::vector<std::string> RedisDatabase::list_get(const std::string& key) {
    // Acquire the database mutex before accessing shared data.
    //
    // std::lock_guard automatically unlocks db_mutex when this
    // function exits, so we do not need to call unlock() manually.
    std::lock_guard<std::mutex> lock(db_mutex);

    // Search list_store for the requested key.
    //
    // find() returns an iterator to the matching entry if the key
    // exists; otherwise, it returns list_store.end().
    auto it = list_store.find(key);

    // If the key exists, return its stored list.
    //
    // it->second refers to the vector associated with the key.
    // Because the function returns by value, the caller receives
    // a copy rather than a reference to the internal vector.
    if (it != list_store.end()) {
        return it->second;
    }

    // The key does not exist in list_store.
    //
    // Return an empty vector to indicate that no list elements
    // were found. The stored database remains unchanged.
    return {};
}

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


/*
 * Hash operations
 *
 * Each operation locks db_mutex before accessing hash_store.
 * These methods do not invoke the expiration-cleanup helpers, so an expired
 * hash key can remain accessible until another operation purges that key.
 */


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

/**
 * @brief Serializes the in-memory database into a text-based dump file.
 *
 * Acquires db_mutex while writing the database contents to the
 * specified file. Each stored entry is written as a line with a
 * one-character record type:
 *
 * K - String key-value entry.
 * L - List key followed by its elements.
 * H - Hash key followed by field:value tokens.
 *
 * @param filename Destination file path.
 * @return false if the file cannot be opened; true after the write loops
 *         complete.
 *
 * @note The current format is whitespace-delimited and does not
 *       reliably preserve values containing spaces. Hash fields and
 *       values containing colons are also ambiguous.
 * @note Expiration timestamps are not persisted.
 * @note The current implementation does not check the stream state
 *       after writing, so a later write failure may go undetected.
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

/**
 * @brief Loads database contents from the custom text-based dump format.
 *
 * Opens the specified file and, if opening succeeds, clears the string,
 * list, and hash stores. It then reads the file line by line and parses
 * each record according to its K, L, or H type.
 *
 * @param filename Source file path.
 * @return false if the file cannot be opened; true after the reading
 *         loop completes.
 *
 * @note The current implementation does not clear expiry_map.
 * @note The loader uses whitespace-delimited tokens and splits hash
 *       tokens at the first colon. Data containing spaces or ambiguous
 *       colons may not be restored correctly.
 * @note Malformed records and stream errors are not fully validated.
 */

bool RedisDatabase::load(const std::string &filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::ifstream ifs(filename, std::ios::binary);

    if (!ifs) return false;
    kv_store.clear();
    list_store.clear();
    hash_store.clear();

    // The dump format currently does not persist TTLs, so old expiration
    // deadlines must not remain attached to keys loaded from the file.
    expiry_map.clear();

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
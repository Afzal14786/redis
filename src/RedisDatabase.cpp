#include "../include/RedisDatabase.h"

#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>

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
    return false;
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
        expiry_map[old_key] = it_expire->second;
        expiry_map.erase(it_expire);
    }
    
    return found;
}


// list operations

// hash operations

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
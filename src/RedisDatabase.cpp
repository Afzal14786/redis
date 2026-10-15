#include "../include/RedisDatabase.h"

#include <iostream>
#include <sstream>
#include <mutex>
#include <fstream>

RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}

// key value operations

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
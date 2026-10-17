#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"
#include <vector>
#include <string>
#include <exception>
#include <sstream>
#include <algorithm>

std::vector<std::string> parseRespCommands(const std::string &input) {
    std::vector<std::string> tokens;

    if (input.empty()) return tokens;
    
    // if it doesn't start with '*' then fallback to splitting by whitespaces
    if (input[0] != '*') {
        std::istringstream iss(input);
        std::string token;

        while (iss >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }

    size_t pos = 0;
    if (input[pos] != '*') return tokens;
    pos++;  // means skip the '*'


    // crlf = Carriage Return (\r), Line Feed (\n)
    size_t crlf = input.find("\r\n", pos);
    if (crlf == std::string::npos) return tokens;
    int numElement = std::stoi(input.substr(pos, crlf - pos));
    pos = crlf + 2;

    for (int i = 0; i < numElement; ++i) {
        if (pos >= input.size() || input[pos] != '$') break;
        pos++; // means skipping the '$'
        
        crlf = input.find("\r\n", pos);
        if (crlf == std::string::npos) break;
        int len = std::stoi(input.substr(pos, crlf - pos));
        pos = crlf + 2;

        if (pos + len > input.size()) break;
        std::string token = input.substr(pos, len);
        tokens.push_back(token);

        pos += len + 2;  // skip token and CRLF
    }

    return tokens;
}

/**
 * command commands handlers
 * 
 */

static std::string handle_ping(const std::vector<std::string>& /* tokens */, RedisDatabase& /* database */) {
    return "+PONG\r\n";
}

static std::string handle_echo(const std::vector<std::string>& tokens, RedisDatabase& /**database */) {
    if (tokens.size() < 2) 
        return "-ERR wrong number of arguments for ECHO\r\n";
    
    return "$" + std::to_string(tokens[1].size()) + "\r\n" + tokens[1] + "\r\n";
}

static std::string handle_flushall(const std::vector<std::string>& /** tokens */, RedisDatabase& db) {
    db.flushAll();
    return "+OK\r\n";
}

// --------------------------------------  --------------------------------------  --------------------------------------  --------------------------------------

/**
 * 
 * key value command handler
 */

static std::string handle_set(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 3) 
        return "-ERR wrong number of arguments for set\r\n";
    else {
        db.set(tokens[1], tokens[2]);
        return "+OK\r\n";
    }
}

static std::string handle_get(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2)
        return "-ERR wrong number of arguments for get\r\n";
    else {
        std::string val;
        if (db.get(tokens[1], val)) {
            return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
        } else {
            return "$-1\r\n";
        }
    }
}

static std::string handle_keys(const std::vector<std::string> &tokens, RedisDatabase &db) {
    std::vector<std::string> all_keys = db.keys();
    std::ostringstream response;
    response << "*" << all_keys.size() << "\r\n";
    for (const auto &key : all_keys) {
        response << "$" << key.size() << "\r\n" << key << "\r\n";
    }
    return response.str();
}

static std::string handle_type(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2)
        return "-ERR wrong number of arguments for TYPE\r\n";
    
    else 
        return "+" + db.type(tokens[1]) + "\r\n";
}

static std::string handle_del(const std::vector<std::string> &tokens, RedisDatabase &db, std::string &cmd) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for " + cmd + "\r\n";
    } else {
        bool res = db.del(tokens[1]);
        return ":" + std::to_string((res ? 1 : 0)) + "\r\n";
    }
}

static std::string handle_expire(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for EXPIRE\r\n";
    } else {
        try {
            size_t pos = 0;
            int seconds = std::stoi(tokens[2], &pos);
            if (pos != tokens[2].size() || seconds < 0) {
                return "-ERR invalid expire time\r\n";
            } else if (db.expire(tokens[1], seconds)) {
                return ":1\r\n";
            } else {
                return ":0\r\n";
            }

        } catch (const std::exception &) {
            return "-ERR invalid expire time\r\n";
        }
    }
}

static std::string handle_rename(const std::vector<std::string> &token, RedisDatabase &db) {
    if (token.size() < 3) {
        return "-ERR wrong number of arguments for RENAME\r\n";
    } else {
        if (db.rename(token[1], token[2])) {
            return "+OK\r\n";
        } else {
            return "-ERR no such key found\r\n";
        }
    }
}

// --------------------------------------  --------------------------------------  --------------------------------------  --------------------------------------

/**
 * 
 * @details list operation handlers
 */

static std::string handle_l_len(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of argument for LLEN\r\n";
    }
    size_t len = db.list_len(tokens[1]);
    return ":" + std::to_string(len) + "\r\n";
}

// push into the front
static std::string handle_l_push(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 3) {
        return "-ERR wrong number of arguments for LPUSH\r\n";
    } else {
        db.list_l_push(tokens[1], tokens[2]);
        size_t len = db.list_len(tokens[1]);
        return ":" + std::to_string(len) + "\r\n";
    }
}

// push into the back
static std::string handle_r_push(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 3) {
        return "-ERR wrong number of arguments for RPUSH\r\n";
    } else {
        db.list_r_push(tokens[1], tokens[2]);
        size_t len = db.list_len(tokens[1]);
        return ":" + std::to_string(len) + "\r\n";
    }
}

// popped from the front
static std::string handle_l_pop(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for LPOP\r\n";
    }

    std::string val;
    if (db.list_l_pop(tokens[1], val)) 
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    else 
        return "$-1\r\n";
}

// popped from the back
static std::string handle_r_pop(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) 
        return "-ERR wrong number of arguments for RPOP\r\n";
    
    std::string val;
    if (db.list_r_pop(tokens[1], val))
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    else 
        return "$-1\r\n";
}

static std::string handle_l_rem(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 4) {
        return "-ERR wrong number of arguments for LREM\r\n";
    }

    int count;
    try {
        size_t pos;
        count = std::stoi(tokens[2], &pos);
        if (pos != tokens[2].size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }
    } catch (const std::exception&) {
        return "-ERR value is not an integer or out of range\r\n";
    }

    int removed = db.list_lrem(tokens[1], count, tokens[3]);
    return ":" + std::to_string(removed) + "\r\n";
}

static std::string handle_l_index(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 4) {
        return "-ERR wrong number of arguments for LINDEX\r\n";
    } else {
        int index;
        try {
            size_t pos;
            index = std::stoi(tokens[2], &pos);
            if (pos != tokens[2].size())
                return "-ERR value is not an integer or out of range\r\n";
            
        } catch (const std::exception&) {
            return "-ERR value is not an integer or out of range\r\n";
        }

        std::string value;
        if (db.list_lindex(tokens[1], index, value)) {
            return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
        }

        return "$-1\r\n";
    }
}

static std::string handle_l_set(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 4) {
        return "-ERR wrong number of arguments of LSET\r\n";
    }
    int index;
    try {
        size_t pos;
        index = std::stoi(tokens[2], &pos);
        if (pos != tokens[2].size())
            return "-ERR value is not an interger or out of range\r\n";
    } catch (const std::exception &) {
        return "-ERR value is not an integer or out of range\r\n";
    }

    std::string value;
    if (db.list_lset(tokens[1], index, value)) {
        return "+OK\r\n";
    }
    return "no such key\r\n";
}

static std::string handle_l_range(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 4) {
        return "-ERR wrong number of arguments for LRANGE\r\n";
    }

    int start;
    int stop;

    try {
        size_t pos;
        start = std::stoi(tokens[2], &pos);
        if (pos != tokens[2].size())
            return "-ERR value is not an integer or out of range\r\n";
        
        stop = std::stoi(tokens[3], &pos);
        if (pos != tokens[3].size())
            return "-ERR value is not an integer or out of range\r\n";
    } catch (const std::exception &) {
        return "-ERR value is not an integer or out of range\r\n";
    }

    std::vector<std::string> elements = db.list_range(tokens[1], start, stop);
    std::ostringstream response;

    response << "*" << elements.size() << "\r\n";
    for (const auto &elem : elements) {
        response << "$" << elements.size() << "\r\n" << elem << "\r\n";
    }

    return response.str();
}

RedisCommandHandler::RedisCommandHandler(){}

std::string RedisCommandHandler::processCommand(const std::string &commandLine) {
    // USE RESP parser
    auto tokens = parseRespCommands(commandLine);
    if (tokens.empty()) return "-Error : empty command \r\n";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    // connect to database
    RedisDatabase &db = RedisDatabase::getInstance();

    // check commands

    // common commands

    /**
     * @command PING
     * @response PONG
     */
    if (cmd == "PING") {
       return handle_ping(tokens, db);
    } 
    
    /**
     * @command ECHO
     * 
     */
    else if(cmd == "ECHO") {
        return handle_echo(tokens, db);
    } 
    
    /**
     * @command
     * FLUSHALL
     */
    else if (cmd == "FLUSHALL") {
        return handle_flushall(tokens, db);
    }

    // key value operations

    /**
     * @command
     * SET
     */
    else if (cmd == "SET") {
        return handle_set(tokens, db);
    } 
    
    /**
     * @command
     * GET
     */
    else if (cmd == "GET") {
        return handle_get(tokens, db);
    } 
    /**
     * @command
     * KEYS
     */
    else if (cmd == "KEYS") {
        return handle_keys(tokens, db);
    } 
    
    /**
     * @command
     * TYPE
     */
    else if (cmd == "TYPE") {
        return handle_type(tokens, db);
    } 
    
    /**
     * @command
     * DEL | UNLINK
     */
    else if (cmd == "DEL" || cmd == "UNLINK") {
        return handle_del(tokens, db, cmd);
    } 
    
    /**
     * @command
     * EXPIRE
     */
    else if (cmd == "EXPIRE") {
        return handle_expire(tokens, db);
    } 
    
    /**
     * @command
     * RENAME
     */
    else if (cmd == "RENAME") {
        return handle_rename(tokens, db);
    }
    // list operations
    
    /**
     * @command
     * LLEN
     */
    else if (cmd == "LLEN") {
        return handle_l_len(tokens, db);
    } 
    
    /**
     * @command
     * LPUSH
     */
    else if (cmd == "LPUSH") {
        return handle_l_push(tokens, db);
    } 

    /**
     * @command
     * RPUSH
     */
    else if (cmd == "RPUSH") {
        return handle_r_push(tokens, db);
    }
    
    /**
     * @command
     * LPOP
     */
    else if(cmd == "LPOP") {
        return handle_l_pop(tokens, db);
    }

    /**
     * @command
     * RPOP
     */
    else if (cmd == "RPOP") {
        return handle_r_pop(tokens, db);
    }

    /**
     * @command
     * LREM
     */
    else if (cmd == "LREM") {
        return handle_l_rem(tokens, db);
    } 
    
    /**
     * @command
     * LINDEX
     */
    else if (cmd == "LINDEX") {
        return handle_l_index(tokens, db);
    }
    
    /**
     * @command
     * LSET
     */
    else if (cmd == "LSET") {
        return handle_l_set(tokens, db);
    }

    /**
     * @command
     * LRANGE
     */

    else if (cmd == "LRANGE") {
        return handle_l_range(tokens, db);
    }

    // hash operations

    else 
        return "-Error Unknown Command.\r\n";
}
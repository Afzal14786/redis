#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"
#include <vector>
#include <string>
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

static std::string handle_set(std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 3) 
        return "-ERR wrong number of arguments for set\r\n";
    else {
        db.set(tokens[1], tokens[2]);
        return "+OK\r\n";
    }
}

static std::string handle_get(std::vector<std::string> &tokens, RedisDatabase &db) {
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

static std::string handle_keys(std::vector<std::string> &tokens, RedisDatabase &db) {
    std::vector<std::string> all_keys = db.keys();
    std::ostringstream response;
    response << "*" << all_keys.size() << "\r\n";
    for (const auto &key : all_keys) {
        response << "$" << key.size() << "\r\n" << key << "\r\n";
    }
    return response.str();
}

static std::string handle_type(std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2)
        return "-ERR wrong number of arguments for TYPE\r\n";
    
    else 
        return "+" + db.type(tokens[1]) + "\r\n";
}

static std::string handle_del(std::vector<std::string> &tokens, RedisDatabase &db, std::string &cmd) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for " + cmd + "\r\n";
    } else {
        bool res = db.del(tokens[1]);
        return ":" + std::to_string((res ? 1 : 0)) + "\r\n";
    }
}

static std::string handle_expire(std::vector<std::string> &tokens, RedisDatabase &db) {
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

static std::string handle_rename(std::vector<std::string> &token, RedisDatabase &db) {
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

RedisCommandHandler::RedisCommandHandler(){}

std::string RedisCommandHandler::processCommand(const std::string &commandLine) {
    // USE RESP parser
    auto tokens = parseRespCommands(commandLine);
    if (tokens.empty()) return "-Error : empty command \r\n";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    std::ostringstream response;

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

    // hash operations

    else {
        response << "-Error Unknown Command.\r\n";
    }

    return response.str();
}
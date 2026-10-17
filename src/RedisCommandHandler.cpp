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
        response << "+PONG\r\n";
    } 
    
    /**
     * @command ECHO
     * 
     */
    else if(cmd == "ECHO") {
        if (tokens.size() < 2) 
            response << "-Error: ECHO requires a message\r\n";
        else 
            response << "+" << tokens[1] << "\r\n";
    } 
    
    /**
     * @command
     * FLUSHALL
     */
    else if (cmd == "FLUSHALL") {
        db.flushAll();
        response << "+OK" << "\r\n";
    }

    // key value operations

    /**
     * @command
     * SET
     */
    else if (cmd == "SET") {
        if (tokens.size() < 3) {
            response << "-Error: SET requires key and value\r\n";
        } else {
            db.set(tokens[1], tokens[2]);
            response << "+OK" << "\r\n";
        }
    } 
    
    /**
     * @command
     * GET
     */
    else if (cmd == "GET") {
        if (tokens.size() < 2) 
            response << "-Error: GET requires a key\r\n";
        else {
            std::string value;
            if (db.get(tokens[1], value)) {
                response << "$" << value.size() << "\r\n" << value << "\r\n";
            } else {
                response << "$-1\r\n";
            }
        }
    } 
    /**
     * @command
     * KEYS
     */
    else if (cmd == "KEYS") {
        std::vector<std::string> all_keys = db.keys();
        response << "*" << all_keys.size() << "\r\n";
        for (const auto &key : all_keys)
            response << "$" << key.size() << "\r\n" << key << "\r\n";
    } 
    
    /**
     * @command
     * TYPE
     */
    else if (cmd == "TYPE") {
        if (tokens.size() < 2) 
            response << "-Error: TYPE requires a key\r\n";
        else 
            response << "+" << db.type(tokens[1]) << "\r\n";
    } 
    
    /**
     * @command
     * DEL | UNLINK
     */
    else if (cmd == "DEL" || cmd == "UNLINK") {
        if (tokens.size() < 2) {
            response << "-Error: " << cmd << " requires a key\r\n";
        } else {
            bool res = db.del(tokens[1]);
            response << ":" << (res ? 1 : 0) << "\r\n";
        }
    } 
    
    /**
     * @command
     * EXPIRE
     */
    else if (cmd == "EXPIRE") {
        if (tokens.size() < 3) 
            response << "-Error: EXPIRE requires key and time in seconds\r\n";
        else {
            int seconds = std::stoi(tokens[2]);
            if (db.expire(tokens[1], seconds))  // expires is 300 seconds means within 5 minutes
                response << "+OK\r\n";
            else 
                response << "-Error: key not found\r\n";
        }
    } 
    
    /**
     * @command
     * RENAME
     */
    else if (cmd == "RENAME") {
        if (tokens.size() < 3) 
            response << "-Error: RENAME requires old_key and new_key\r\n";
        else {
            if (db.rename(tokens[1], tokens[2]))
                response << "+OK\r\n";
            else
                response << "-Error: no such key.\r\n";
        }
    }
    // list operations

    // hash operations

    else {
        response << "-Error Unknown Command.\r\n";
    }

    return response.str();
}
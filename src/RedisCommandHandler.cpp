
#include <include/RedisCommandHandler.h>
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

    // check commands

    return response.str();
}
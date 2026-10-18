
#ifndef REDIS_COMMAND_HANDLER_H
#define REDIS_COMMAND_HANDLER_H

#include <string>

/**
 * @class RedisCommandHandler
 * @brief Processes Redis-style commands and produces RESP responses.
 *
 * RedisCommandHandler acts as the command-processing layer of the
 * Redis-like server.
 *
 * Its primary responsibility is to accept a command represented as
 * a string, process it according to the supported command syntax,
 * and return a response formatted using the Redis Serialization
 * Protocol (RESP).
 *
 * The implementation of command parsing, validation, dispatching,
 * database interaction, and response formatting is defined in
 * RedisCommandHandler.cpp.
 *
 * Keeping the class declaration in this header allows other source
 * files to use RedisCommandHandler without needing to know the
 * internal implementation details.
 */
class RedisCommandHandler {
public:
    /**
     * @brief Constructs a RedisCommandHandler object.
     *
     * Initializes a command handler so that it can process incoming
     * commands.
     *
     * Any specific initialization performed by the constructor is
     * defined in RedisCommandHandler.cpp.
     */
    RedisCommandHandler();

    /**
     * @brief Processes a command and returns its RESP-formatted response.
     *
     * Accepts a command as a string, processes it, and returns the
     * corresponding response encoded using the Redis Serialization
     * Protocol (RESP).
     *
     * Depending on the command and its outcome, the response may
     * represent a successful result, a missing value, an error,
     * or another supported RESP response type.
     *
     * The exact parsing rules, supported commands, database
     * operations, and response types are determined by the
     * implementation in RedisCommandHandler.cpp.
     *
     * @param commandLine The command input represented as a string.
     *
     * @return A string containing the RESP-formatted response.
     *
     * @note This method's signature alone does not guarantee that
     *       every Redis command or RESP data type is supported.
     *       Supported behavior depends on the implementation.
     */
    std::string processCommand(const std::string& commandLine);
};

#endif // REDIS_COMMAND_HANDLER_H

#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"
#include <vector>
#include <string>
#include <exception>
#include <sstream>
#include <cctype>
#include <algorithm>


/**
 * @brief Parses a raw command string into individual command tokens.
 *
 * This function supports two input formats:
 *
 * 1. RESP array format:
 *      *<number-of-elements>\r\n
 *      $<element-length>\r\n<element-data>\r\n
 *
 *    Each array element is represented as a RESP bulk string.
 *
 * 2. Whitespace-separated format:
 *      SET name Afzal
 *
 *    If the input does not begin with '*', it is split using
 *    whitespace as the delimiter.
 *
 * Parsing process for RESP input:
 *    1. Read the array header and its element count.
 *    2. Read each bulk-string length.
 *    3. Extract exactly the declared number of bytes.
 *    4. Validate the CRLF terminator after each element.
 *    5. Return the extracted tokens.
 *
 * Error handling:
 *    Malformed RESP input returns an empty vector. Numeric parsing
 *    errors are caught so malformed input does not propagate a
 *    std::stoi exception to the caller.
 *
 * @param input The raw command string received from the client.
 *
 * @return A vector containing the parsed command and its arguments.
 *         An empty vector indicates empty input, an empty array,
 *         or malformed RESP input.
 *
 * @note This parser handles one RESP array per input string; it does
 *       not implement pipelined commands or every RESP data type.
 */

std::vector<std::string> parseRespCommands(const std::string& input) {
    std::vector<std::string> tokens;

    // Nothing can be parsed from an empty input string.
    if (input.empty()) {
        return tokens;
    }

    // Preserve support for simple, whitespace-separated commands.
    // RESP arrays begin with '*'; all other input uses this fallback.
    if (input[0] != '*') {
        std::istringstream iss(input);
        std::string token;

        while (iss >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }

    // Start immediately after the RESP array marker '*'.
    size_t pos = 1;

    // Locate the end of the array header.
    size_t crlf = input.find("\r\n", pos);
    if (crlf == std::string::npos) {
        return {};
    }

    int numElements;

    try {
        // Parse the number of elements declared by the array.
        size_t parsedChars = 0;
        numElements = std::stoi(
            input.substr(pos, crlf - pos),
            &parsedChars
        );

        // Reject invalid suffixes, such as "*2abc".
        if (parsedChars != crlf - pos || numElements < 0) {
            return {};
        }
    } catch (const std::exception&) {
        // Reject invalid or out-of-range array counts.
        return {};
    }

    // Move past the array header and its CRLF.
    pos = crlf + 2;

    // Parse each declared array element as a bulk string.
    for (int i = 0; i < numElements; ++i) {
        // Every element in this parser's supported RESP format
        // must begin with '$'.
        if (pos >= input.size() || input[pos] != '$') {
            return {};
        }

        // Skip the bulk-string marker.
        ++pos;

        // Find the end of the bulk-string length header.
        crlf = input.find("\r\n", pos);
        if (crlf == std::string::npos) {
            return {};
        }

        int length;

        try {
            // Parse the declared byte length of the element.
            size_t parsedChars = 0;
            length = std::stoi(
                input.substr(pos, crlf - pos),
                &parsedChars
            );

            // Reject malformed lengths and negative lengths.
            if (parsedChars != crlf - pos || length < 0) {
                return {};
            }
        } catch (const std::exception&) {
            // Reject invalid or out-of-range bulk-string lengths.
            return {};
        }

        // Move to the beginning of the element's actual data.
        pos = crlf + 2;

        // Ensure the payload and its two-byte CRLF terminator fit
        // inside the input. Subtraction avoids unsafe addition
        // when checking the declared length against input.size().
        if (pos > input.size() ||
            input.size() - pos < 2 ||
            static_cast<size_t>(length) > input.size() - pos - 2) {
            return {};
        }

        // Extract exactly 'length' bytes. Unlike whitespace
        // splitting, this preserves spaces inside a RESP value.
        tokens.push_back(input.substr(pos, length));

        // Advance past the payload.
        pos += static_cast<size_t>(length);

        // Each RESP bulk-string payload must end with CRLF.
        if (input[pos] != '\r' || input[pos + 1] != '\n') {
            return {};
        }

        // Skip the payload terminator before reading the next item.
        pos += 2;
    }

    // Reject trailing data rather than silently accepting a malformed
    // or unsupported multi-command input.
    if (pos != input.size()) {
        return {};
    }

    return tokens;
}

/**
 * command commands handlers
 * 
 */

/**
 * @brief Handles the PING command.
 *
 * PING is used to check whether the server is responding to commands.
 *
 * In this implementation, PING does not require any arguments and
 * always returns a PONG response.
 *
 * The database reference is unused because this command does not
 * read or modify stored data.
 *
 * RESP response:
 *     +PONG\r\n
 *
 * @param tokens Parsed command tokens. Unused by this handler.
 * @param database Reference to the database instance. Unused.
 *
 * @return A RESP simple-string response containing PONG.
 */

static std::string handle_ping(const std::vector<std::string>& /* tokens */, RedisDatabase& /* database */) {
    // RESP simple strings begin with '+' and end with CRLF.
    return "+PONG\r\n";
}

/**
 * @brief Handles the ECHO command.
 *
 * Syntax:
 *     ECHO message
 *
 * Returns the supplied message without modifying the database.
 *
 * Argument validation:
 *     Exactly two tokens are required:
 *       tokens[0] = command name
 *       tokens[1] = message
 *
 * RESP response:
 *     $<message-length>\r\n<message>\r\n
 *
 * A bulk string is used so the response includes the message's
 * byte length, allowing the client to identify the end of the value.
 *
 * @param tokens Parsed command and its arguments.
 * @param database Reference to the database instance. Unused.
 *
 * @return The message encoded as a RESP bulk string, or a RESP error
 *         if the number of arguments is incorrect.
 */


static std::string handle_echo(const std::vector<std::string>& tokens, RedisDatabase& /* database */) {
    // Require the command name and exactly one message argument.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for ECHO\r\n";
    }

    // Use a reference to avoid copying the message string.
    const std::string& message = tokens[1];

    // Serialize the message as a RESP bulk string.
    return "$" + std::to_string(message.size()) + "\r\n" + message + "\r\n";
}

/**
 * @brief Handles the FLUSHALL command.
 *
 * Removes all stored data managed by RedisDatabase.
 *
 * This handler delegates the actual clearing operation to
 * RedisDatabase::flushAll(), keeping storage management inside
 * the database layer rather than manipulating its internal
 * containers directly.
 *
 * RESP response:
 *     +OK\r\n
 *
 * @param tokens Parsed command tokens. Unused by this handler.
 * @param db Reference to the database instance to be cleared.
 *
 * @return A RESP simple-string success response.
 *
 * @warning This command clears the database's stored data.
 */


static std::string handle_flushall(const std::vector<std::string>& /* tokens */, RedisDatabase& db) {
    // Delegate the clearing operation to the database layer.
    db.flushAll();

    // Return a RESP simple-string acknowledgement.
    return "+OK\r\n";
}

// --------------------------------------  --------------------------------------  --------------------------------------  --------------------------------------

/**
 * 
 * key value command handler
 */


/**
 * @brief Handles the SET command.
 *
 * Syntax:
 *     SET key value
 *
 * Stores the supplied value under the specified key by delegating
 * the storage operation to RedisDatabase::set().
 *
 * Argument layout:
 *     tokens[0] = command name
 *     tokens[1] = key
 *     tokens[2] = value
 *
 * RESP response:
 *     +OK\r\n on success.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to store the value.
 *
 * @return A RESP simple-string success response, or a RESP error
 *         if the key or value argument is missing.
 *
 * @note This implementation checks for at least three tokens.
 *       Additional arguments are currently ignored.
 */

static std::string handle_set(const std::vector<std::string> &tokens, RedisDatabase &db) {
    // A valid SET command needs a command name, key, and value.
    if (tokens.size() < 3) 
        return "-ERR wrong number of arguments for set\r\n";
    else {

        // Delegate storage and any associated database rules to the
        // database layer rather than modifying its containers here.
        db.set(tokens[1], tokens[2]);

        // RESP simple-string response indicating success.
        return "+OK\r\n";
    }
}

/**
 * @brief Handles the GET command.
 *
 * Syntax:
 *     GET key
 *
 * Retrieves the string value associated with the specified key.
 * The lookup is delegated to RedisDatabase::get().
 *
 * Argument layout:
 *     tokens[0] = command name
 *     tokens[1] = key
 *
 * RESP response:
 *     $<length>\r\n<value>\r\n when the key exists.
 *     $-1\r\n when the key is missing or no string value is found.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to retrieve the value.
 *
 * @return The stored value as a RESP bulk string, a null bulk string
 *         if the lookup fails, or a RESP error for missing arguments.
 *
 * @note Additional arguments are currently ignored.
 */

static std::string handle_get(const std::vector<std::string> &tokens, RedisDatabase &db) {
    // A key is required after the command name.
    if (tokens.size() < 2)
        return "-ERR wrong number of arguments for get\r\n";
    else {
        std::string val;

        // get() writes the retrieved value into 'val' and reports
        // whether the lookup succeeded.
        if (db.get(tokens[1], val)) {

            // Encode the value as a RESP bulk string.
            return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
        } else {

            // RESP2 null bulk string represents a missing value.
            return "$-1\r\n";
        }
    }
}

/**
 * @brief Handles the KEYS command.
 *
 * Retrieves all keys returned by RedisDatabase::keys() and serializes
 * them into a RESP array of bulk strings.
 *
 * Response structure:
 *     *<number-of-keys>\r\n
 *     $<key-length>\r\n<key>\r\n
 *     ...
 *
 * @param tokens Parsed command tokens. Currently unused.
 * @param db Reference to the database used to retrieve the keys.
 *
 * @return A RESP array containing the retrieved keys.
 *
 * @note This implementation does not process a key pattern such as
 *       KEYS *. It also does not validate the number of arguments.
 */


static std::string handle_keys(const std::vector<std::string> &tokens, RedisDatabase &db) {

    // Retrieve the keys through the database interface.
    std::vector<std::string> all_keys = db.keys();

    // Use a stream to assemble the complete RESP response.
    std::ostringstream response;

    // A RESP array header declares the number of elements that follow.
    response << "*" << all_keys.size() << "\r\n";
    
    // Encode each key as a separate RESP bulk string.
    for (const auto &key : all_keys) {
        response << "$" << key.size() << "\r\n" << key << "\r\n";
    }
    return response.str();
}

/**
 * @brief Handles the TYPE command.
 *
 * Syntax:
 *     TYPE key
 *
 * Asks RedisDatabase::type() to identify the type associated with
 * the supplied key.
 *
 * Argument layout:
 *     tokens[0] = command name
 *     tokens[1] = key
 *
 * RESP response:
 *     +<type>\r\n
 *
 * The exact type names and the response for a missing key are
 * determined by RedisDatabase::type().
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to inspect the key.
 *
 * @return The database's type result encoded as a RESP simple string,
 *         or a RESP error when the key argument is missing.
 *
 * @note Additional arguments are currently ignored.
 */

static std::string handle_type(const std::vector<std::string> &tokens, RedisDatabase &db) {

    // The command must include the key to inspect.
    if (tokens.size() < 2)
        return "-ERR wrong number of arguments for TYPE\r\n";
    
    else 
        // Prefix the type result with '+' to form a RESP simple string.
        return "+" + db.type(tokens[1]) + "\r\n";
}

/**
 * @brief Handles the DEL and UNLINK commands.
 *
 * Syntax supported by this implementation:
 *     DEL key
 *     UNLINK key
 *
 * Delegates deletion to RedisDatabase::del().
 *
 * Argument layout:
 *     tokens[0] = command name
 *     tokens[1] = key
 *
 * RESP response:
 *     :1\r\n if the database reports that the key was deleted.
 *     :0\r\n if the database reports that the key was not deleted.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to delete the key.
 * @param cmd Normalized command name, used in the error response.
 *
 * @return A RESP integer reply, or a RESP error if the key is missing.
 *
 * @note Only the first key is passed to db.del(). Multiple-key
 *       deletion is not implemented by this handler.
 * @note DEL and UNLINK share the same deletion behavior here;
 *       Redis's asynchronous UNLINK semantics are not implemented.
 */

static std::string handle_del(const std::vector<std::string> &tokens, RedisDatabase &db, std::string &cmd) {

    // A key must follow the command name.
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for " + cmd + "\r\n";
    } else {

        // Delegate deletion to the database and capture its result.
        bool deleted = db.del(tokens[1]);

        // Convert the boolean result to the RESP integer 1 or 0.
        return ":" + std::to_string((deleted ? 1 : 0)) + "\r\n";
    }
}


/**
 * @brief Handles the EXPIRE command.
 *
 * Syntax:
 *     EXPIRE key seconds
 *
 * Assigns an expiration time to an existing key through RedisDatabase.
 *
 * @return RESP integer 1 if the expiration was applied, or 0 if the
 *         key does not exist. Invalid arguments produce a RESP error.
 */
static std::string handle_expire(
    const std::vector<std::string>& tokens,
    RedisDatabase& db
) {
    // EXPIRE requires exactly three tokens:
    // command name, key, and expiration duration.
    // Validate before accessing tokens[1] or tokens[2].
    if (tokens.size() != 3) {
        return "-ERR wrong number of arguments for EXPIRE\r\n";
    }

    int seconds;

    try {
        size_t pos = 0;

        // Convert the duration and track how many characters were parsed.
        seconds = std::stoi(tokens[2], &pos);

        // Reject partially valid inputs such as "10abc".
        if (pos != tokens[2].size()) {
            return "-ERR invalid expire time\r\n";
        }
    } catch (const std::exception&) {
        // Handles invalid numbers and values outside the int range.
        return "-ERR invalid expire time\r\n";
    }

    // Redis-style EXPIRE accepts zero and negative durations.
    // Your database implementation treats these as immediate expiration.
    if (db.expire(tokens[1], seconds)) {
        return ":1\r\n";
    }

    return ":0\r\n";
}


/**
 * @brief Handles the RENAME command.
 *
 * Syntax:
 *     RENAME old_key new_key
 *
 * Requests RedisDatabase::rename() to rename an existing key.
 *
 * Argument layout:
 *     tokens[0] = command name
 *     tokens[1] = existing key name
 *     tokens[2] = destination key name
 *
 * RESP response:
 *     +OK\r\n when the database reports success.
 *     -ERR no such key found\r\n when it reports failure.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to rename the key.
 *
 * @return A RESP simple-string success response or a RESP error.
 *
 * @note This implementation checks for at least three tokens.
 *       Additional arguments are currently ignored.
 * @note The exact rename behavior, including handling of an existing
 *       destination key, is determined by RedisDatabase::rename().
 */


static std::string handle_rename(const std::vector<std::string> &token, RedisDatabase &db) {

    // Both the source and destination keys are required.
    if (token.size() < 3) {
        return "-ERR wrong number of arguments for RENAME\r\n";
    } else {

        // Delegate the rename operation to the database layer.
        if (db.rename(token[1], token[2])) {
            return "+OK\r\n";
        } else {

            // The database reports that the rename could not be completed.
            return "-ERR no such key found\r\n";
        }
    }
}

// --------------------------------------  --------------------------------------  --------------------------------------  --------------------------------------

/**
 * 
 * @details list operation handlers
 */


/**
 * @brief Handles the LGET command and returns all elements of a list.
 *
 * Command syntax:
 *     LGET <key>
 *
 * Example:
 *     LGET fruits
 *
 * If the list contains:
 *     ["apple", "banana", "mango"]
 *
 * The handler retrieves the complete list using db.list_get() and
 * serializes the elements into a RESP array containing bulk strings.
 *
 * RESP response example:
 *     *3\r\n
 *     $5\r\napple\r\n
 *     $6\r\nbanana\r\n
 *     $5\r\nmango\r\n
 *
 * Response structure:
 *     *3       -> The array contains three elements.
 *     $5        -> The next element contains five bytes.
 *     apple     -> The actual element value.
 *
 * Error handling:
 *     If the command does not contain exactly two tokens
 *     (the command name and its key), an error response is returned.
 *
 * Missing key:
 *     If the key does not exist, list_get() returns an empty vector.
 *     The handler then returns an empty RESP array: *0\r\n.
 *
 * Thread safety:
 *     This handler does not directly access list_store. It delegates
 *     database access to list_get(), which protects the shared data
 *     with db_mutex.
 *
 * @param tokens The parsed command and its arguments.
 *                tokens[0] is "LGET".
 *                tokens[1] is the key identifying the list.
 *
 * @param db Reference to the RedisDatabase instance used to retrieve
 *           the list elements.
 *
 * @return A RESP-formatted array containing all list elements, or a
 *         RESP error if the number of arguments is incorrect.
 */
static std::string handle_l_get(const std::vector<std::string>& tokens, RedisDatabase& db) {
    // LGET requires exactly two tokens:
    // 1. The command name.
    // 2. The key whose list elements should be retrieved.
    //
    // Reject missing keys and unexpected additional arguments.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for LGET\r\n";
    }

    // Retrieve all elements associated with the specified key.
    //
    // list_get() returns a copy of the list, so this operation
    // does not modify the original list stored in the database.
    auto elem = db.list_get(tokens[1]);

    // Build the RESP response in a string stream.
    //
    // A RESP array starts with:
    //     *<number-of-elements>\r\n
    //
    // For example, a list containing three elements starts with *3\r\n.
    std::ostringstream oss;
    oss << "*" << elem.size() << "\r\n";

    // Serialize every list element as a RESP bulk string.
    //
    // A bulk string has this format:
    //     $<number-of-bytes>\r\n
    //     <element-data>\r\n
    //
    // e.size() provides the string's byte length.
    // Using the actual length allows values containing spaces
    // to be represented correctly in the RESP response.
    for (const auto& e : elem) {
        oss << "$" << e.size() << "\r\n"
            << e << "\r\n";
    }

    // Convert the completed response stream into a string
    // that the server can send to the client.
    return oss.str();
}


/**
 * @brief Returns the number of elements in a Redis list.
 *
 * Implements the LLEN command by retrieving the length of the
 * list associated with the specified key.
 *
 * If the key does not exist or the database reports an empty
 * list, the result depends on the behavior of RedisDatabase::list_len().
 *
 * @param tokens Parsed command tokens:
 *               tokens[0] = "LLEN"
 *               tokens[1] = key
 * @param db Reference to the RedisDatabase instance.
 *
 * @return A RESP integer containing the list length, or a RESP
 *         error if the key argument is missing.
 *
 * @note Additional arguments are currently ignored.
 */

static std::string handle_l_len(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of argument for LLEN\r\n";
    }

    // Retrieve the current length of the list.
    size_t len = db.list_len(tokens[1]);

    // Encode the length as a RESP integer.
    return ":" + std::to_string(len) + "\r\n";
}

/**
 * @brief Handles the LPUSH command.
 *
 * Syntax:
 *     LPUSH key value [value ...]
 *
 * Inserts each supplied value at the beginning of the list.
 * When multiple values are supplied, they are processed from
 * left to right, so the final list order is reversed relative
 * to the supplied values.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used for list operations.
 *
 * @return A RESP integer containing the final list length,
 *         or a RESP error if the key or values are missing.
 *
 * @note The final length is obtained through list_len() after
 *       all insertions have completed.
 */

static std::string handle_l_push(const std::vector<std::string>& tokens, RedisDatabase& db) {
    // Step 1: Validate the arguments.
    // Syntax: LPUSH key value [value ...]
    if (tokens.size() < 3) {
        return "-ERR wrong number of arguments for LPUSH\r\n";
    }

    // Step 2: Insert every supplied value at the beginning.
    //
    // Example:
    // LPUSH fruits apple banana mango
    //
    // Insert apple:  [apple]
    // Insert banana: [banana, apple]
    // Insert mango:  [mango, banana, apple]
    //
    // This matches Redis's LPUSH ordering when multiple values
    // are supplied in a single command.
    for (size_t i = 2; i < tokens.size(); ++i) {
        db.list_l_push(tokens[1], tokens[i]);
    }

    // Step 3: Return the final list length.
    size_t len = db.list_len(tokens[1]);

    return ":" + std::to_string(len) + "\r\n";
}


/**
 * @brief Handles the RPUSH command.
 *
 * Syntax:
 *     RPUSH key value [value ...]
 *
 * Appends each supplied value to the end of the list in the
 * order in which the values appear in the command.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used for list operations.
 *
 * @return A RESP integer containing the final list length,
 *         or a RESP error if the key or values are missing.
 *
 * @note The final length is obtained through list_len() after
 *       all insertions have completed.
 */

static std::string handle_r_push(const std::vector<std::string>& tokens, RedisDatabase& db) {
    // Step 1: Validate the arguments.
    // Syntax: RPUSH key value [value ...]
    // At least one value is required after the key.
    if (tokens.size() < 3) {
        return "-ERR wrong number of arguments for RPUSH\r\n";
    }

    // Step 2: Append every supplied value to the end of the list.
    //
    // tokens[0] = "RPUSH"
    // tokens[1] = key
    // tokens[2] onwards = values to append
    //
    // Example:
    // RPUSH fruits apple banana mango
    //
    // First append apple, then banana, then mango.
    for (size_t i = 2; i < tokens.size(); ++i) {
        db.list_r_push(tokens[1], tokens[i]);
    }

    // Step 3: Return the final length of the list.
    // Redis responds with an integer representing the list length.
    size_t len = db.list_len(tokens[1]);

    return ":" + std::to_string(len) + "\r\n";
}



/**
 * @brief Removes and returns the first element of a list.
 *
 * Implements the LPOP command by asking RedisDatabase to remove
 * the element at the front of the list associated with the key.
 *
 * If the list contains an element, that element is returned as
 * a RESP bulk string. If no element can be popped, the handler
 * returns a RESP null bulk string.
 *
 * @param tokens Parsed command tokens:
 *               tokens[0] = "LPOP"
 *               tokens[1] = key
 * @param db Reference to the RedisDatabase instance.
 *
 * @return A RESP bulk string containing the removed element,
 *         or "$-1\r\n" if no element is available.
 *
 * @note Only the key argument is used. Optional arguments, such
 *       as a count, are not currently implemented by this handler.
 */

static std::string handle_l_pop(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) {
        return "-ERR wrong number of arguments for LPOP\r\n";
    }

    std::string val;
    // Remove the first element and store it in val.
    if (db.list_l_pop(tokens[1], val)) 
        // Return the removed element as a RESP bulk string.
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    else 
        // Return a null bulk string when no element is available.
        return "$-1\r\n";
}


/**
 * @brief Removes and returns the last element of a list.
 *
 * Implements the RPOP command by asking RedisDatabase to remove
 * the element at the back of the list associated with the key.
 *
 * If an element is available, it is returned as a RESP bulk
 * string. Otherwise, the handler returns a RESP null bulk string.
 *
 * @param tokens Parsed command tokens:
 *               tokens[0] = "RPOP"
 *               tokens[1] = key
 * @param db Reference to the RedisDatabase instance.
 *
 * @return A RESP bulk string containing the removed element,
 *         or "$-1\r\n" if no element is available.
 *
 * @note Only the key argument is used. Optional arguments, such
 *       as a count, are not currently implemented by this handler.
 */

static std::string handle_r_pop(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 2) 
        return "-ERR wrong number of arguments for RPOP\r\n";
    
    std::string val;

    // Remove the last element and store it in val.
    if (db.list_r_pop(tokens[1], val))
        // Return the removed element as a RESP bulk string.
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    else 
        // Return a null bulk string when no element is available.
        return "$-1\r\n";
}


/**
 * @brief Removes matching elements from a list.
 *
 * Implements the LREM command using the supplied key, count,
 * and target value.
 *
 * The count determines how the database searches for matching
 * elements, according to the behavior implemented by
 * RedisDatabase::list_lrem().
 *
 * @param tokens Parsed command tokens:
 *               tokens[0] = "LREM"
 *               tokens[1] = key
 *               tokens[2] = count
 *               tokens[3] = value
 * @param db Reference to the RedisDatabase instance.
 *
 * @return A RESP integer containing the number of removed
 *         elements, or a RESP error if the arguments or count
 *         are invalid.
 *
 * @note Only tokens[3] is used as the target value; additional
 *       arguments are ignored.
 */

static std::string handle_l_rem(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() < 4) {
        return "-ERR wrong number of arguments for LREM\r\n";
    }

    int count;

    // Convert the count argument to an integer.
    try {
        size_t pos;
        count = std::stoi(tokens[2], &pos);

        // Reject values that contain non-numeric trailing characters.
        if (pos != tokens[2].size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }
    } catch (const std::exception&) {
        return "-ERR value is not an integer or out of range\r\n";
    }

    // Remove matching elements and retrieve the removal count.
    int removed = db.list_lrem(tokens[1], count, tokens[3]);

    // Return the number of removed elements as a RESP integer.
    return ":" + std::to_string(removed) + "\r\n";
}

/**
 * @brief Retrieves an element from a list by index.
 *
 * Implements the LINDEX command by passing the key and requested
 * index to RedisDatabase::list_lindex().
 *
 * If the index identifies an existing element, the handler
 * returns that element as a RESP bulk string. If the key or
 * index does not identify an available element, it returns a
 * RESP null bulk string.
 *
 * @param tokens Parsed command tokens:
 *               tokens[0] = "LINDEX"
 *               tokens[1] = key
 *               tokens[2] = index
 * @param db Reference to the RedisDatabase instance.
 *
 * @return A RESP bulk string containing the requested element,
 *         "$-1\r\n" if no element is available, or a RESP error
 *         if the arguments or index are invalid.
 *
 * @note This handler requires exactly three tokens.
 */

static std::string handle_l_index(const std::vector<std::string> &tokens, RedisDatabase &db) {
    if (tokens.size() != 3) {
        return "-ERR wrong number of arguments for LINDEX\r\n";
    } else {
        int index;

        // Convert the index argument to an integer.
        try {
            size_t pos;
            index = std::stoi(tokens[2], &pos);

            // Reject values that contain non-numeric trailing characters.
            if (pos != tokens[2].size())
                return "-ERR value is not an integer or out of range\r\n";
            
        } catch (const std::exception&) {
            return "-ERR value is not an integer or out of range\r\n";
        }

        std::string value;

        // Retrieve the element at the requested index.
        if (db.list_lindex(tokens[1], index, value)) {
            // Encode the retrieved value as a RESP bulk string.
            return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
        }

        // Return a null bulk string when the index is unavailable.
        return "$-1\r\n";
    }
}

/**
 * @brief Handles the LSET command.
 *
 * Syntax:
 *     LSET key index value
 *
 * Replaces the element at the specified list index with the
 * supplied value. The database layer determines whether the
 * key and index identify a valid list element.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used for the update.
 *
 * @return A RESP simple string containing OK on success, or
 *         a RESP error if the arguments, key, or index are invalid.
 *
 * @note Exactly four tokens are required.
 */

static std::string handle_l_set(const std::vector<std::string>& tokens,RedisDatabase& db    ) {
    // Step 1: Validate the number of arguments.
    // Syntax: LSET key index value
    //
    // tokens[0] = "LSET"
    // tokens[1] = key
    // tokens[2] = index
    // tokens[3] = new value
    if (tokens.size() != 4) {
        return "-ERR wrong number of arguments for LSET\r\n";
    }
    
    // Step 2: Convert the index from a string to an integer.
    int index;

    try {
        size_t pos;
        index = std::stoi(tokens[2], &pos);

        // Reject inputs such as "1abc".
        if (pos != tokens[2].size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }
    } catch (const std::exception&) {
        return "-ERR value is not an integer or out of range\r\n";
    }

    // Step 3: Update the list element.
    //
    // IMPORTANT:
    // tokens[3] contains the new value supplied by the client.
    // Do not pass an empty or uninitialized local variable here.
    if (db.list_lset(tokens[1], index, tokens[3])) {
        // RESP simple-string success response.
        return "+OK\r\n";
    }

    // Step 4: Return a valid RESP error if the key or index is invalid.
    return "-ERR index out of range or list does not exist\r\n";
}

/**
 * @brief Handles the LRANGE command.
 *
 * Syntax:
 *     LRANGE key start stop
 *
 * Retrieves the requested range of list elements by delegating
 * index interpretation and range selection to RedisDatabase.
 * The returned elements are serialized as a RESP array of
 * bulk strings.
 *
 * @param tokens Parsed command and its arguments.
 * @param db Reference to the database used to retrieve the range.
 *
 * @return A RESP array containing the selected elements, or a
 *         RESP error if the argument count or indexes are invalid.
 *
 * @note Exactly four tokens are required.
 * @note Negative indexes and range boundaries depend on the
 *       implementation of RedisDatabase::list_range().
 */

static std::string handle_l_range(const std::vector<std::string>& tokens, RedisDatabase& db) {
    // Step 1: Validate the number of arguments.
    // Syntax: LRANGE key start stop
    //
    // tokens[0] = "LRANGE"
    // tokens[1] = key
    // tokens[2] = start index
    // tokens[3] = stop index
    //
    // Exactly 4 tokens are required.
    if (tokens.size() != 4) {
        return "-ERR wrong number of arguments for LRANGE\r\n";
    }

    // Step 2: Declare variables to store the start and stop indexes.
    int start;
    int stop;

    // Step 3: Convert the start and stop arguments from strings to integers.
    //
    // Command arguments arrive as strings, so we need to convert
    // tokens[2] and tokens[3] into integers before calling list_range().
    //
    // std::stoi() converts a string into an integer.
    // The position variable tells us how many characters were consumed.
    // We use it to reject inputs such as "2abc", which std::stoi()
    // could otherwise partially parse as the integer 2.
    try {
        size_t pos;

        // Convert the start index to an integer.
        start = std::stoi(tokens[2], &pos);

        // Ensure the entire start argument was a valid integer.
        if (pos != tokens[2].size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }

        // Convert the stop index to an integer.
        stop = std::stoi(tokens[3], &pos);

        // Ensure the entire stop argument was a valid integer.
        if (pos != tokens[3].size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }
    } catch (const std::exception&) {
        // std::stoi() throws an exception if the argument is invalid
        // or if its value is outside the range supported by int.
        return "-ERR value is not an integer or out of range\r\n";
    }

    // Step 4: Retrieve the requested range from the database.
    //
    // list_range() receives the key, start index, and stop index.
    // The database method is responsible for finding the list and
    // handling indexes, including negative indexes and invalid ranges.
    //
    // Example:
    // LRANGE fruits 0 2
    // might return: {"apple", "banana", "mango"}
    std::vector<std::string> elements =
        db.list_range(tokens[1], start, stop);

    // Step 5: Create a stream to construct the RESP response.
    //
    // RESP arrays begin with:
    // *<number-of-elements>\r\n
    //
    // Each element is encoded as a bulk string:
    // $<string-length>\r\n
    // <string-value>\r\n
    std::ostringstream response;

    // Write the RESP array header.
    // elements.size() is the number of strings in the returned range.
    response << "*" << elements.size() << "\r\n";

    // Step 6: Encode every list element as a RESP bulk string.
    for (const auto& elem : elements) {
        // IMPORTANT:
        // elem.size() is the length of THIS element.
        // Do not use elements.size() here, because that gives
        // the number of elements in the vector, not the string length.
        response << "$" << elem.size() << "\r\n";

        // Write the actual string followed by the RESP line terminator.
        response << elem << "\r\n";
    }

    // Step 7: Return the complete RESP response to the caller.
    //
    // The command handler can then send this response to the client.
    return response.str();
}

// --------------------------------------  --------------------------------------  --------------------------------------  --------------------------------------

/**
 * @details  HASH COMMAND HANDLERS
 */

// A Redis hash stores multiple field-value pairs under one key.
//
// Example:
//   HSET user:1 name Afzal
//   HSET user:1 city Mumbai
//
// Logical representation:
//   user:1 -> {
//       "name": "Afzal",
//       "city": "Mumbai"
//   }
//
// Architecture:
//   1. The client sends a command.
//   2. The command dispatcher selects the appropriate handler.
//   3. The handler validates the arguments.
//   4. The handler calls RedisDatabase to perform the operation.
//   5. The handler serializes the result into a RESP reply.
//
// The database layer manages data; this layer manages command
// validation and client-facing protocol responses.
//
// RESP framing uses CRLF ("\r\n") to terminate protocol lines.


/**
 * @brief Handle the HSET command.
 *
 * Syntax:
 *     HSET key field value
 *
 * Purpose:
 *     Create a hash field or update its existing value.
 *
 * Validation:
 *     Four tokens are required:
 *       tokens[0] = HSET
 *       tokens[1] = key
 *       tokens[2] = field
 *       tokens[3] = value
 *
 * Database contract:
 *     hash_set() returns true when a new field is created
 *     and false when an existing field is updated.
 *
 * RESP response:
 *     :1\r\n -> A new field was created.
 *     :0\r\n -> An existing field was updated.
 *
 * @return RESP integer reply, or a RESP error for invalid arguments.
 */
std::string handle_hash_set(std::vector<std::string>& tokens, RedisDatabase& db) {
    // HSET requires exactly three arguments after the command:
    // key, field, and value. Reject missing or extra arguments.
    if (tokens.size() != 4) {
        return "-ERR wrong number of arguments for HSET\r\n";
    }

    // Delegate storage logic to the database layer.
    // This method must distinguish insertion from updating.
    bool is_new_field = db.hash_set(tokens[1], tokens[2], tokens[3]);

    // RESP integer replies begin with ':' and end with CRLF.
    // Convert the boolean result to the integer expected by HSET.
    return ":" + std::to_string(is_new_field ? 1 : 0) + "\r\n";
}


/**
 * @brief Handle the HGET command.
 *
 * Syntax:
 *     HGET key field
 *
 * Purpose:
 *     Retrieve the value associated with one field.
 *
 * RESP response:
 *     $<length>\r\n<value>\r\n -> Field exists.
 *     $-1\r\n                 -> Hash or field is missing.
 *
 * The length is the number of bytes in the value, not the number
 * of characters displayed on screen.
 */
std::string handle_hash_get(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command, key, field.
    // Exact validation prevents out-of-range access to tokens[2].
    if (tokens.size() != 3) {
        return "-ERR wrong number of arguments for HGET\r\n";
    }

    std::string value;

    // hash_get() writes the result into 'value' and returns true
    // only when the requested field exists.
    if (db.hash_get(tokens[1], tokens[2], value)) {
        // A RESP bulk string contains:
        //   1. '$' followed by the byte length
        //   2. CRLF
        //   3. The value itself
        //   4. CRLF
        //
        // Using size() preserves correct framing for values that
        // contain spaces or other ordinary characters.
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }

    // RESP2 null bulk string: the requested value does not exist.
    return "$-1\r\n";
}


/**
 * @brief Handle the HEXISTS command.
 *
 * Syntax:
 *     HEXISTS key field
 *
 * Purpose:
 *     Check whether a particular field exists in a hash.
 *
 * Important:
 *     This checks the field's existence, not whether its value
 *     is non-empty. An existing field with an empty value exists.
 *
 * RESP response:
 *     :1\r\n -> Field exists.
 *     :0\r\n -> Hash or field does not exist.
 */
std::string handle_hash_exists(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command, key, field.
    if (tokens.size() != 3) {
        return "-ERR wrong number of arguments for HEXISTS\r\n";
    }

    // The database layer performs the actual existence check.
    bool field_exists = db.hash_exists(tokens[1], tokens[2]);

    // Convert the boolean into a RESP integer.
    return ":" + std::to_string(field_exists ? 1 : 0) + "\r\n";
}


/**
 * @brief Handle the HDEL command.
 *
 * Syntax:
 *     HDEL key field [field ...]
 *
 * Purpose:
 *     Delete one or more fields from a hash.
 *
 * Validation:
 *     The command must include a key and at least one field.
 *
 * RESP response:
 *     An integer containing the number of fields actually deleted.
 *
 * Important:
 *     Missing fields are not counted. If a field is repeated in
 *     the command, it can only be counted once because the first
 *     successful deletion removes it.
 */
std::string handle_hash_del(std::vector<std::string>& tokens, RedisDatabase& db) {
    // tokens[0] is the command and tokens[1] is the hash key.
    // At least one field must follow the key.
    if (tokens.size() < 3) {
        return "-ERR wrong number of arguments for HDEL\r\n";
    }

    size_t deleted_count = 0;

    // Start at index 2 because that is the first field.
    // Keep calling the single-field database method for each
    // requested field, counting only successful deletions.
    for (size_t i = 2; i < tokens.size(); ++i) {
        if (db.hash_del(tokens[1], tokens[i])) {
            ++deleted_count;
        }
    }

    // RESP integer reply reports the actual number removed,
    // not the number of field arguments supplied by the client.
    return ":" + std::to_string(deleted_count) + "\r\n";
}


/**
 * @brief Handle the HGETALL command.
 *
 * Syntax:
 *     HGETALL key
 *
 * Purpose:
 *     Retrieve every field-value pair stored in a hash.
 *
 * RESP response:
 *     An array containing alternating field and value elements:
 *       [field1, value1, field2, value2, ...]
 *
 * Example logical result:
 *     name -> Afzal
 *     city -> Mumbai
 *
 *     RESP array has four elements:
 *       name, Afzal, city, Mumbai
 *
 * Ordering:
 *     The order is unspecified because the database uses an
 *     unordered_map. Clients should not rely on a stable order.
 */
std::string handle_hash_getall(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command and key.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for HGETALL\r\n";
    }

    // Obtain a copy of the hash from the database layer.
    // If the key is missing, this returns an empty map.
    auto hash = db.hash_getall(tokens[1]);

    std::ostringstream oss;

    // RESP arrays start with '*' followed by the element count.
    // Each field-value pair produces two bulk-string elements.
    oss << "*" << hash.size() * 2 << "\r\n";

    for (const auto& pair : hash) {
        // Serialize the field name as a RESP bulk string.
        oss << "$" << pair.first.size() << "\r\n";
        oss << pair.first << "\r\n";

        // Serialize the corresponding value as a bulk string.
        oss << "$" << pair.second.size() << "\r\n";
        oss << pair.second << "\r\n";
    }

    // ostringstream lets us assemble the complete response before
    // returning it to the server's client-connection code.
    return oss.str();
}


/**
 * @brief Handle the HKEYS command.
 *
 * Syntax:
 *     HKEYS key
 *
 * Purpose:
 *     Return all field names in the specified hash.
 *
 * RESP response:
 *     An array of bulk strings, one for each field.
 *
 * Missing hash:
 *     The database returns an empty vector, serialized as *0.
 *
 * Ordering:
 *     Field order is unspecified.
 */
std::string handle_hash_keys(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command and key.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for HKEYS\r\n";
    }

    // Fetch all field names from the database.
    auto keys = db.hash_keys(tokens[1]);

    std::ostringstream oss;

    // Declare a RESP array with one element per field.
    oss << "*" << keys.size() << "\r\n";

    for (const auto& key : keys) {
        // Each field name is encoded as a bulk string.
        oss << "$" << key.size() << "\r\n";
        oss << key << "\r\n";
    }

    return oss.str();
}


/**
 * @brief Handle the HVALS command.
 *
 * Syntax:
 *     HVALS key
 *
 * Purpose:
 *     Return every value stored in the specified hash.
 *
 * RESP response:
 *     An array of bulk strings, one for each field value.
 *
 * Note:
 *     Values may repeat if different fields contain the same
 *     value. The returned order is unspecified.
 */
std::string handle_hash_vals(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command and key.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for HVALS\r\n";
    }

    // Fetch all field values from the database.
    auto values = db.hash_vals(tokens[1]);

    std::ostringstream oss;

    // The array contains exactly one element per returned value.
    oss << "*" << values.size() << "\r\n";

    for (const auto& value : values) {
        // Encode each value as a RESP bulk string.
        oss << "$" << value.size() << "\r\n";
        oss << value << "\r\n";
    }

    return oss.str();
}


/**
 * @brief Handle the HLEN command.
 *
 * Syntax:
 *     HLEN key
 *
 * Purpose:
 *     Count the number of fields in a hash.
 *
 * RESP response:
 *     A RESP integer containing the number of fields.
 *
 * Missing hash:
 *     Returns zero, matching Redis behavior for a missing key.
 */
std::string handle_hash_len(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Required tokens: command and key.
    if (tokens.size() != 2) {
        return "-ERR wrong number of arguments for HLEN\r\n";
    }

    // The database returns the number of fields in this hash,
    // or zero when the hash does not exist.
    size_t length = db.hash_len(tokens[1]);

    // Serialize the count as a RESP integer.
    return ":" + std::to_string(length) + "\r\n";
}


/**
 * @brief Handle the HMSET command.
 *
 * Syntax:
 *     HMSET key field value [field value ...]
 *
 * Purpose:
 *     Create or update several fields in one hash operation.
 *
 * Validation:
 *     There must be a key and at least one complete field-value
 *     pair. After the command and key, the number of tokens must
 *     therefore be even.
 *
 * Example:
 *     HMSET user:1 name Afzal city Mumbai age 25
 *
 * Database behavior:
 *     New fields are inserted and existing fields are updated.
 *
 * RESP response:
 *     +OK\r\n on success.
 *
 * Note:
 *     HMSET is retained for compatibility; HSET can also accept
 *     multiple field-value pairs in modern Redis.
 */
std::string handle_hash_mset(std::vector<std::string>& tokens, RedisDatabase& db) {
    // Minimum valid command:
    // HMSET key field value
    //
    // Total tokens must be even:
    //   0: command
    //   1: key
    //   2: field
    //   3: value
    //   4: next field
    //   5: next value
    //
    // An odd token count means a field is missing its value.
    if (tokens.size() < 4 || tokens.size() % 2 != 0) {
        return "-ERR wrong number of arguments for HMSET\r\n";
    }

    std::vector<std::pair<std::string, std::string>> field_values;

    // Start at index 2, skipping the command and key.
    // Advance by two because each field is followed by its value.
    for (size_t i = 2; i < tokens.size(); i += 2) {
        field_values.emplace_back(tokens[i], tokens[i + 1]);
    }

    // Let the database layer insert or update the complete batch.
    // The number of newly created fields is not needed for HMSET's
    // client-facing response, so intentionally ignore the result.
    db.hash_mset(tokens[1], field_values);

    // HMSET returns a simple-string success reply, not an integer.
    return "+OK\r\n";
}


/**
 * @brief Constructs a RedisCommandHandler instance.
 *
 * Initializes a command handler responsible for processing
 * incoming Redis-style commands.
 *
 * The constructor currently performs no explicit initialization.
 * Command parsing, command dispatching, and database operations
 * are performed by other methods.
 */

RedisCommandHandler::RedisCommandHandler(){}

/**
 * @brief Processes an incoming command and generates a RESP response.
 *
 * This method is the main entry point for command processing.
 * It parses the incoming command, validates that the parsed
 * command is not empty, normalizes the command name, obtains
 * the RedisDatabase singleton, and dispatches the request to
 * the appropriate command handler.
 *
 * The processing flow is:
 *
 * 1. Parse the input into command tokens.
 * 2. Return an error if the command is empty.
 * 3. Convert the command name to uppercase for case-insensitive
 *    command matching.
 * 4. Obtain the RedisDatabase singleton instance.
 * 5. Dispatch the command to its corresponding handler.
 * 6. Return the handler's RESP-formatted response.
 *
 * Supported commands include basic key-value operations, list
 * operations, and hash operations. DEL and UNLINK are routed
 * through the same handler in the current implementation.
 *
 * If the command name does not match a supported command,
 * the method returns an unknown-command error.
 *
 * @param commandLine The incoming command represented as a string.
 *
 * @return A RESP-formatted response representing the command's
 *         result or an error.
 *
 * @note The commands and argument combinations supported by this
 *       method depend on the implemented handlers.
 *
 * @see parseRespCommands()
 * @see RedisDatabase::getInstance()
 */

std::string RedisCommandHandler::processCommand(const std::string &commandLine) {
    // USE RESP parser
    auto tokens = parseRespCommands(commandLine);
    if (tokens.empty()) return "-Error : empty command \r\n";

    std::string cmd = tokens[0];
    // Normalize the command name to uppercase so command matching
    // is case-insensitive (for example, "get" becomes "GET").
    //
    // Convert through unsigned char before calling std::toupper.
    // This avoids undefined behavior for negative signed-char values.

    std::transform(
        cmd.begin(),
        cmd.end(),
        cmd.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        }
    );

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
     * LGET
     */

    else if (cmd == "LGET") {
        return handle_l_get(tokens, db);
    }

    /**
     * @command
     * LRANGE
     */

    else if (cmd == "LRANGE") {
        return handle_l_range(tokens, db);
    }

    // hash operations

    /**
     * @command
     * HGET
     */

    else if (cmd == "HGET") {
        return handle_hash_get(tokens, db);
    }

    /**
     * @command
     * HSET
     */

    else if (cmd == "HSET") {
        return handle_hash_set(tokens, db);
    }

    /**
     * @command
     * HEXISTS
     */

    else if (cmd == "HEXISTS") {
        return handle_hash_exists(tokens, db);
    }

    /**
     * @command
     * HDEL
     */

    else if (cmd == "HDEL") {
        return handle_hash_del(tokens, db);
    }
    
    /**
     * @command
     * HGETALL
     */

    else if (cmd == "HGETALL") {
        return handle_hash_getall(tokens, db);
    }
    
    /**
     * @command
     * HKEYS
     */

    else if (cmd == "HKEYS") {
        return handle_hash_keys(tokens, db);
    }
    
    /**
     * @command
     * HVALS
     */

    else if (cmd == "HVALS") {
        return handle_hash_vals(tokens, db);
    }

    /**
     * @command
     * HLEN
     */

    else if (cmd == "HLEN") {
        return handle_hash_len(tokens, db);
    }
    
    /**
     * @command
     * HMSET
     */
    
    else if (cmd == "HMSET") {
        return handle_hash_mset(tokens, db);
    }

    else 
        return "-Error Unknown Command.\r\n";
}
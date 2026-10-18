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
    if (tokens.size() != 3) {
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
# This file contains all the concepts which I learn while building this project  

### What is `std::stod()W` and what it is use for

```cpp
std::stoi(argv[1]);  
```  

**Explanation :**  

The `std::stoi()` stands for **string of integres** and it is a function that converts a string contains numeric characters into an integer value and it is the member of `C++ Standard Template Library` and declared inside **<string>** header.  

- This provides a simple and efficient way of converting a string into numeric value.
- Supports different number bases such as `Hexadecimal`, `Decimal` or `Binary`.  

**How this function works**  

The `std::stoi()` function reads the string from **left** to **right** and convert the valid numeric portion into an integre.  
The conversion continues until an invalid character is encountered or the entire numeric value has been processed.  

- This ignore any leading whitespace characters before starting the conversion
- Converts consecutive numeric characters and stops at the first invalid character.
- It supports number base from `2 to 36` with `10` as the **default base**.  

---  

### `close` method  

**Explanation :**  

The `close()` and `open()` method is define inside **<unistd.h>** header file as a standard POSIX system call.  
It takes the **socket file descriptor** (`int`) as an argument.  

---  

### `#include <sys/socket.h>`  

This header file is used when we are dealing with sockets.  


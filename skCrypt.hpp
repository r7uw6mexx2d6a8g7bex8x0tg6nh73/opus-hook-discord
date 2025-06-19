#pragma once

#include <string>
#include <cstring>

#define skCrypt(str) ([]() { \
    static const char _crypt_key = 0xAA; /* Random key */ \
    static char _crypted[sizeof(str)] = {0}; \
    for (size_t i = 0; i < sizeof(str) - 1; i++) { \
        _crypted[i] = str[i] ^ _crypt_key; \
    } \
    return _crypted; \
})()

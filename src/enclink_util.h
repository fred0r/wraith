#ifndef _ENCLINK_UTIL_H
#define _ENCLINK_UTIL_H

#include <cstdarg>
#include <cstddef>

#include <bdlib/src/String.h>

static constexpr size_t LINK_KEY_LEN = 32;

bool link_random_key(char out[LINK_KEY_LEN + 1]);
bool link_random_seed(int& seed);
bool parse_link_seed(const char *text, int& seed);
bd::String format_link_message_va(const char *format, va_list va);
bd::String format_link_message(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

#endif

#ifndef _GARBLE_H
#define _GARBLE_H

#include <string>

#ifdef DEBUG
#define STR(x) x
#endif

class Garble {
public:
	static std::string degarble(int len, const char *encoded);
};

/* C API for stringfix-generated code and legacy callers */
const char *degarble(int, const char *);

#endif /* !_GARBLE_H */

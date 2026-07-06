#ifndef _MATCH_H
#define _MATCH_H

#include <string>

namespace wraith {

class WildcardMatcher {
public:
	/* Forward-direction (bind-style): *, %, ?, ~ */
	static int match_per(const std::string& pattern, const std::string& text);

	/* Reverse-direction (hostmask-style): *, ? */
	static int match(const std::string& pattern, const std::string& text);

	/* CIDR matching */
	static bool match_cidr(const std::string& pattern, const std::string& address);
};

} /* namespace wraith */

/* C API macros for existing callers */
#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))
#define wild_match_per(a,b) _wild_match_per((unsigned char *)(a),(unsigned char *)(b))

int _wild_match(const unsigned char *, const unsigned char *) __attribute__((pure));
int _wild_match_per(const unsigned char *, const unsigned char *) __attribute__((pure));

int match_cidr(const char *, const char *) __attribute__((pure));

#endif /* !_MATCH_H */

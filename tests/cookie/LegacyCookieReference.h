#ifndef TESTS_COOKIE_LEGACYCOOKIEREFERENCE_H
#define TESTS_COOKIE_LEGACYCOOKIEREFERENCE_H

#include <ctime>

#include <bdlib/src/String.h>

namespace legacy_cookie {

struct Member {
  bd::String nick;
  bd::String userhost;
  bd::String handle;
};

struct CookieContext {
  bd::String channel;
  Member opper;
  bd::String salt1;
  bd::String salt2;
  time_t now;
  bool opper_is_self;
};

struct Crypto {
  bd::String (*md5)(const bd::String&);
  bd::String (*encrypt)(const bd::String&, const bd::String&);
  bd::String (*decrypt)(const bd::String&, const bd::String&);
};

struct CheckResult {
  int code;
  bool store_counter;
  unsigned long counter;
};

bd::String make_cookie(const CookieContext& context, const Member* opped,
    size_t opped_count, const bd::String& random, unsigned long counter,
    const Crypto& crypto);

CheckResult check_cookie(const CookieContext& context, const Member& opped,
    const char* cookie, int index_hint, unsigned long last_counter,
    const Crypto& crypto);

} // namespace legacy_cookie

#endif

#ifndef _COOKIE_CODEC_H
#define _COOKIE_CODEC_H

#include <cstddef>
#include <ctime>
#include <sys/types.h>

#include <bdlib/src/String.h>

namespace irc_cookie {

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

enum CheckCode {
  CHECK_OK = 0,
  CHECK_NOCOOKIE = 1,
  CHECK_SLACK = 2,
  CHECK_HASH = 3,
  CHECK_COUNTER = 4
};

struct CheckResult {
  CheckCode code;
  bool store_counter;
  unsigned long counter;
};

bd::String make_cookie(const CookieContext& context, const Member* opped,
    size_t opped_count, const bd::String& random, unsigned long counter,
    const Crypto& crypto);

CheckResult check_cookie(const CookieContext& context, const Member& opped,
    const char* cookie, int index_hint, unsigned long last_counter,
    const Crypto& crypto);

} /* namespace irc_cookie */

#endif

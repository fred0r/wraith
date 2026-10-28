#include "LegacyCookieReference.h"

#include <cstdlib>
#include <cstring>

#include <bdlib/src/base64.h>

#include "src/base64.h"
#include "src/compat/strlcpy.h"
#include "src/sprintf.h"

namespace legacy_cookie {

static bd::String cookie_hash(const CookieContext& context, const Member& opped,
    const char* ts, const char* randstring, const char* key,
    const Crypto& crypto)
{
  char tohash[201] = "";

  simple_snprintf(tohash, sizeof(tohash), "%c%s%c%c%c\n%c%c%s%s%s",
      context.salt2.c_str()[0],
      ts,
      randstring[0], randstring[1], randstring[2], randstring[3],
      context.salt2.c_str()[15],
      opped.nick.c_str(),
      opped.userhost.c_str(),
      key);

  return crypto.md5(tohash);
}

static void cookie_key(char *key, size_t key_len, const char* randstring,
    const CookieContext& context)
{
  const char* salt1 = context.salt1.c_str();
  const char* salt2 = context.salt2.c_str();

  simple_snprintf2(key, key_len, "%c%c%c%^s%c%c%c%c%c%c%^s%c%c%c%c%c%c%c%s",
      randstring[0],
      salt1[5],
      randstring[3],
      context.opper.handle.c_str(),
      randstring[2],
      salt1[4],
      salt1[0],
      salt1[1],
      salt1[3],
      salt1[6],
      context.channel.c_str(),
      salt1[10],
      randstring[1],
      salt2[15],
      salt2[13],
      salt1[10],
      salt2[3],
      salt2[1],
      context.opper.userhost.c_str());
}

#define HASH_INDEX1(_x) (8 + (_x))
#define HASH_INDEX2(_x) (16 + (_x))
#define HASH_INDEX3(_x) (18 + (_x))

bd::String make_cookie(const CookieContext& context, const Member* opped,
    size_t opped_count, const bd::String& random, unsigned long counter,
    const Crypto& crypto)
{
  char randstring[5] = "";
  char ts[11] = "";
  char cookie_clear[101] = "";
  char key[150] = "";
  char out[301] = "";

  if (random.length() < 4 || opped_count < 1 || opped_count > 3)
    return bd::String();

  memcpy(randstring, random.c_str(), 4);
  randstring[4] = 0;

  simple_snprintf(ts, sizeof(ts), "%010li", (long) context.now);
  simple_snprintf2(cookie_clear, sizeof(cookie_clear), "%s%s%D",
      randstring, &ts[3], static_cast<unsigned int>(counter));

  cookie_key(key, sizeof(key), randstring, context);

  const bd::String hash1 = cookie_hash(context, opped[0], &ts[4], randstring,
      key, crypto);
  const bd::String hash2 = opped_count > 1
      ? cookie_hash(context, opped[1], &ts[4], randstring, key, crypto)
      : bd::String();
  const bd::String hash3 = opped_count > 2
      ? cookie_hash(context, opped[2], &ts[4], randstring, key, crypto)
      : bd::String();

  const bd::String encrypted = crypto.encrypt(crypto.md5(key),
      bd::String(cookie_clear));
  bd::String encoded_cookie;
  if (!bd::base64Encode(encrypted, encoded_cookie))
    return bd::String();

  if (opped_count > 2) {
    simple_snprintf(out, sizeof(out), "%c%c%c%c%c%c%c%c%c!%s@%s",
        hash1[HASH_INDEX1(0)], hash1[HASH_INDEX2(0)], hash1[HASH_INDEX3(0)],
        hash2[HASH_INDEX1(1)], hash2[HASH_INDEX2(1)], hash2[HASH_INDEX3(1)],
        hash3[HASH_INDEX1(2)], hash3[HASH_INDEX2(2)], hash3[HASH_INDEX3(2)],
        randstring, encoded_cookie.c_str());
  } else if (opped_count > 1) {
    simple_snprintf(out, sizeof(out), "%c%c%c%c%c%c!%s@%s",
        hash1[HASH_INDEX1(0)], hash1[HASH_INDEX2(0)], hash1[HASH_INDEX3(0)],
        hash2[HASH_INDEX1(1)], hash2[HASH_INDEX2(1)], hash2[HASH_INDEX3(1)],
        randstring, encoded_cookie.c_str());
  } else {
    simple_snprintf(out, sizeof(out), "%c%c%c!%s@%s",
        hash1[HASH_INDEX1(0)], hash1[HASH_INDEX2(0)], hash1[HASH_INDEX3(0)],
        randstring, encoded_cookie.c_str());
  }

  return bd::String(out);
}

CheckResult check_cookie(const CookieContext& context, const Member& opped,
    const char* cookie, int index_hint, unsigned long last_counter,
    const Crypto& crypto)
{
#define HOST(_x) (6 + (_x) + ((hashes << 1) + hashes))
#define SALT(_x) (1 + (_x) + ((hashes << 1) + hashes))

  if (!cookie)
    return {3, false, 0};

  const size_t cookie_len = strlen(cookie);
  size_t hashes = 0;

  if (cookie_len > 3 && cookie[3] == '!')
    hashes = 1;
  else if (cookie_len > 6 && cookie[6] == '!')
    hashes = 2;
  else if (cookie_len > 9 && cookie[9] == '!')
    hashes = 3;
  else
    return {3, false, 0};

  if (cookie_len <= HOST(0))
    return {3, false, 0};

  char key[150] = "";
  const char *randstring = &cookie[SALT(0)];

  cookie_key(key, sizeof(key), randstring, context);

  bd::String encoded_cookie(&cookie[HOST(0)]);
  bd::String ciphertext;
  if (!bd::base64Decode(encoded_cookie, ciphertext, false))
    return {3, false, 0};

  bd::String cleartext = crypto.decrypt(crypto.md5(key), ciphertext);
  if (cleartext.length() < 4 + 7 + 1)
    return {3, false, 0};

  char ts[8] = "";
  strlcpy(ts, cleartext.c_str() + 4, sizeof(ts));
  unsigned long counter = base64_to_int(cleartext.c_str() + 4 + 7);
  const bd::String hash = cookie_hash(context, opped, &ts[1], randstring, key,
      crypto);

  for (size_t i = index_hint; i < hashes; ++i) {
    const char* cookie_index = cookie + ((i << 1) + i);

    if (hash[HASH_INDEX1(i)] == cookie_index[0]
        && hash[HASH_INDEX2(i)] == cookie_index[1]
        && hash[HASH_INDEX3(i)] == cookie_index[2]) {
      const time_t optime = atol(ts);
      if (((context.now % 10000000) - optime) > 3900)
        return {2, false, 0};

      if (index_hint == 0 && !context.opper_is_self) {
        if (counter <= last_counter)
          return {4, false, 0};

        if (counter > (unsigned long)(-1000))
          counter = 0;

        return {0, true, counter};
      }

      return {0, false, 0};
    }
  }

  return {3, false, 0};
#undef HOST
#undef SALT
}

} // namespace legacy_cookie

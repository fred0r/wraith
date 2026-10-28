#include "cookie_codec.h"

#include <openssl/crypto.h>

#include <bdlib/src/base64.h>

#include <cstdlib>
#include <cstring>

#include "src/base64.h"
#include "src/compat/strlcpy.h"
#include "src/sprintf.h"

namespace irc_cookie {

#define HASH_INDEX1(_x) (8 + (_x))
#define HASH_INDEX2(_x) (16 + (_x))
#define HASH_INDEX3(_x) (18 + (_x))

static bd::String cookie_hash(const CookieContext& context, const Member& opped,
    const char* ts, const bd::String& random, const bd::String& key,
    const Crypto& crypto)
{
  char tohash[201] = "";

  simple_snprintf(tohash, sizeof(tohash), "%c%s%c%c%c\n%c%c%s%s%s",
      context.salt2.c_str()[0],
      ts,
      random.c_str()[0], random.c_str()[1], random.c_str()[2], random.c_str()[3],
      context.salt2.c_str()[15],
      opped.nick.c_str(),
      opped.userhost.c_str(),
      key.c_str());

  bd::String result = crypto.md5(tohash);
  OPENSSL_cleanse(tohash, sizeof(tohash));
  return result;
}

static bd::String cookie_key(const CookieContext& context, const bd::String& random)
{
  char key[150] = "";
  const char* salt1 = context.salt1.c_str();
  const char* salt2 = context.salt2.c_str();

  simple_snprintf2(key, sizeof(key), "%c%c%c%^s%c%c%c%c%c%c%^s%c%c%c%c%c%c%c%s",
      random.c_str()[0],
      salt1[5],
      random.c_str()[3],
      context.opper.handle.c_str(),
      random.c_str()[2],
      salt1[4],
      salt1[0],
      salt1[1],
      salt1[3],
      salt1[6],
      context.channel.c_str(),
      salt1[10],
      random.c_str()[1],
      salt2[15],
      salt2[13],
      salt1[10],
      salt2[3],
      salt2[1],
      context.opper.userhost.c_str());

  return bd::String(key);
}

bd::String make_cookie(const CookieContext& context, const Member* opped,
    size_t opped_count, const bd::String& random, unsigned long counter,
    const Crypto& crypto)
{
  if (!opped || opped_count < 1 || opped_count > 3 || random.length() < 4
      || context.salt1.length() < 32 || context.salt2.length() < 16)
    return bd::String();

  char ts[11] = "";
  simple_snprintf(ts, sizeof(ts), "%010li", (long) context.now);

  char cookie_clear[101] = "";
  simple_snprintf2(cookie_clear, sizeof(cookie_clear), "%s%s%D",
      random.c_str(), &ts[3], static_cast<unsigned int>(counter));

  const bd::String key = cookie_key(context, random);
  const bd::String hash1 = cookie_hash(context, opped[0], &ts[4], random, key, crypto);
  const bd::String hash2 = opped_count > 1
      ? cookie_hash(context, opped[1], &ts[4], random, key, crypto)
      : bd::String();
  const bd::String hash3 = opped_count > 2
      ? cookie_hash(context, opped[2], &ts[4], random, key, crypto)
      : bd::String();

  if (hash1.length() < 21 || (opped_count > 1 && hash2.length() < 20)
      || (opped_count > 2 && hash3.length() < 21))
    return bd::String();

  const bd::String encrypted = crypto.encrypt(crypto.md5(key), bd::String(cookie_clear));
  bd::String encoded;
  if (!bd::base64Encode(encrypted, encoded))
    return bd::String();

  char out[301] = "";

  if (opped_count > 2) {
    simple_snprintf(out, sizeof(out), "%c%c%c%c%c%c%c%c%c!%s@%s",
        hash1[8], hash1[16], hash1[18],
        hash2[9], hash2[17], hash2[19],
        hash3[10], hash3[18], hash3[20],
        random.c_str(), encoded.c_str());
  } else if (opped_count > 1) {
    simple_snprintf(out, sizeof(out), "%c%c%c%c%c%c!%s@%s",
        hash1[8], hash1[16], hash1[18],
        hash2[9], hash2[17], hash2[19],
        random.c_str(), encoded.c_str());
  } else {
    simple_snprintf(out, sizeof(out), "%c%c%c!%s@%s",
        hash1[8], hash1[16], hash1[18],
        random.c_str(), encoded.c_str());
  }

  return bd::String(out);
}

CheckResult check_cookie(const CookieContext& context, const Member& opped,
    const char* cookie, int index_hint, unsigned long last_counter,
    const Crypto& crypto)
{
#define HOST(_x) (6 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */
#define SALT(_x) (1 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */

  if (!cookie)
    return {CHECK_HASH, false, 0};

  const size_t cookie_len = strlen(cookie);
  size_t hashes = 0;

  if (cookie_len > 3 && cookie[3] == '!')
    hashes = 1;
  else if (cookie_len > 6 && cookie[6] == '!')
    hashes = 2;
  else if (cookie_len > 9 && cookie[9] == '!')
    hashes = 3;
  else
    return {CHECK_HASH, false, 0};

  if (cookie_len <= HOST(0) || context.salt1.length() < 32
      || context.salt2.length() < 16)
    return {CHECK_HASH, false, 0};

  const bd::String random(&cookie[SALT(0)], 4);
  const bd::String key = cookie_key(context, random);

  bd::String encoded_cookie(&cookie[HOST(0)]);
  bd::String ciphertext;
  if (!bd::base64Decode(encoded_cookie, ciphertext, false))
    return {CHECK_HASH, false, 0};

  const bd::String cleartext = crypto.decrypt(crypto.md5(key), ciphertext);
  if (cleartext.length() < 4 + 7 + 1)
    return {CHECK_HASH, false, 0};

  char ts[8] = "";
  strlcpy(ts, cleartext.c_str() + 4, sizeof(ts));

  const bd::String hash = cookie_hash(context, opped, &ts[1], random, key, crypto);

  for (size_t i = index_hint; i < hashes; ++i) {
    const char* cookie_index = cookie + ((i << 1) + i);

    if (hash[HASH_INDEX1(i)] == cookie_index[0]
        && hash[HASH_INDEX2(i)] == cookie_index[1]
        && hash[HASH_INDEX3(i)] == cookie_index[2]) {
      const time_t optime = atol(ts);
      if (((context.now % 10000000) - optime) > 3900)
        return {CHECK_SLACK, false, 0};

      if (index_hint == 0 && !context.opper_is_self) {
        unsigned long counter = base64_to_int(cleartext.c_str() + 4 + 7);

        if (counter <= last_counter)
          return {CHECK_COUNTER, false, 0};

        if (counter > (unsigned long) (-1000))
          counter = 0;

        return {CHECK_OK, true, counter};
      }

      return {CHECK_OK, false, 0};
    }
  }

  return {CHECK_HASH, false, 0};

#undef HOST
#undef SALT
}

} /* namespace irc_cookie */

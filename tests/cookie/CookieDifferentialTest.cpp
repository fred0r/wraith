#include "CookieDifferentialTest.h"

#include <cstdio>
#include <random>
#include <string>

#include <openssl/evp.h>

#include <bdlib/src/String.h>

#include "src/libcrypto.h"
#include "src/mod/irc.mod/cookie_codec.h"
#include "LegacyCookieReference.h"

CPPUNIT_TEST_SUITE_REGISTRATION(CookieDifferentialTest);

static bd::String md5_hex(const bd::String& input)
{
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;

  CPPUNIT_ASSERT(EVP_Digest(input.c_str(), input.length(), digest, &digest_len,
      EVP_md5(), nullptr) == 1);

  char hex[33] = "";
  for (unsigned int i = 0; i < digest_len; ++i)
    snprintf(&hex[i * 2], sizeof(hex) - i * 2, "%02x", digest[i]);

  return bd::String(hex, digest_len * 2);
}

static irc_cookie::Crypto new_crypto()
{
  return {md5_hex, encrypt_string, decrypt_string};
}

static legacy_cookie::Crypto legacy_crypto()
{
  return {md5_hex, encrypt_string, decrypt_string};
}

struct Contexts {
  irc_cookie::CookieContext current;
  legacy_cookie::CookieContext legacy;
};

static Contexts make_contexts(int tag, int now)
{
  char channel[32] = "";
  char nick[32] = "";
  char userhost[64] = "";
  char handle[32] = "";
  char salt1[33] = "";
  char salt2[17] = "";

  snprintf(channel, sizeof(channel), "#diff%d", tag);
  snprintf(nick, sizeof(nick), "Oper%d", tag);
  snprintf(userhost, sizeof(userhost), "oper%d!user@host%d", tag, tag);
  snprintf(handle, sizeof(handle), "oper%d", tag);
  snprintf(salt1, sizeof(salt1), "%032d", tag);
  snprintf(salt2, sizeof(salt2), "%016d", tag);

  irc_cookie::Member current_opper;
  current_opper.nick = nick;
  current_opper.userhost = userhost;
  current_opper.handle = handle;

  legacy_cookie::Member legacy_opper;
  legacy_opper.nick = nick;
  legacy_opper.userhost = userhost;
  legacy_opper.handle = handle;

  Contexts result;
  result.current.channel = channel;
  result.current.opper = current_opper;
  result.current.salt1 = salt1;
  result.current.salt2 = salt2;
  result.current.now = now;
  result.current.opper_is_self = false;

  result.legacy.channel = channel;
  result.legacy.opper = legacy_opper;
  result.legacy.salt1 = salt1;
  result.legacy.salt2 = salt2;
  result.legacy.now = now;
  result.legacy.opper_is_self = false;
  return result;
}

static irc_cookie::Member member(const char* nick, const char* userhost,
    const char* handle)
{
  irc_cookie::Member result;
  result.nick = nick;
  result.userhost = userhost;
  result.handle = handle;
  return result;
}

static legacy_cookie::Member legacy_member(const char* nick,
    const char* userhost, const char* handle)
{
  legacy_cookie::Member result;
  result.nick = nick;
  result.userhost = userhost;
  result.handle = handle;
  return result;
}

static void assert_check_equal(const irc_cookie::CheckResult& current,
    const legacy_cookie::CheckResult& legacy)
{
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(current.code), legacy.code);
  CPPUNIT_ASSERT_EQUAL(current.store_counter, legacy.store_counter);
  if (current.store_counter)
    CPPUNIT_ASSERT_EQUAL(current.counter, legacy.counter);
}

void CookieDifferentialTest::generatedCookiesMatch()
{
  const Contexts contexts = make_contexts(1, 1000000);
  const irc_cookie::Crypto ncrypto = new_crypto();
  const legacy_cookie::Crypto lcrypto = legacy_crypto();
  const irc_cookie::Member members[] = {
    member("One", "one!u@h", "one"),
    member("Two", "two!u@h", "two"),
    member("Three", "three!u@h", "three"),
  };
  const legacy_cookie::Member lmembers[] = {
    legacy_member("One", "one!u@h", "one"),
    legacy_member("Two", "two!u@h", "two"),
    legacy_member("Three", "three!u@h", "three"),
  };

  for (size_t count = 1; count <= 3; ++count) {
    const bd::String current = irc_cookie::make_cookie(contexts.current,
        members, count, "Ab3d", 42, ncrypto);
    const bd::String legacy = legacy_cookie::make_cookie(contexts.legacy,
        lmembers, count, "Ab3d", 42, lcrypto);
    CPPUNIT_ASSERT_EQUAL(legacy, current);
  }
}

void CookieDifferentialTest::validCookiesCrossValidate()
{
  const Contexts contexts = make_contexts(2, 2000000);
  const irc_cookie::Crypto ncrypto = new_crypto();
  const legacy_cookie::Crypto lcrypto = legacy_crypto();
  const irc_cookie::Member member0 = member("One", "one!u@h", "one");
  const irc_cookie::Member member1 = member("Two", "two!u@h", "two");
  const irc_cookie::Member member2 = member("Three", "three!u@h", "three");
  const irc_cookie::Member members[] = {member0, member1, member2};
  const legacy_cookie::Member lmember0 = legacy_member("One", "one!u@h", "one");
  const legacy_cookie::Member lmember1 = legacy_member("Two", "two!u@h", "two");
  const legacy_cookie::Member lmember2 = legacy_member("Three", "three!u@h", "three");
  const legacy_cookie::Member lmembers[] = {lmember0, lmember1, lmember2};

  for (size_t count = 1; count <= 3; ++count) {
    const bd::String current_cookie = irc_cookie::make_cookie(
        contexts.current, members, count, "Ab3d", 42, ncrypto);
    const bd::String legacy_cookie = legacy_cookie::make_cookie(
        contexts.legacy, lmembers, count, "Ab3d", 42, lcrypto);
    CPPUNIT_ASSERT_EQUAL(legacy_cookie, current_cookie);

    for (size_t index = 0; index < count; ++index) {
      const irc_cookie::CheckResult current = irc_cookie::check_cookie(
          contexts.current, members[index], legacy_cookie.c_str(),
          static_cast<int>(index), 0, ncrypto);
      const legacy_cookie::CheckResult legacy = legacy_cookie::check_cookie(
          contexts.legacy, lmembers[index], current_cookie.c_str(),
          static_cast<int>(index), 0, lcrypto);
      CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(current.code));
      CPPUNIT_ASSERT_EQUAL(0, legacy.code);
    }
  }
}

void CookieDifferentialTest::timeAndCounterDecisionsMatch()
{
  Contexts contexts = make_contexts(3, 3000000);
  const irc_cookie::Crypto ncrypto = new_crypto();
  const legacy_cookie::Crypto lcrypto = legacy_crypto();
  const irc_cookie::Member current_member = member("User", "user!u@h", "user");
  const legacy_cookie::Member legacy_member_value =
      legacy_member("User", "user!u@h", "user");
  const bd::String current_cookie = irc_cookie::make_cookie(contexts.current,
      &current_member, 1, "Ab3d", 7, ncrypto);
  const bd::String legacy_cookie = legacy_cookie::make_cookie(contexts.legacy,
      &legacy_member_value, 1, "Ab3d", 7, lcrypto);
  CPPUNIT_ASSERT_EQUAL(legacy_cookie, current_cookie);

  assert_check_equal(
      irc_cookie::check_cookie(contexts.current, current_member,
          current_cookie.c_str(), 0, 6, ncrypto),
      legacy_cookie::check_cookie(contexts.legacy, legacy_member_value,
          legacy_cookie.c_str(), 0, 6, lcrypto));

  contexts.current.now += 3900;
  contexts.legacy.now += 3900;
  assert_check_equal(
      irc_cookie::check_cookie(contexts.current, current_member,
          current_cookie.c_str(), 0, 0, ncrypto),
      legacy_cookie::check_cookie(contexts.legacy, legacy_member_value,
          legacy_cookie.c_str(), 0, 0, lcrypto));

  contexts.current.now += 1;
  contexts.legacy.now += 1;
  assert_check_equal(
      irc_cookie::check_cookie(contexts.current, current_member,
          current_cookie.c_str(), 0, 0, ncrypto),
      legacy_cookie::check_cookie(contexts.legacy, legacy_member_value,
          legacy_cookie.c_str(), 0, 0, lcrypto));
}

void CookieDifferentialTest::malformedLayoutDecisionsMatch()
{
  const Contexts contexts = make_contexts(4, 4000000);
  const irc_cookie::Crypto ncrypto = new_crypto();
  const legacy_cookie::Crypto lcrypto = legacy_crypto();
  const irc_cookie::Member current_member = member("User", "user!u@h", "user");
  const legacy_cookie::Member legacy_member_value =
      legacy_member("User", "user!u@h", "user");
  const char* cookies[] = {
    "", "abc", "abc!", "abcdef!", "abcdefghi!", "abc!Ab3d@", "%%%%",
  };

  for (const char* value : cookies) {
    assert_check_equal(
        irc_cookie::check_cookie(contexts.current, current_member, value, 0, 0,
            ncrypto),
        legacy_cookie::check_cookie(contexts.legacy, legacy_member_value, value,
            0, 0, lcrypto));
  }
}

void CookieDifferentialTest::randomizedDifferential()
{
  std::mt19937 rng(0x5a17c0de);
  const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const irc_cookie::Crypto ncrypto = new_crypto();
  const legacy_cookie::Crypto lcrypto = legacy_crypto();

  for (int iteration = 0; iteration < 100; ++iteration) {
    char salts[2][33] = {};
    for (int s = 0; s < 2; ++s) {
      for (int i = 0; i < 32; ++i)
        salts[s][i] = alphabet[rng() % (sizeof(alphabet) - 1)];
    }
    salts[0][32] = 0;
    salts[1][32] = 0;

    Contexts contexts = make_contexts(iteration + 10,
        1000000 + static_cast<int>(rng() % 9000000));
    contexts.current.salt1 = salts[0];
    contexts.current.salt2 = salts[1];
    contexts.legacy.salt1 = salts[0];
    contexts.legacy.salt2 = salts[1];

    irc_cookie::Member members[3];
    legacy_cookie::Member lmembers[3];
    char names[3][32] = {};
    char userhosts[3][64] = {};
    char handles[3][32] = {};
    for (int i = 0; i < 3; ++i) {
      snprintf(names[i], sizeof(names[i]), "Nick%d_%d", iteration, i);
      snprintf(userhosts[i], sizeof(userhosts[i]), "user%d!u%d@host%d",
          iteration, i, i);
      snprintf(handles[i], sizeof(handles[i]), "Handle%d_%d", iteration, i);
      members[i] = member(names[i], userhosts[i], handles[i]);
      lmembers[i] = legacy_member(names[i], userhosts[i], handles[i]);
    }

    char random[5] = {};
    for (int i = 0; i < 4; ++i)
      random[i] = alphabet[rng() % (sizeof(alphabet) - 1)];
    const unsigned long counter = 1 + (rng() % 500000);

    for (size_t count = 1; count <= 3; ++count) {
      const bd::String current_cookie = irc_cookie::make_cookie(
          contexts.current, members, count, random, counter, ncrypto);
      const bd::String legacy_cookie = legacy_cookie::make_cookie(
          contexts.legacy, lmembers, count, random, counter, lcrypto);
      CPPUNIT_ASSERT_EQUAL(legacy_cookie, current_cookie);

      for (size_t index = 0; index < count; ++index) {
        const irc_cookie::CheckResult current = irc_cookie::check_cookie(
            contexts.current, members[index], legacy_cookie.c_str(),
            static_cast<int>(index), 0, ncrypto);
        const legacy_cookie::CheckResult legacy =
            legacy_cookie::check_cookie(contexts.legacy, lmembers[index],
                current_cookie.c_str(), static_cast<int>(index), 0, lcrypto);
        CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(current.code));
        CPPUNIT_ASSERT_EQUAL(0, legacy.code);
      }
    }
  }
}

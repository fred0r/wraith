#include "CookieCodecTest.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <openssl/evp.h>

#include <bdlib/src/String.h>

#include "src/libcrypto.h"
#include "src/crypto/aes_util.h"
#include "src/mod/irc.mod/cookie_codec.h"

CPPUNIT_TEST_SUITE_REGISTRATION(CookieCodecTest);

static bd::String test_md5(const bd::String& input)
{
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;

  CPPUNIT_ASSERT(EVP_Digest(input.c_str(), input.length(), digest, &digest_len,
      EVP_md5(), nullptr) == 1);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned int>(16), digest_len);

  char hex[33] = "";
  for (unsigned int i = 0; i < digest_len; ++i)
    snprintf(&hex[i * 2], sizeof(hex) - i * 2, "%02x", digest[i]);

  return bd::String(hex, digest_len * 2);
}

static bd::String identity_encrypt(const bd::String&, const bd::String& data)
{
  return data;
}

static bd::String short_decrypt(const bd::String&, const bd::String&)
{
  return bd::String("short");
}

static irc_cookie::Crypto real_crypto()
{
  irc_cookie::Crypto crypto;

  crypto.md5 = test_md5;
  crypto.encrypt = encrypt_string;
  crypto.decrypt = decrypt_string;
  return crypto;
}

static irc_cookie::Crypto short_plaintext_crypto()
{
  irc_cookie::Crypto crypto;

  crypto.md5 = test_md5;
  crypto.encrypt = identity_encrypt;
  crypto.decrypt = short_decrypt;
  return crypto;
}

static irc_cookie::Member make_member(const char* nick, const char* userhost,
    const char* handle)
{
  irc_cookie::Member member;

  member.nick = nick;
  member.userhost = userhost;
  member.handle = handle;
  return member;
}

static irc_cookie::CookieContext make_context()
{
  irc_cookie::CookieContext context;

  context.channel = "#test";
  context.opper = make_member("Oper", "oper!user@host", "oper");
  context.salt1 = "0123456789abcdef0123456789abcdef";
  context.salt2 = "0123456789abcdef";
  context.now = 1000000;
  context.opper_is_self = false;
  return context;
}

void CookieCodecTest::layoutRejectsMalformed()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const irc_cookie::Crypto crypto = real_crypto();

  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, nullptr, 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "", 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abc", 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abc!", 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abcdef!", 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abcdefghi!", 0, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abc!Ab3d@", 0, 0, crypto).code);
}

void CookieCodecTest::makeFormats()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  irc_cookie::Member opped[3];

  opped[0] = make_member("One", "one!u@h", "one");
  opped[1] = make_member("Two", "two!u@h", "two");
  opped[2] = make_member("Three", "three!u@h", "three");

  const bd::String one = irc_cookie::make_cookie(context, opped, 1, "Ab3d", 1, crypto);
  const bd::String two = irc_cookie::make_cookie(context, opped, 2, "Ab3d", 1, crypto);
  const bd::String three = irc_cookie::make_cookie(context, opped, 3, "Ab3d", 1, crypto);

  CPPUNIT_ASSERT(!one.isEmpty());
  CPPUNIT_ASSERT(!two.isEmpty());
  CPPUNIT_ASSERT(!three.isEmpty());
  CPPUNIT_ASSERT_EQUAL('!', one[3]);
  CPPUNIT_ASSERT_EQUAL('!', two[6]);
  CPPUNIT_ASSERT_EQUAL('!', three[9]);
  CPPUNIT_ASSERT(strstr(one.c_str(), "!Ab3d@") != nullptr);
  CPPUNIT_ASSERT(strstr(two.c_str(), "!Ab3d@") != nullptr);
  CPPUNIT_ASSERT(strstr(three.c_str(), "!Ab3d@") != nullptr);
  CPPUNIT_ASSERT(one.length() <= 300);
  CPPUNIT_ASSERT(two.length() <= 300);
  CPPUNIT_ASSERT(three.length() <= 300);
}

void CookieCodecTest::roundtrip()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  irc_cookie::Member opped[3];

  opped[0] = make_member("One", "one!u@h", "one");
  opped[1] = make_member("Two", "two!u@h", "two");
  opped[2] = make_member("Three", "three!u@h", "three");

  const bd::String one = irc_cookie::make_cookie(context, opped, 1, "Ab3d", 1, crypto);
  irc_cookie::CheckResult result = irc_cookie::check_cookie(context, opped[0],
      one.c_str(), 0, 0, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK, result.code);
  CPPUNIT_ASSERT(result.store_counter);

  const bd::String two = irc_cookie::make_cookie(context, opped, 2, "Ab3d", 2, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(context, opped[0], two.c_str(), 0, 1, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(context, opped[1], two.c_str(), 1, 0, crypto).code);

  const bd::String three = irc_cookie::make_cookie(context, opped, 3, "Ab3d", 3, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(context, opped[0], three.c_str(), 0, 2, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(context, opped[1], three.c_str(), 1, 0, crypto).code);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(context, opped[2], three.c_str(), 2, 0, crypto).code);
}

void CookieCodecTest::rejectsBadHash()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1, crypto);

  cookie[0] = cookie[0] == 'a' ? 'b' : 'a';

  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, cookie.c_str(), 0, 0, crypto).code);
}

void CookieCodecTest::rejectsWrongMembers()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1, crypto);

  irc_cookie::CookieContext wrong_opper = context;
  wrong_opper.opper.userhost = "oper!stale@host";
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(wrong_opper, opped, cookie.c_str(), 0, 0, crypto).code);

  const irc_cookie::Member wrong_opped = make_member("User", "user!other@host", "user");
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, wrong_opped, cookie.c_str(), 0, 0, crypto).code);
}

void CookieCodecTest::timeSkew()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1, crypto);

  irc_cookie::CookieContext check = context;
  check.now = context.now + 3900;
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK,
      irc_cookie::check_cookie(check, opped, cookie.c_str(), 0, 0, crypto).code);

  check.now = context.now + 3901;
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_SLACK,
      irc_cookie::check_cookie(check, opped, cookie.c_str(), 0, 0, crypto).code);
}

void CookieCodecTest::counterHandling()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  irc_cookie::Member opped[2];

  opped[0] = make_member("One", "one!u@h", "one");
  opped[1] = make_member("Two", "two!u@h", "two");

  const bd::String first = irc_cookie::make_cookie(context, opped, 1, "Ab3d", 1, crypto);
  irc_cookie::CheckResult result = irc_cookie::check_cookie(context, opped[0],
      first.c_str(), 0, 0, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK, result.code);
  CPPUNIT_ASSERT(result.store_counter);
  CPPUNIT_ASSERT_EQUAL(1UL, result.counter);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_COUNTER,
      irc_cookie::check_cookie(context, opped[0], first.c_str(), 0, 1, crypto).code);

  const bd::String second = irc_cookie::make_cookie(context, opped, 1, "Ab3d", 2, crypto);
  result = irc_cookie::check_cookie(context, opped[0], second.c_str(), 0, 1, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK, result.code);
  CPPUNIT_ASSERT(result.store_counter);
  CPPUNIT_ASSERT_EQUAL(2UL, result.counter);

  const bd::String two = irc_cookie::make_cookie(context, opped, 2, "Ab3d", 1, crypto);
  result = irc_cookie::check_cookie(context, opped[1], two.c_str(), 1, 1, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK, result.code);
  CPPUNIT_ASSERT(!result.store_counter);
}

void CookieCodecTest::ownBotCounterExemption()
{
  irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1, crypto);

  context.opper_is_self = true;
  const irc_cookie::CheckResult result = irc_cookie::check_cookie(context, opped,
      cookie.c_str(), 0, 1, crypto);
  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_OK, result.code);
  CPPUNIT_ASSERT(!result.store_counter);
}

void CookieCodecTest::shortPlaintext()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto make_crypto = short_plaintext_crypto();
  const irc_cookie::Crypto check_crypto = short_plaintext_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1,
      make_crypto);

  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, cookie.c_str(), 0, 0,
          check_crypto).code);
}

void CookieCodecTest::malformedBase64()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");

  CPPUNIT_ASSERT_EQUAL(irc_cookie::CHECK_HASH,
      irc_cookie::check_cookie(context, opped, "abc!Ab3d@!!!!", 0, 0,
          crypto).code);
}

void CookieCodecTest::goldenCookie()
{
  const irc_cookie::CookieContext context = make_context();
  const irc_cookie::Crypto crypto = real_crypto();
  const irc_cookie::Member opped = make_member("User", "user!u@h", "user");
  const bd::String cookie = irc_cookie::make_cookie(context, &opped, 1, "Ab3d", 1,
      crypto);

  CPPUNIT_ASSERT_EQUAL(bd::String("bdf!Ab3d@PQTiVeOrM7X5lG6N9CtXow=="), cookie);
}

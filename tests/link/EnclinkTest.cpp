#include "EnclinkTest.h"

#include <cstring>
#include <string>

#include <bdlib/src/String.h>

#include "src/enclink_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION(EnclinkTest);

void EnclinkTest::randomKey()
{
  char key[LINK_KEY_LEN + 1] = "";

  CPPUNIT_ASSERT(link_random_key(key));
  CPPUNIT_ASSERT_EQUAL(LINK_KEY_LEN, strlen(key));
  CPPUNIT_ASSERT(strspn(key, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/") ==
      static_cast<size_t>(LINK_KEY_LEN));
}

void EnclinkTest::randomSeed()
{
  int seed = 0;

  CPPUNIT_ASSERT(link_random_seed(seed));
  CPPUNIT_ASSERT(seed > 0);
  CPPUNIT_ASSERT(seed <= 0x7ffffffe);
}

void EnclinkTest::parseSeeds()
{
  int seed = -1;

  CPPUNIT_ASSERT(parse_link_seed("0", seed));
  CPPUNIT_ASSERT_EQUAL(0, seed);
  CPPUNIT_ASSERT(parse_link_seed(" 42\r", seed));
  CPPUNIT_ASSERT_EQUAL(42, seed);
  CPPUNIT_ASSERT(parse_link_seed("2147483647", seed));
  CPPUNIT_ASSERT_EQUAL(2147483647, seed);

  CPPUNIT_ASSERT(!parse_link_seed(NULL, seed));
  CPPUNIT_ASSERT(!parse_link_seed("", seed));
  CPPUNIT_ASSERT(!parse_link_seed("   ", seed));
  CPPUNIT_ASSERT(!parse_link_seed("-1", seed));
  CPPUNIT_ASSERT(!parse_link_seed("42x", seed));
  CPPUNIT_ASSERT(!parse_link_seed("2147483648", seed));
  CPPUNIT_ASSERT(!parse_link_seed("999999999999999999999999999999", seed));
}

void EnclinkTest::formatMessages()
{
  CPPUNIT_ASSERT(format_link_message("").isEmpty());
  CPPUNIT_ASSERT_EQUAL(bd::String("plain"), format_link_message("plain"));
  CPPUNIT_ASSERT_EQUAL(bd::String("plain"),
      format_link_message("%s", "plain\r\n"));
  CPPUNIT_ASSERT_EQUAL(bd::String("hello"),
      format_link_message("%s", "hello\r\nignored"));

  const std::string large(4096, 'q');
  const bd::String formatted = format_link_message("%s", large.c_str());
  CPPUNIT_ASSERT_EQUAL(large.size(), formatted.length());
  CPPUNIT_ASSERT_EQUAL(bd::String(large), formatted);
}

#include "Base64Test.h"

#include <string>

#include <bdlib/src/String.h>
#include "src/base64.h"

CPPUNIT_TEST_SUITE_REGISTRATION(Base64Test);

void Base64Test::standardVectors()
{
  CPPUNIT_ASSERT_EQUAL(bd::String(""), Base64::encode(""));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zg=="), Base64::encode("f"));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zm8="), Base64::encode("fo"));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zm9v"), Base64::encode("foo"));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zm9vYg=="), Base64::encode("foob"));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zm9vYmE="), Base64::encode("fooba"));
  CPPUNIT_ASSERT_EQUAL(bd::String("Zm9vYmFy"), Base64::encode("foobar"));

  CPPUNIT_ASSERT_EQUAL(bd::String("f"), Base64::decode("Zg=="));
  CPPUNIT_ASSERT_EQUAL(bd::String("fo"), Base64::decode("Zm8="));
  CPPUNIT_ASSERT_EQUAL(bd::String("foo"), Base64::decode("Zm9v"));
  CPPUNIT_ASSERT_EQUAL(bd::String("foobar"), Base64::decode("Zm9vYmFy"));
}

void Base64Test::binaryRoundtrip()
{
  const char raw[] = {'a', '\0', 'b', '\xff'};
  const bd::String input(raw, sizeof(raw));

  CPPUNIT_ASSERT_EQUAL(input, Base64::decode(Base64::encode(input)));
}

void Base64Test::eggdropRoundtrip()
{
  const bd::String input("eggdrop base64");

  CPPUNIT_ASSERT_EQUAL(input,
      Base64::decode_eggdrop(Base64::encode_eggdrop(input)));
}

void Base64Test::integerRoundtrip()
{
  const unsigned int values[] = {0, 1, 63, 64, 255, 4096, 100000};

  for (unsigned int value : values)
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(value),
        Base64::int_decode(Base64::int_encode(value)));
}

void Base64Test::integerVectors()
{
  struct integer_vector {
    unsigned int value;
    const char *encoded;
  };
  const integer_vector vectors[] = {
    {0, "A"},
    {1, "B"},
    {25, "Z"},
    {26, "a"},
    {51, "z"},
    {52, "0"},
    {61, "9"},
    {62, "["},
    {63, "]"},
    {64, "BA"},
    {255, "D]"},
    {4096, "BAA"},
  };

  for (const integer_vector& vector : vectors) {
    CPPUNIT_ASSERT_EQUAL(std::string(vector.encoded),
        Base64::int_encode(vector.value));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(vector.value),
        Base64::int_decode(vector.encoded));
  }
}

void Base64Test::integerMalformed()
{
  CPPUNIT_ASSERT_EQUAL(0, Base64::int_decode(std::string("\x80", 1)));
  CPPUNIT_ASSERT_EQUAL(1, Base64::int_decode(std::string("\x80" "B", 2)));
  CPPUNIT_ASSERT_EQUAL(0, Base64::int_decode(std::string(32, '\xff')));
}

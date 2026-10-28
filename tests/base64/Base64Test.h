#ifndef TESTS_BASE64_BASE64TEST_H
#define TESTS_BASE64_BASE64TEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class Base64Test : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(Base64Test);
  CPPUNIT_TEST(standardVectors);
  CPPUNIT_TEST(binaryRoundtrip);
  CPPUNIT_TEST(eggdropRoundtrip);
  CPPUNIT_TEST(integerRoundtrip);
  CPPUNIT_TEST(integerVectors);
  CPPUNIT_TEST(integerMalformed);
  CPPUNIT_TEST_SUITE_END();

public:
  void standardVectors();
  void binaryRoundtrip();
  void eggdropRoundtrip();
  void integerRoundtrip();
  void integerVectors();
  void integerMalformed();
};

#endif

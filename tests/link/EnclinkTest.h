#ifndef TESTS_LINK_ENCLINKTEST_H
#define TESTS_LINK_ENCLINKTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class EnclinkTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(EnclinkTest);
  CPPUNIT_TEST(randomKey);
  CPPUNIT_TEST(randomSeed);
  CPPUNIT_TEST(parseSeeds);
  CPPUNIT_TEST(formatMessages);
  CPPUNIT_TEST_SUITE_END();

public:
  void randomKey();
  void randomSeed();
  void parseSeeds();
  void formatMessages();
};

#endif

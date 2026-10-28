#ifndef TESTS_COOKIE_COOKIEDIFFERENTIALTEST_H
#define TESTS_COOKIE_COOKIEDIFFERENTIALTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class CookieDifferentialTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(CookieDifferentialTest);
  CPPUNIT_TEST(generatedCookiesMatch);
  CPPUNIT_TEST(validCookiesCrossValidate);
  CPPUNIT_TEST(timeAndCounterDecisionsMatch);
  CPPUNIT_TEST(malformedLayoutDecisionsMatch);
  CPPUNIT_TEST(randomizedDifferential);
  CPPUNIT_TEST_SUITE_END();

public:
  void generatedCookiesMatch();
  void validCookiesCrossValidate();
  void timeAndCounterDecisionsMatch();
  void malformedLayoutDecisionsMatch();
  void randomizedDifferential();
};

#endif

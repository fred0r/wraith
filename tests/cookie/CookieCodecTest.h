#ifndef TESTS_COOKIE_COOKIECODECTEST_H
#define TESTS_COOKIE_COOKIECODECTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class CookieCodecTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(CookieCodecTest);
  CPPUNIT_TEST(layoutRejectsMalformed);
  CPPUNIT_TEST(makeFormats);
  CPPUNIT_TEST(roundtrip);
  CPPUNIT_TEST(rejectsBadHash);
  CPPUNIT_TEST(rejectsWrongMembers);
  CPPUNIT_TEST(timeSkew);
  CPPUNIT_TEST(counterHandling);
  CPPUNIT_TEST(ownBotCounterExemption);
  CPPUNIT_TEST(shortPlaintext);
  CPPUNIT_TEST(malformedBase64);
  CPPUNIT_TEST(goldenCookie);
  CPPUNIT_TEST_SUITE_END();

public:
  void layoutRejectsMalformed();
  void makeFormats();
  void roundtrip();
  void rejectsBadHash();
  void rejectsWrongMembers();
  void timeSkew();
  void counterHandling();
  void ownBotCounterExemption();
  void shortPlaintext();
  void malformedBase64();
  void goldenCookie();
};

#endif

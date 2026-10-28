#ifndef TESTS_PENDING_COOKIES_PENDINGCOOKIEDIFFERENTIALTEST_H
#define TESTS_PENDING_COOKIES_PENDINGCOOKIEDIFFERENTIALTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PendingCookieDifferentialTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(PendingCookieDifferentialTest);
  CPPUNIT_TEST(refreshAndExpiryMatch);
  CPPUNIT_TEST(failurePolicyMatch);
  CPPUNIT_TEST_SUITE_END();

public:
  void refreshAndExpiryMatch();
  void failurePolicyMatch();
};

#endif

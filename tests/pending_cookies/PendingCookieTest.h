#ifndef TESTS_PENDING_COOKIES_PENDINGCOOKIETEST_H
#define TESTS_PENDING_COOKIES_PENDINGCOOKIETEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PendingCookieTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(PendingCookieTest);
  CPPUNIT_TEST(storeEntry);
  CPPUNIT_TEST(queueLimit);
  CPPUNIT_TEST(refreshRequiresBothSides);
  CPPUNIT_TEST(refreshSuccess);
  CPPUNIT_TEST(refreshCaseInsensitive);
  CPPUNIT_TEST(refreshIgnoresOtherChannelsAndNicks);
  CPPUNIT_TEST(refreshMissingMembers);
  CPPUNIT_TEST(refreshMultipleEntries);
  CPPUNIT_TEST(failureKicksOpped);
  CPPUNIT_TEST(failureKicksOpper);
  CPPUNIT_TEST(failureKicksBoth);
  CPPUNIT_TEST(failureNoKickWhenAlreadySent);
  CPPUNIT_TEST(failureDeflagsOpper);
  CPPUNIT_TEST(timeoutBoundary);
  CPPUNIT_TEST(timeoutValid);
  CPPUNIT_TEST(timeoutInvalid);
  CPPUNIT_TEST(timeoutMissingMembers);
  CPPUNIT_TEST_SUITE_END();

public:
  void storeEntry();
  void queueLimit();
  void refreshRequiresBothSides();
  void refreshSuccess();
  void refreshCaseInsensitive();
  void refreshIgnoresOtherChannelsAndNicks();
  void refreshMissingMembers();
  void refreshMultipleEntries();
  void failureKicksOpped();
  void failureKicksOpper();
  void failureKicksBoth();
  void failureNoKickWhenAlreadySent();
  void failureDeflagsOpper();
  void timeoutBoundary();
  void timeoutValid();
  void timeoutInvalid();
  void timeoutMissingMembers();
};

#endif

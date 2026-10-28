#ifndef TESTS_IRC_INVITESHARDTEST_H
#define TESTS_IRC_INVITESHARDTEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class InviteShardTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(InviteShardTest);
  CPPUNIT_TEST(canonicalGroupUsesChannelOrder);
  CPPUNIT_TEST(distributesByTargetGroup);
  CPPUNIT_TEST(avoidsSelfAssignment);
  CPPUNIT_TEST(deterministicHandles);
  CPPUNIT_TEST_SUITE_END();

public:
  void canonicalGroupUsesChannelOrder();
  void distributesByTargetGroup();
  void avoidsSelfAssignment();
  void deterministicHandles();
};

#endif

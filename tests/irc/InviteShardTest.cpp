#include "InviteShardTest.h"

#include <map>
#include <string>
#include <vector>

#include "src/mod/irc.mod/invite_shard.h"

CPPUNIT_TEST_SUITE_REGISTRATION(InviteShardTest);

static invite_shard::Bot make_bot(const char *handle,
    const std::vector<std::string>& groups)
{
  invite_shard::Bot bot;

  bot.handle = handle;
  bot.groups = groups;
  return bot;
}

static std::map<std::string, std::string> assignment_map(
    const std::vector<invite_shard::Assignment>& assignments)
{
  std::map<std::string, std::string> result;

  for (const auto& assignment : assignments)
    result[assignment.target] = assignment.inviter;
  return result;
}

void InviteShardTest::canonicalGroupUsesChannelOrder()
{
  const std::vector<std::string> channel_groups = { "main", "znc", "alpha" };
  const std::vector<std::string> bot_groups = { "znc", "alpha" };

  CPPUNIT_ASSERT_EQUAL(std::string("znc"),
      invite_shard::canonical_group(channel_groups, bot_groups));
  CPPUNIT_ASSERT_EQUAL(std::string("main"),
      invite_shard::canonical_group(channel_groups, { "main", "alpha" }));
  CPPUNIT_ASSERT(invite_shard::canonical_group(channel_groups, { "other" }).empty());
}

void InviteShardTest::distributesByTargetGroup()
{
  const std::vector<std::string> channel_groups = { "main", "znc" };
  const std::vector<invite_shard::Bot> inviters = {
    make_bot("b0nzai", { "drei", "main" }),
    make_bot("m0re", { "drei", "main" }),
    make_bot("x0r", { "main" })
  };
  const std::vector<invite_shard::Bot> targets = {
    make_bot("t1", { "main" }),
    make_bot("t2", { "main" }),
    make_bot("t3", { "znc" }),
    make_bot("t4", { "znc" }),
    make_bot("t5", { "main", "znc" })
  };
  const std::map<std::string, std::string> assigned = assignment_map(
      invite_shard::assign(channel_groups, inviters, targets));

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), assigned.size());
  CPPUNIT_ASSERT_EQUAL(std::string("b0nzai"), assigned.at("t1"));
  CPPUNIT_ASSERT_EQUAL(std::string("m0re"), assigned.at("t2"));
  CPPUNIT_ASSERT_EQUAL(std::string("b0nzai"), assigned.at("t3"));
  CPPUNIT_ASSERT_EQUAL(std::string("m0re"), assigned.at("t4"));
  CPPUNIT_ASSERT_EQUAL(std::string("x0r"), assigned.at("t5"));
}

void InviteShardTest::avoidsSelfAssignment()
{
  const std::vector<std::string> channel_groups = { "main" };
  const std::vector<invite_shard::Bot> inviters = {
    make_bot("a", { "main" }),
    make_bot("b", { "main" })
  };
  const std::vector<invite_shard::Bot> targets = {
    make_bot("a", { "main" })
  };
  const std::vector<invite_shard::Assignment> assignments =
      invite_shard::assign(channel_groups, inviters, targets);

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), assignments.size());
  CPPUNIT_ASSERT_EQUAL(std::string("a"), assignments[0].target);
  CPPUNIT_ASSERT_EQUAL(std::string("b"), assignments[0].inviter);
}

void InviteShardTest::deterministicHandles()
{
  const std::vector<std::string> channel_groups = { "main" };
  const std::vector<invite_shard::Bot> inviters = {
    make_bot("Zed", { "main" }),
    make_bot("alpha", { "main" })
  };
  const std::vector<invite_shard::Bot> targets = {
    make_bot("Two", { "main" }),
    make_bot("one", { "main" })
  };
  const std::vector<invite_shard::Assignment> first =
      invite_shard::assign(channel_groups, inviters, targets);
  const std::vector<invite_shard::Assignment> second =
      invite_shard::assign(channel_groups, inviters, targets);

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), first.size());
  CPPUNIT_ASSERT_EQUAL(first.size(), second.size());
  for (size_t i = 0; i < first.size(); ++i) {
    CPPUNIT_ASSERT_EQUAL(first[i].target, second[i].target);
    CPPUNIT_ASSERT_EQUAL(first[i].inviter, second[i].inviter);
  }
}

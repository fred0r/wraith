#include "PendingCookieDifferentialTest.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/mod/irc.mod/pending_cookies.h"
#include "LegacyPendingCookieQueue.h"

CPPUNIT_TEST_SUITE_REGISTRATION(PendingCookieDifferentialTest);

class RecordingSink : public PendingCookieSink {
public:
  std::vector<std::string> who;
  std::vector<std::string> kicked_opped;
  std::vector<std::string> kicked_opper;
  std::vector<std::string> deflags;
  std::vector<int> check_results;
  std::vector<int> rolls;
  int check_calls = 0;
  bool members_exist = true;
  bool can_kick_opped = true;
  bool can_kick_opper = true;

  bool member_exists(const char*, const char*) override
  {
    return members_exist;
  }

  int check_cookie(const PendingCookieEntry&) override
  {
    ++check_calls;
    if (check_results.empty())
      return 0;
    const int result = check_results.front();
    check_results.erase(check_results.begin());
    return result;
  }

  int random7() override
  {
    if (rolls.empty())
      return 3;
    const int roll = rolls.front();
    rolls.erase(rolls.begin());
    return roll;
  }

  bool can_kick(const char*, const char* nick) override
  {
    return std::strcmp(nick, "User") == 0 ? can_kick_opped : can_kick_opper;
  }

  void send_who(const char* nick) override
  {
    who.push_back(nick);
  }

  void kick_opped(const PendingCookieEntry& entry) override
  {
    kicked_opped.push_back(entry.opped_nick);
  }

  void kick_opper(const PendingCookieEntry& entry) override
  {
    kicked_opper.push_back(entry.opper_nick);
  }

  void deflag_opper(const PendingCookieEntry& entry) override
  {
    deflags.push_back(entry.opper_nick);
  }

  void log(PendingCookieLog, const char*) override
  {
  }
};

static PendingCookieEntry make_entry(const char* chan, const char* opper,
    const char* opped, int index = 0, time_t timestamp = 100)
{
  PendingCookieEntry entry = {};

  std::snprintf(entry.chan, sizeof(entry.chan), "%s", chan);
  std::snprintf(entry.opper_nick, sizeof(entry.opper_nick), "%s", opper);
  std::snprintf(entry.opped_nick, sizeof(entry.opped_nick), "%s", opped);
  std::snprintf(entry.cookie, sizeof(entry.cookie), "cookie");
  entry.index_hint = index;
  entry.timestamp = timestamp;
  return entry;
}

template <typename Queue>
static void run_refresh_and_expiry(Queue& queue, RecordingSink& sink)
{
  queue.add(make_entry("#chan", "Oper1", "User1"), sink);
  queue.add(make_entry("#chan", "Oper2", "User2"), sink);

  queue.refresh("#chan", "Oper1", 100, sink);
  queue.refresh("#chan", "User1", 100, sink);

  sink.check_results.push_back(1);
  sink.rolls = {1, 0};
  queue.refresh("#chan", "Oper2", 100, sink);
  queue.refresh("#chan", "User2", 100, sink);

  sink.members_exist = false;
  queue.add(make_entry("#chan", "Oper3", "User3"), sink);
  queue.refresh("#chan", "Oper3", 100, sink);
  queue.refresh("#chan", "User3", 100, sink);

  sink.members_exist = true;
  sink.check_results = {0, 1};
  sink.rolls = {2, 1};
  queue.add(make_entry("#chan", "Oper4", "User4", 0, 100), sink);
  queue.add(make_entry("#chan", "Oper5", "User5", 0, 100), sink);
  queue.expire(105, sink);
  queue.expire(106, sink);
}

template <typename Queue>
static void run_failure_policy(Queue& queue, RecordingSink& sink)
{
  queue.add(make_entry("#chan", "Oper", "User", 0), sink);

  sink.check_results = {3};
  sink.rolls = {0, 0};
  sink.can_kick_opped = true;
  sink.can_kick_opper = true;
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  queue.add(make_entry("#chan", "Oper", "User", 0), sink);
  sink.check_results = {3};
  sink.rolls = {0, 0};
  sink.can_kick_opped = false;
  sink.can_kick_opper = false;
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);
}

void PendingCookieDifferentialTest::refreshAndExpiryMatch()
{
  PendingCookieQueue current;
  LegacyPendingCookieQueue legacy;
  RecordingSink current_sink;
  RecordingSink legacy_sink;

  run_refresh_and_expiry(current, current_sink);
  run_refresh_and_expiry(legacy, legacy_sink);

  CPPUNIT_ASSERT_EQUAL(current.size(), legacy.size());
  CPPUNIT_ASSERT_EQUAL(current_sink.check_calls, legacy_sink.check_calls);
  CPPUNIT_ASSERT(current_sink.who == legacy_sink.who);
  CPPUNIT_ASSERT(current_sink.kicked_opped == legacy_sink.kicked_opped);
  CPPUNIT_ASSERT(current_sink.kicked_opper == legacy_sink.kicked_opper);
  CPPUNIT_ASSERT(current_sink.deflags == legacy_sink.deflags);
}

void PendingCookieDifferentialTest::failurePolicyMatch()
{
  PendingCookieQueue current;
  LegacyPendingCookieQueue legacy;
  RecordingSink current_sink;
  RecordingSink legacy_sink;

  run_failure_policy(current, current_sink);
  run_failure_policy(legacy, legacy_sink);

  CPPUNIT_ASSERT_EQUAL(current.size(), legacy.size());
  CPPUNIT_ASSERT_EQUAL(current_sink.check_calls, legacy_sink.check_calls);
  CPPUNIT_ASSERT(current_sink.who == legacy_sink.who);
  CPPUNIT_ASSERT(current_sink.kicked_opped == legacy_sink.kicked_opped);
  CPPUNIT_ASSERT(current_sink.kicked_opper == legacy_sink.kicked_opper);
  CPPUNIT_ASSERT(current_sink.deflags == legacy_sink.deflags);
}

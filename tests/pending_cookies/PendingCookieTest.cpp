#include "PendingCookieTest.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/mod/irc.mod/pending_cookies.h"

CPPUNIT_TEST_SUITE_REGISTRATION(PendingCookieTest);

class FakeSink : public PendingCookieSink {
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
    const char* opped, const char* cookie = "cookie", int index = 0,
    time_t timestamp = 100)
{
  PendingCookieEntry entry = {};

  std::snprintf(entry.chan, sizeof(entry.chan), "%s", chan);
  std::snprintf(entry.opper_nick, sizeof(entry.opper_nick), "%s", opper);
  std::snprintf(entry.opped_nick, sizeof(entry.opped_nick), "%s", opped);
  std::snprintf(entry.cookie, sizeof(entry.cookie), "%s", cookie);
  entry.index_hint = index;
  entry.timestamp = timestamp;
  return entry;
}

void PendingCookieTest::storeEntry()
{
  FakeSink sink;
  PendingCookieQueue queue;
  const PendingCookieEntry entry = make_entry("#chan", "Oper", "User", "abc", 2, 42);

  CPPUNIT_ASSERT(queue.add(entry, sink));
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), queue.size());

  const PendingCookieEntry& stored = queue.get(0);
  CPPUNIT_ASSERT_EQUAL(std::string("#chan"), std::string(stored.chan));
  CPPUNIT_ASSERT_EQUAL(std::string("Oper"), std::string(stored.opper_nick));
  CPPUNIT_ASSERT_EQUAL(std::string("User"), std::string(stored.opped_nick));
  CPPUNIT_ASSERT_EQUAL(std::string("abc"), std::string(stored.cookie));
  CPPUNIT_ASSERT_EQUAL(2, stored.index_hint);
  CPPUNIT_ASSERT_EQUAL(time_t(42), stored.timestamp);
  CPPUNIT_ASSERT(!stored.opper_refreshed);
  CPPUNIT_ASSERT(!stored.opped_refreshed);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), sink.who.size());
  CPPUNIT_ASSERT_EQUAL(std::string("Oper"), sink.who[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("User"), sink.who[1]);
}

void PendingCookieTest::queueLimit()
{
  FakeSink sink;
  PendingCookieQueue queue;

  for (size_t i = 0; i < PENDING_COOKIE_MAX; ++i) {
    char nick[32] = "";
    std::snprintf(nick, sizeof(nick), "User%zu", i);
    CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", nick), sink));
  }

  CPPUNIT_ASSERT_EQUAL(PENDING_COOKIE_MAX, queue.size());
  CPPUNIT_ASSERT(!queue.add(make_entry("#chan", "Oper", "Overflow"), sink));
  CPPUNIT_ASSERT_EQUAL(PENDING_COOKIE_MAX, queue.size());
}

void PendingCookieTest::refreshRequiresBothSides()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User"), sink));

  queue.refresh("#chan", "Oper", 100, sink);
  CPPUNIT_ASSERT_EQUAL(0, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), queue.size());

  queue.refresh("#chan", "User", 100, sink);
  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), queue.size());
}

void PendingCookieTest::refreshSuccess()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User"), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
  CPPUNIT_ASSERT(sink.deflags.empty());
}

void PendingCookieTest::refreshCaseInsensitive()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#Chan", "Oper", "User"), sink));
  queue.refresh("#chan", "oPeR", 100, sink);
  queue.refresh("#CHAN", "uSeR", 100, sink);

  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), queue.size());
}

void PendingCookieTest::refreshIgnoresOtherChannelsAndNicks()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User"), sink));
  queue.refresh("#other", "Oper", 100, sink);
  queue.refresh("#chan", "Other", 100, sink);

  CPPUNIT_ASSERT_EQUAL(0, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), queue.size());
}

void PendingCookieTest::refreshMissingMembers()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.members_exist = false;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User"), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT_EQUAL(0, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), queue.size());
  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
}

void PendingCookieTest::refreshMultipleEntries()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper1", "User1"), sink));
  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper2", "User2"), sink));

  queue.refresh("#chan", "Oper1", 100, sink);
  queue.refresh("#chan", "User1", 100, sink);

  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), queue.size());
  CPPUNIT_ASSERT_EQUAL(std::string("Oper2"), std::string(queue.get(0).opper_nick));
}

void PendingCookieTest::failureKicksOpped()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {1, 1};

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 1), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opped.size());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
}

void PendingCookieTest::failureKicksOpper()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {2, 0};

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 1), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opper.size());
}

void PendingCookieTest::failureKicksBoth()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {0, 0};

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opped.size());
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opper.size());
}

void PendingCookieTest::failureNoKickWhenAlreadySent()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {0, 0};
  sink.can_kick_opped = false;
  sink.can_kick_opper = false;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
}

void PendingCookieTest::failureDeflagsOpper()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {2, 0};
  sink.can_kick_opper = false;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 1), sink));
  queue.refresh("#chan", "Oper", 100, sink);
  queue.refresh("#chan", "User", 100, sink);

  CPPUNIT_ASSERT(sink.kicked_opper.empty());
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.deflags.size());
  CPPUNIT_ASSERT_EQUAL(std::string("Oper"), sink.deflags[0]);
}

void PendingCookieTest::timeoutBoundary()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0, 100), sink));
  queue.expire(105, sink);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), queue.size());
  CPPUNIT_ASSERT_EQUAL(0, sink.check_calls);

  queue.expire(106, sink);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), queue.size());
  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
}

void PendingCookieTest::timeoutValid()
{
  FakeSink sink;
  PendingCookieQueue queue;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0, 100), sink));
  queue.expire(106, sink);

  CPPUNIT_ASSERT_EQUAL(1, sink.check_calls);
  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
}

void PendingCookieTest::timeoutInvalid()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.check_results.push_back(3);
  sink.rolls = {0, 0};

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0, 100), sink));
  queue.expire(106, sink);

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opped.size());
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), sink.kicked_opper.size());
}

void PendingCookieTest::timeoutMissingMembers()
{
  FakeSink sink;
  PendingCookieQueue queue;
  sink.members_exist = false;

  CPPUNIT_ASSERT(queue.add(make_entry("#chan", "Oper", "User", "cookie", 0, 100), sink));
  queue.expire(106, sink);

  CPPUNIT_ASSERT_EQUAL(0, sink.check_calls);
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), queue.size());
  CPPUNIT_ASSERT(sink.kicked_opped.empty());
  CPPUNIT_ASSERT(sink.kicked_opper.empty());
}

#ifndef TESTS_PENDING_COOKIES_LEGACYPENDINGCOOKIEQUEUE_H
#define TESTS_PENDING_COOKIES_LEGACYPENDINGCOOKIEQUEUE_H

#include <cstddef>
#include <ctime>
#include <vector>

#include "src/mod/irc.mod/pending_cookies.h"

class LegacyPendingCookieQueue {
public:
  bool add(const PendingCookieEntry& entry, PendingCookieSink& sink);
  void refresh(const char* chan, const char* nick, time_t now,
      PendingCookieSink& sink);
  void expire(time_t now, PendingCookieSink& sink);
  size_t size() const { return entries_.size(); }
  const PendingCookieEntry& get(size_t index) const { return entries_[index]; }

private:
  void apply_failure(const PendingCookieEntry& entry, PendingCookieSink& sink);
  std::vector<PendingCookieEntry> entries_;
};

#endif

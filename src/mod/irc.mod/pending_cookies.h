#ifndef _PENDING_COOKIES_H
#define _PENDING_COOKIES_H

#include <cstddef>
#include <ctime>
#include <sys/types.h>
#include <vector>

#include "src/eggdrop.h"

static constexpr size_t PENDING_COOKIE_CHAN_LEN = 81;
static constexpr size_t PENDING_COOKIE_MAX = 32;
static constexpr int PENDING_COOKIE_TIMEOUT = 5;

struct PendingCookieEntry {
  char chan[PENDING_COOKIE_CHAN_LEN];
  char opper_nick[NICKLEN];
  char opped_nick[NICKLEN];
  char cookie[300];
  int index_hint;
  time_t timestamp;
  bool opper_refreshed;
  bool opped_refreshed;
};

enum class PendingCookieLog {
  Info,
  Warning
};

class PendingCookieSink {
public:
  virtual ~PendingCookieSink() {}

  virtual bool member_exists(const char* chan, const char* nick) = 0;
  virtual int check_cookie(const PendingCookieEntry& entry) = 0;
  virtual int random7() = 0;
  virtual bool can_kick(const char* chan, const char* nick) = 0;
  virtual void send_who(const char* nick) = 0;
  virtual void kick_opped(const PendingCookieEntry& entry) = 0;
  virtual void kick_opper(const PendingCookieEntry& entry) = 0;
  virtual void deflag_opper(const PendingCookieEntry& entry) = 0;
  virtual void log(PendingCookieLog level, const char* message) = 0;
};

class PendingCookieQueue {
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

#include "LegacyPendingCookieQueue.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "src/rfc1459.h"

static void logf(PendingCookieSink& sink, PendingCookieLog level,
    const char* format, ...)
{
  char message[512] = "";
  va_list va;

  va_start(va, format);
  vsnprintf(message, sizeof(message), format, va);
  va_end(va);
  sink.log(level, message);
}

void LegacyPendingCookieQueue::apply_failure(const PendingCookieEntry& entry,
    PendingCookieSink& sink)
{
  const int opped_roll = sink.random7();
  if (opped_roll == entry.index_hint && sink.can_kick(entry.chan, entry.opped_nick))
    sink.kick_opped(entry);

  const int opper_roll = sink.random7();
  if (opper_roll == 0) {
    if (sink.can_kick(entry.chan, entry.opper_nick))
      sink.kick_opper(entry);
    sink.deflag_opper(entry);
  }
}

bool LegacyPendingCookieQueue::add(const PendingCookieEntry& entry,
    PendingCookieSink& sink)
{
  if (entries_.size() >= PENDING_COOKIE_MAX)
    return false;

  entries_.push_back(entry);
  logf(sink, PendingCookieLog::Info,
      "Deferred cookie check for %s -> %s in %s (WHO sent)",
      entry.opper_nick, entry.opped_nick, entry.chan);
  sink.send_who(entry.opper_nick);
  sink.send_who(entry.opped_nick);
  return true;
}

void LegacyPendingCookieQueue::refresh(const char* chan, const char* nick,
    time_t now, PendingCookieSink& sink)
{
  (void) now;

  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    PendingCookieEntry& entry = entries_[i];

    if (rfc_casecmp(entry.chan, chan))
      continue;

    if (!entry.opper_refreshed && !rfc_casecmp(entry.opper_nick, nick))
      entry.opper_refreshed = true;
    else if (!entry.opped_refreshed && !rfc_casecmp(entry.opped_nick, nick))
      entry.opped_refreshed = true;
    else
      continue;

    if (!entry.opper_refreshed || !entry.opped_refreshed)
      continue;

    if (!sink.member_exists(entry.chan, entry.opper_nick) ||
        !sink.member_exists(entry.chan, entry.opped_nick)) {
      logf(sink, PendingCookieLog::Warning,
          "Pending cookie: %s or %s left %s, applying kick",
          entry.opper_nick, entry.opped_nick, entry.chan);
      entries_.erase(entries_.begin() + i);
      continue;
    }

    const int result = sink.check_cookie(entry);
    if (result == 0) {
      logf(sink, PendingCookieLog::Info,
          "Cookie retry succeeded for %s -> %s in %s",
          entry.opper_nick, entry.opped_nick, entry.chan);
    } else {
      logf(sink, PendingCookieLog::Warning,
          "Cookie retry failed for %s -> %s in %s (%d), applying kick",
          entry.opper_nick, entry.opped_nick, entry.chan, result);
      apply_failure(entry, sink);
    }

    entries_.erase(entries_.begin() + i);
  }
}

void LegacyPendingCookieQueue::expire(time_t now, PendingCookieSink& sink)
{
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    PendingCookieEntry& entry = entries_[i];

    if ((now - entry.timestamp) <= PENDING_COOKIE_TIMEOUT)
      continue;

    if (!sink.member_exists(entry.chan, entry.opper_nick) ||
        !sink.member_exists(entry.chan, entry.opped_nick)) {
      entries_.erase(entries_.begin() + i);
      continue;
    }

    const int result = sink.check_cookie(entry);
    if (result == 0) {
      logf(sink, PendingCookieLog::Info,
          "Pending cookie expired but valid for %s -> %s in %s",
          entry.opper_nick, entry.opped_nick, entry.chan);
    } else {
      logf(sink, PendingCookieLog::Warning,
          "Pending cookie expired for %s -> %s in %s (%d), applying kick",
          entry.opper_nick, entry.opped_nick, entry.chan, result);
      apply_failure(entry, sink);
    }

    entries_.erase(entries_.begin() + i);
  }
}

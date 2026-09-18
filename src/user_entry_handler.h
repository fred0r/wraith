#ifndef _USER_ENTRY_HANDLER_H
#define _USER_ENTRY_HANDLER_H

#include "users.h"
#include <bdlib/src/Stream.h>

namespace wraith {

class UserEntryHandler {
public:
  virtual ~UserEntryHandler() = default;

  virtual bool on_got_share(struct userrec *u, struct user_entry *e, char *data, int idx) = 0;
  virtual bool on_unpack(struct userrec *u, struct user_entry *e) = 0;
  virtual void on_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx) = 0;
  virtual bool on_kill(struct user_entry *e) = 0;
  virtual void *on_get(struct userrec *u, struct user_entry *e) = 0;
  virtual bool on_set(struct userrec *u, struct user_entry *e, void *buf) = 0;
  virtual void on_display(int idx, struct user_entry *e, struct userrec *u) = 0;
};

template <class Handler>
struct UserEntryTypeAdapter {
  static Handler& handler() {
    static Handler instance;
    return instance;
  }

  static bool got_share(struct userrec *u, struct user_entry *e, char *data, int idx) { return handler().on_got_share(u, e, data, idx); }
  static bool unpack(struct userrec *u, struct user_entry *e) { return handler().on_unpack(u, e); }
  static void write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx) { handler().on_write_userfile(stream, u, e, idx); }
  static bool kill(struct user_entry *e) { return handler().on_kill(e); }
  static void *get(struct userrec *u, struct user_entry *e) { return handler().on_get(u, e); }
  static bool set(struct userrec *u, struct user_entry *e, void *buf) { return handler().on_set(u, e, buf); }
  static void display(int idx, struct user_entry *e, struct userrec *u) { handler().on_display(idx, e, u); }
};

} /* namespace wraith */

#endif /* !_USER_ENTRY_HANDLER_H */

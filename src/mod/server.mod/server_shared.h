/*
 * server_shared.h -- internal to server.mod
 *
 * Pulls in the headers that servmsg.cc and cmdsserv.cc used to inherit
 * implicitly by being #included into server.cc, and declares the state
 * shared across the module's translation units.
 */

#ifndef _EGG_MOD_SERVER_SERVER_SHARED_H
#define _EGG_MOD_SERVER_SERVER_SHARED_H

#include "src/common.h"
#include "src/set.h"
#include "src/botmsg.h"
#include "src/rfc1459.h"
#include "src/settings.h"
#include "src/match.h"
#include "src/binds.h"
#include "src/users.h"
#include "src/userrec.h"
#include "src/main.h"
#include "src/response.h"
#include "src/misc.h"
#include "src/chanprog.h"
#include "src/net.h"
#include "src/auth.h"
#include "src/adns.h"
#include "src/socket.h"
#include "src/egg_timer.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "src/mod/irc.mod/irc.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>
#include "server.h"
#include <stdarg.h>
#include <vector>

// Ratbox is (5*8):30, ircd-seven is (5*8):20, try to not push the limits.
#define SERVER_CONNECT_BURST_TIME 18
#define SERVER_CONNECT_BURST_RATE 5 * 8

/* Global module state, promoted from server.cc so servmsg.cc and
 * cmdsserv.cc can use it as separate translation units.
 */
extern bind_table_t *BT_raw, *BT_msg;
extern bool use_monitor;
extern char serverpass[121];
extern time_t trying_server;
extern int nick_juped, jnick_juped;
extern bool waiting_for_awake;
extern interval_t server_timeout;
extern const interval_t stoned_timeout;
extern bool resolvserv;
extern time_t lastpingtime;
extern char stackablecmds[511], stackable2cmds[511];
extern egg_timeval_t last_time;
extern int real_msgburst, real_msgrate;
extern bool use_flood_count, use_penalties;
extern int use_fastdeq;
extern bool replaying_cache, double_warned;

class MessageQueue : public msgq_head {
public:
  void clear();
  bool empty() const { return head == NULL; }
  size_t count() const { return tot; }
  void enqueue(const char *buf, int len, bool front);
};

static_assert(sizeof(MessageQueue) == sizeof(msgq_head), "MessageQueue must not change msgq_head layout");

extern MessageQueue mq, hq, modeq, aq;

void empty_msgq(void);
void end_burstmode(void);
void connect_server(void);
void disconnect_server(int idx);
void server_activity(int idx, char *msg, int len);
void nick_available(bool is_jupe, bool is_orig);
extern cmd_t my_raw_binds[];
extern cmd_t C_dcc_serv[];

#endif /* !_EGG_MOD_SERVER_SERVER_SHARED_H */

/*
 * irc_shared.h -- internal to irc.mod
 *
 * Pulls in the headers that chan.cc, mode.cc, cmdsirc.cc and msgcmds.cc
 * used to inherit implicitly by being #included into irc.cc, activates
 * the MAKING_IRC declarations in irc.h, and declares the state/functions
 * shared across the module's translation units.
 */

#ifndef _EGG_MOD_IRC_IRC_SHARED_H
#define _EGG_MOD_IRC_IRC_SHARED_H

#define MAKING_IRC

#include "src/common.h"
#include "irc.h"
#include "src/adns.h"
#include "src/match.h"
#include "src/settings.h"
#include "src/base64.h"
#include "src/tandem.h"
#include "src/net.h"
#include "src/botnet.h"
#include "src/botmsg.h"
#include "src/main.h"
#include "src/response.h"
#include "src/set.h"
#include "src/userrec.h"
#include "src/misc.h"
#include "src/rfc1459.h"
#include "src/socket.h"
#include "src/chanprog.h"
#include "src/auth.h"
#include "src/userent.h"
#include "src/binds.h"
#include "src/egg_timer.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include <algorithm>
using std::swap;
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/base64.h>
#include <deque>
#include <vector>
#include <stdarg.h>
#include <math.h>

/* Global module state, promoted so the module's translation units share it. */
extern bool reversing;
extern struct flag_record irc_user, irc_victim;
extern bool bounce_bans, bounce_exempts, bounce_invites, bounce_modes;
extern size_t mode_buf_len;
extern bool kick_fun, ban_fun, prevent_mixing;
extern std::deque<bd::String> chained_who;
extern int chained_who_idx;
extern cmd_t irc_raw[], C_msg[], irc_dcc[];

/* Cross-file functions that irc.h's MAKING_IRC block does not declare. */
int voice_ok(memberlist *m, struct chanset_t *chan);
void refresh_exempt(struct chanset_t *chan, char *user);
void kick_all(struct chanset_t *chan, char *hostmask, const char *comment, int bantype);
void enforce_bans(struct chanset_t *chan);
void enforce_bitch(struct chanset_t *chan, bool flush = 1);
void do_take(struct chanset_t *chan);
void check_exemptlist(struct chanset_t *chan, const char *from);
void resetmasks(struct chanset_t *chan, masklist *m, maskrec *mrec, maskrec *global_masks, char mode);
void logc(const char *cmd, Auth *auth, char *chname, char *par);
void mass_request(const char *botnick, const char *code, char *par);

#define LOGC(cmd) logc(cmd, a, chname, par)

#endif /* !_EGG_MOD_IRC_IRC_SHARED_H */

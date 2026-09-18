/*
 * channels_shared.h -- internal to channels.mod
 *
 * Pulls in the headers that cmdschan.cc, chanmisc.cc and userchan.cc used
 * to inherit implicitly by being #included into channels.cc, and declares
 * the state shared across the module's translation units.
 */

#ifndef _EGG_MOD_CHANNELS_CHANNELS_SHARED_H
#define _EGG_MOD_CHANNELS_CHANNELS_SHARED_H

#include "src/common.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"
#include "src/chanprog.h"
#include "src/egg_timer.h"
#include "src/misc.h"
#include "src/main.h"
#include "src/color.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/set.h"
#include "src/rfc1459.h"
#include "src/match.h"
#include "src/settings.h"
#include "src/tandem.h"
#include "src/botnet.h"
#include "src/botmsg.h"
#include "src/net.h"
#include "src/binds.h"
#include "src/cmds.h"
#include "src/mod/console.mod/console.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>
#include <sys/stat.h>
#include <ctype.h>

#include "channels.h"

#define PLSMNS(x) (x ? '+' : '-')

/* Global module state, promoted from channels.cc/userchan.cc so the
 * module's translation units can share it.
 */
extern bool use_info;
extern char glob_chanmode[64];
extern interval_t global_ban_time, global_exempt_time, global_invite_time;
extern char *lastdeletedmask;

extern cmd_t C_dcc_channels[];

int count_mask(maskrec *rec);
void tell_masks(const char type, int idx, bool show_inact, char *match, bool all = 0);
void check_expired_masks(void);

class Channel : public chanset_t {
public:
  void set_mode_protect(char *set);
  void get_mode_protect(char *s, size_t ssiz);
};

static_assert(sizeof(Channel) == sizeof(chanset_t), "Channel must not change chanset_t layout");

#endif /* !_EGG_MOD_CHANNELS_CHANNELS_SHARED_H */

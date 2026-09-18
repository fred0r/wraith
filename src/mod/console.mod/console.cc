/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * console.c -- part of console.mod
 *   saved console settings
 *   by cmwagner/billyjoe/D. Senso
 *
 */


#include "console.h"
#include "src/common.h"
#include "src/mod/share.mod/share.h"
#include "src/binds.h"
#include "src/tandem.h"
#include "src/cmds.h"
#include "src/users.h"
#include "src/userent.h"
#include "src/botmsg.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/misc.h"
#include "src/core_binds.h"
#include "src/user_entry_handler.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

class ConsoleInfo {
public:
  static ConsoleInfo *parse(char *par)
  {
    ConsoleInfo *ci = (ConsoleInfo *) calloc(1, sizeof(ConsoleInfo));
    char *arg = NULL;

    arg = newsplit(&par);
    ci->channel = strdup(arg);
    arg = newsplit(&par);
    ci->conflags = logmodes(arg);
    arg = newsplit(&par);
    ci->stripflags = stripmodes(arg);
    arg = newsplit(&par);
    ci->echoflags = (arg[0] == '1') ? 1 : 0;
    arg = newsplit(&par);
    ci->page = atoi(arg);
    arg = newsplit(&par);
    ci->conchan = atoi(arg);
    arg = newsplit(&par);
    ci->color = atoi(arg);
    arg = newsplit(&par);
    ci->banner = atoi(arg);
    arg = newsplit(&par);
    ci->channels = atoi(arg);
    arg = newsplit(&par);
    ci->bots = atoi(arg);
    arg = newsplit(&par);
    ci->whom = atoi(arg);
    arg = newsplit(&par);
    ci->tz_offset = atoi(arg);

    return ci;
  }

  static ConsoleInfo *from_chat(struct chat_info *chat, unsigned long status, int channel)
  {
    ConsoleInfo *ci = (ConsoleInfo *) calloc(1, sizeof(ConsoleInfo));

    ci->channel = strdup(chat->con_chan);
    ci->conflags = chat->con_flags;
    ci->stripflags = chat->strip_flags;
    ci->echoflags = (status & STAT_ECHO) ? 1 : 0;
    ci->page = (status & STAT_PAGE) ? chat->max_line : 0;
    ci->color = (status & STAT_COLOR) ? 1 : 0;
    ci->banner = (status & STAT_BANNER) ? 1 : 0;
    ci->channels = (status & STAT_CHANNELS) ? 1 : 0;
    ci->bots = (status & STAT_BOTS) ? 1 : 0;
    ci->whom = (status & STAT_WHOM) ? 1 : 0;
    ci->tz_offset = chat->tz_offset;
    ci->conchan = channel;

    return ci;
  }

  void apply_to(struct chat_info *chat, unsigned long &status) const
  {
    if (channel && channel[0])
      strlcpy(chat->con_chan, channel, sizeof(chat->con_chan));
    chat->con_flags = conflags;
    chat->strip_flags = stripflags;
    if (echoflags)
      status |= STAT_ECHO;
    else
      status &= ~STAT_ECHO;
    if (page) {
      status |= STAT_PAGE;
      chat->max_line = page;
      if (!chat->line_count)
        chat->current_lines = 0;
    }
    if (color)
      status |= STAT_COLOR;
    else
      status &= ~STAT_COLOR;
    if (banner)
      status |= STAT_BANNER;
    else
      status &= ~STAT_BANNER;
    if (channels)
      status |= STAT_CHANNELS;
    else
      status &= ~STAT_CHANNELS;
    if (bots)
      status |= STAT_BOTS;
    else
      status &= ~STAT_BOTS;
    if (whom)
      status |= STAT_WHOM;
    else
      status &= ~STAT_WHOM;
    chat->tz_offset = tz_offset;
  }

  bd::String to_string() const
  {
    return bd::String::printf("%s %s %s %d %d %d %d %d %d %d %d %d",
                 channel, masktype(conflags),
                 stripmasktype(stripflags), echoflags,
                 page, conchan, color, banner, channels, bots, whom,
                 tz_offset);
  }

  void destroy()
  {
    free(channel);
    free(this);
  }

  const char *get_channel() const { return channel; }
  int get_conflags() const { return conflags; }
  int get_stripflags() const { return stripflags; }
  int get_echoflags() const { return echoflags; }
  int get_page() const { return page; }
  int get_conchan() const { return conchan; }
  int get_color() const { return color; }
  int get_banner() const { return banner; }
  int get_channels() const { return channels; }
  int get_bots() const { return bots; }
  int get_whom() const { return whom; }
  int get_tz_offset() const { return tz_offset; }

private:
  char *channel;
  int conflags;
  int stripflags;
  int echoflags;
  int page;
  int conchan;
  int color;
  int banner;
  int channels;
  int bots;
  int whom;
  int tz_offset;
};

class ConsoleEntry : public wraith::UserEntryHandler {
public:
  bool
  on_unpack(struct userrec *u, struct user_entry *e) override
  {
  char *par = e->u.list->extra;

  ConsoleInfo *ci = ConsoleInfo::parse(par);
  list_type_kill(e->u.list);
  e->u.extra = ci;
  return 1;
  }

  bool
  on_kill(struct user_entry *e) override
  {
  ConsoleInfo *i = (ConsoleInfo *) e->u.extra;

  i->destroy();
  free(e);
  return 1;
  }

  void
  on_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx) override
  {
  if (u->bot)
    return;

  ConsoleInfo *i = (ConsoleInfo *) e->u.extra;

  stream << bd::String::printf("--CONSOLE %s\n", i->to_string().c_str());
  }

  bool
  on_set(struct userrec *u, struct user_entry *e, void *buf) override
  {
  ConsoleInfo *ci = (ConsoleInfo *) e->u.extra;

  if (!ci && !buf)
    return 1;

  if (ci != buf) {
    if (ci)
      ci->destroy();
    ci = (ConsoleInfo *) buf;
    e->u.extra = (ConsoleInfo *) buf;
  }

  if (!noshare && !u->bot)
    shareout("c CONSOLE %s %s\n", u->handle, ci->to_string().c_str());
  return 1;
  }

  bool
  on_got_share(struct userrec *u, struct user_entry *e, char *par, int idx) override
  {
  ConsoleInfo *ci = ConsoleInfo::parse(par);

  if (e->u.extra)
    ((ConsoleInfo *) e->u.extra)->destroy();
  e->u.extra = ci;

  /* now let's propogate to the dcc list */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_CHAT) && !strcmp(dcc[i].user->handle, u->handle))
      ci->apply_to(dcc[i].u.chat, dcc[i].status);
  }
  return 1;
  }

  void
  on_display(int idx, struct user_entry *e, struct userrec *u) override
  {
  ConsoleInfo *i = (ConsoleInfo *) e->u.extra;

  if (dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    dprintf(idx, "  %s\n", "Saved Console Settings:");
    dprintf(idx, "    %s %s\n", "Channel:", i->get_channel());
    dprintf(idx, "    %s %s, %s %s, %s %s\n", "Console flags:",
            masktype(i->get_conflags()), "",
            stripmasktype(i->get_stripflags()), "Echo:", i->get_echoflags() ? "yes" : "no");
    dprintf(idx, "    %s %d, %s %s%d\n", "Page setting:", i->get_page(),
            "Console channel:", (i->get_conchan() < GLOBAL_CHANS) ? "" : "*", i->get_conchan() % GLOBAL_CHANS);
    dprintf(idx, "    Color: $b%s$b\n", i->get_color() ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner:   $b%-3s$b   Bots: $b%-3s$b\n", i->get_banner() ? "on" : "off", i->get_bots() ? "on" : "off");
    dprintf(idx, "     Channels: $b%-3s$b   Whom: $b%-3s$b\n", i->get_channels() ? "on" : "off", i->get_whom() ? "on" : "off");
    if (i->get_tz_offset())
      dprintf(idx, "    Timezone: $b%s$b\n", tz_format(i->get_tz_offset()));
  }
  }

  void *
  on_get(struct userrec *u, struct user_entry *e) override
  {
    return def_get(u, e);
  }
};

struct user_entry_type USERENTRY_CONSOLE = {
  0,                            /* always 0 ;) */
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::got_share,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::unpack,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::write_userfile,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::kill,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::get,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::set,
  &wraith::UserEntryTypeAdapter<ConsoleEntry>::display,
  "CONSOLE"
};

static int
console_chon(char *handle, int idx)
{
  if (dcc[idx].type == &DCC_CHAT) {
    ConsoleInfo *i = (ConsoleInfo *) get_user(&USERENTRY_CONSOLE, dcc[idx].user);

    if (i)
      i->apply_to(dcc[idx].u.chat, dcc[idx].status);
    if ((dcc[idx].u.chat->channel >= 0) && (dcc[idx].u.chat->channel < GLOBAL_CHANS)) {
      botnet_send_join_idx(idx);
    }
  }
  return 0;
}

static int
console_store(int idx, char *par, bool displaySave)
{
  ConsoleInfo *i = ConsoleInfo::from_chat(dcc[idx].u.chat, dcc[idx].status, dcc[idx].u.chat->channel);

  if (par) {
    dprintf(idx, "%s\n", "Saved your Console Settings:");
    dprintf(idx, "  %s %s\n", "Channel:", i->get_channel());
    dprintf(idx, "  %s %s, %s %s, %s %s\n", "Console flags:",
            masktype(i->get_conflags()), "",
            stripmasktype(i->get_stripflags()), "Echo:", i->get_echoflags() ? "yes" : "no");
    dprintf(idx, "  %s %d, %s %d\n", "Page setting:", i->get_page(), "Console channel:", i->get_conchan());
    dprintf(idx, "    Color: $b%s$b\n", i->get_color() ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner:   $b%-3s$b   Bots: $b%-3s$b\n", i->get_banner() ? "on" : "off", i->get_bots() ? "on" : "off");
    dprintf(idx, "     Channels: $b%-3s$b   Whom: $b%-3s$b\n", i->get_channels() ? "on" : "off", i->get_whom() ? "on" : "off");
    if (i->get_tz_offset())
      dprintf(idx, "    Timezone: $b%s$b\n", tz_format(i->get_tz_offset()));

  }
  set_user(&USERENTRY_CONSOLE, dcc[idx].user, i);
  dprintf(idx, "Console setting stored.\n");
  if (conf.bot->hub)
    write_userfile(displaySave ? idx : -1);
  return 0;
}

/* cmds.c:cmd_console calls this, better than chof bind - drummer,07/25/1999 */
void
console_dostore(int idx, bool displaySave)
{
  console_store(idx, NULL, displaySave);
  return;
}

static cmd_t mychon[] = {
  {"*", "", (Function) console_chon, "console:chon", 0},
  {NULL, NULL, NULL, NULL, 0}
};

static cmd_t mydcc[] = {
  {"store", "", (Function) console_store, NULL, 0},
  {NULL, NULL, NULL, NULL, 0}
};

void
ConsoleModule::init()
{
  add_builtins("dcc", mydcc);
  add_builtins("chon", mychon);

  add_entry_type(&USERENTRY_CONSOLE);
}
/* vim: set sts=2 sw=2 ts=8 et: */

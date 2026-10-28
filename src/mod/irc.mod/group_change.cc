#include "group_change.h"

#include "irc_shared.h"
#include "invite_shard.h"

#include <string>
#include <vector>

namespace {

void append_groups(std::vector<std::string>& out, const char *value)
{
  if (!value)
    return;

  const std::string groups(value);
  size_t start = 0;

  while (start < groups.length()) {
    size_t end = groups.find_first_of(", \t\r\n", start);

    if (end == std::string::npos)
      end = groups.length();

    std::string group = groups.substr(start, end - start);
    size_t first = group.find_first_not_of(" \t\r\n");
    size_t last = group.find_last_not_of(" \t\r\n");

    if (first != std::string::npos)
      out.push_back(group.substr(first, last - first + 1));

    start = end + 1;
  }
}

std::vector<std::string> user_groups(struct userrec *u)
{
  std::vector<std::string> result;

  if (!u)
    return result;

  append_groups(result, u == conf.bot->u ? groups : var_get_bot_data(u, "groups", true));
  return result;
}

std::vector<std::string> channel_groups(const struct chanset_t *chan)
{
  std::vector<std::string> result;

  if (!chan || !chan->groups)
    return result;

  for (size_t i = 0; i < chan->groups->length(); ++i)
    result.push_back((*chan->groups)[i].get().c_str());
  return result;
}

std::vector<invite_shard::Bot> channel_inviters(struct chanset_t *chan)
{
  std::vector<invite_shard::Bot> result;

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->split || !chan_hasop(m) || chan_sentdeop(m))
      continue;

    struct userrec *u = m->is_me ? conf.bot->u : (member_getuser(m) ? m->user : NULL);

    if (!u || !is_bot(u))
      continue;

    invite_shard::Bot bot;
    bot.handle = m->is_me ? conf.bot->nick : m->nick;
    bot.groups = user_groups(u);
    result.push_back(bot);
  }

  return result;
}

std::vector<invite_shard::Bot> channel_invite_targets(struct chanset_t *chan)
{
  std::vector<invite_shard::Bot> result;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_BOT, 0, 0, 0 };

  for (tand_t *b = tandbot; b; b = b->next) {
    if (b->hub || !b->u || ismember(chan, b->bot))
      continue;

    get_user_flagrec(b->u, &fr, chan->dname, chan);
    if (!bot_shouldjoin(b->u, &fr, chan) || !chk_op(fr, chan))
      continue;

    invite_shard::Bot target;
    target.handle = b->bot;
    target.groups = user_groups(b->u);
    result.push_back(target);
  }

  return result;
}

void op_new_members(struct chanset_t *chan)
{
  if (!me_op(chan) || !group_change::is_op_owner(chan))
    return;

  struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->is_me || m->split || chan_hasop(m) || chan_sentop(m) ||
        !member_getuser(m) || !is_bot(m->user))
      continue;

    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if (chk_op(fr, chan) && bot_shouldjoin(m->user, &fr, chan))
      do_op(m, chan, 0, 0);
  }

  flush_mode(chan, QUICK);
}

void send_go(struct chanset_t *chan)
{
  if (chan->channel.groupchange.op_sent)
    return;

  chan->channel.groupchange.op_sent = now;

  char go_msg[120];
  simple_snprintf(go_msg, sizeof(go_msg), "go %s", chan->dname);
  putallbots(go_msg);
}

}

namespace group_change {

void begin(struct chanset_t *chan)
{
  if (!chan)
    return;

  chan->channel.groupchange.invite_started = now;
  chan->channel.groupchange.op_started = now;
  chan->channel.groupchange.op_owner[0] = 0;
}

void finish(struct chanset_t *chan)
{
  if (!chan || !chan->channel.groupchange.pending)
    return;

  chan->channel.groupchange.pending = false;
  chan->channel.parttime = 0;
  chan->channel.groupchange.op_sent = 0;
}

bool op_window_active(struct chanset_t *chan)
{
  return chan && chan->channel.groupchange.op_window_active(now);
}

bool is_op_owner(struct chanset_t *chan)
{
  if (!chan || !me_op(chan) || !op_window_active(chan))
    return false;

  if (chan->channel.groupchange.op_owner[0]) {
    memberlist *owner = ismember(chan, chan->channel.groupchange.op_owner);

    if (owner && !owner->split && chan_hasop(owner) && member_getuser(owner) &&
        is_bot(owner->user) && in_chain(owner->user->handle))
      return !rfc_casecmp(chan->channel.groupchange.op_owner, conf.bot->nick);

    chan->channel.groupchange.op_owner[0] = 0;
  }

  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_BOT, 0, 0, 0 };
  const char *owner = NULL;
  const char *staying_owner = NULL;

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->split || !chan_hasop(m) || !member_getuser(m) || !is_bot(m->user))
      continue;

    if (!in_chain(m->user->handle))
      continue;

    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if (!chk_op(fr, chan))
      continue;

    if (!owner || rfc_casecmp(m->user->handle, owner) < 0)
      owner = m->user->handle;

    if (bot_shouldjoin(m->user, &fr, chan) &&
        (!staying_owner || rfc_casecmp(m->user->handle, staying_owner) < 0))
      staying_owner = m->user->handle;
  }

  if (staying_owner)
    owner = staying_owner;
  if (!owner)
    return false;

  strlcpy(chan->channel.groupchange.op_owner, owner,
      sizeof(chan->channel.groupchange.op_owner));
  return !rfc_casecmp(owner, conf.bot->nick);
}

bool replacement_opped(struct chanset_t *chan)
{
  if (!chan)
    return false;

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->is_me || m->split || !chan_hasop(m) || !member_getuser(m) ||
        !is_bot(m->user))
      continue;

    struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };
    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if (chk_op(fr, chan) && bot_shouldjoin(m->user, &fr, chan))
      return true;
  }

  return false;
}

void invite_peers(struct chanset_t *chan, bool sharded, bool force)
{
  if (!chan || !me_op(chan) || !(chan->channel.mode & CHANINV))
    return;

  if (!force && (now - chan->channel.groupchange.invite_pulled) < 30)
    return;

  if (!sharded) {
    struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_BOT, 0, 0, 0 };
    bd::String pull;
    int missing = 0;

    for (tand_t *b = tandbot; b; b = b->next) {
      if (b->hub || !b->u || ismember(chan, b->bot))
        continue;

      get_user_flagrec(b->u, &fr, chan->dname, chan);
      if (!bot_shouldjoin(b->u, &fr, chan) || !chk_op(fr, chan))
        continue;

      ++missing;
      if (!pull.length())
        pull = bd::String::printf("gi r %s", chan->dname);

      dprintf(DP_MODE, "INVITE %s %s\n", b->bot, chan->name[0] ? chan->name : chan->dname);
    }

    if (missing) {
      putallbots(pull.c_str());
      chan->channel.groupchange.invite_pulled = now;
      putlog(LOG_GETIN, "*", "Pulled %d bot(s) into invite-only %s", missing, chan->dname);
    }
    return;
  }

  const std::vector<std::string> groups = channel_groups(chan);
  const std::vector<invite_shard::Bot> inviters = channel_inviters(chan);
  const std::vector<invite_shard::Bot> targets = channel_invite_targets(chan);
  const std::vector<invite_shard::Assignment> assignments =
      invite_shard::assign(groups, inviters, targets);
  int invited = 0;

  for (const invite_shard::Assignment& assignment : assignments) {
    if (rfc_casecmp(assignment.inviter.c_str(), conf.bot->nick))
      continue;
    if (ismember(chan, assignment.target.c_str()))
      continue;

    dprintf(DP_MODE, "INVITE %s %s\n", assignment.target.c_str(),
        chan->name[0] ? chan->name : chan->dname);
    ++invited;
  }

  if (invited) {
    chan->channel.groupchange.invite_pulled = now;
    putlog(LOG_GETIN, "*", "Invited %d bot(s) into invite-only %s", invited, chan->dname);
  }
}

bool handle_leave(struct chanset_t *chan, bool from_group_change)
{
  if (!chan)
    return false;

  if (!(from_group_change || chan->channel.groupchange.pending))
    return false;

  if (!(channel_active(chan) || channel_pending(chan)) || shouldjoin(chan))
    return false;

  if (!chan->channel.groupchange.pending) {
    putlog(LOG_WARNING, "*",
        "Group change for %s: waiting for an opped replacement bot",
        chan->dname);
  }
  chan->channel.groupchange.pending = true;

  if (replacement_opped(chan)) {
    send_go(chan);
    chan->channel.parttime = now;
    schedule_groupchange_check(chan, 1);
    return true;
  }

  op_new_members(chan);
  send_go(chan);
  chan->channel.parttime = 0;
  schedule_groupchange_check(chan, 10);
  return true;
}

}

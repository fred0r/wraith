#ifndef _EGG_MOD_IRC_GROUP_CHANGE_H
#define _EGG_MOD_IRC_GROUP_CHANGE_H

struct chanset_t;

namespace group_change {

void begin(struct chanset_t *chan);
void finish(struct chanset_t *chan);
bool op_window_active(struct chanset_t *chan);
bool is_op_owner(struct chanset_t *chan);
bool replacement_opped(struct chanset_t *chan);
void invite_peers(struct chanset_t *chan, bool sharded, bool force = false);
bool handle_leave(struct chanset_t *chan, bool from_group_change);

}

#endif

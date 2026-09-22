#ifndef _USERENT_H
#define _USERENT_H

#include "users.h"

void update_mod(const char *, const char *, const char *, const char *);
void list_type_kill(struct list_type *);
void stats_add(struct userrec *, int, int);
void init_userent(void);

/*
 * Set before a userfile stream is built: `peer_allowed` says whether the
 * destination may receive SECPASS/auth-key (hub, +c chathub, or a localhub
 * that must forward them). The on-disk `.u` (hub) always allows.
 */
void userfile_write_context(bool peer_allowed);
/* True when the current userfile stream targets a peer that may hold SECPASS/
 * auth-key. Valid only while a stream is being built. */
bool userfile_peer_allowed(void);
/* True when connection idx may receive SECPASS/auth-key: a hub, a +c chathub,
 * or a localhub that has to forward them to +c bots below it. */
bool peer_allowed_auth_values(int idx);

#endif /* !_USERENT_H */

/*
 * server.h -- part of server.mod
 *
 */

#ifndef _EGG_MOD_SERVER_SERVER_H
#define _EGG_MOD_SERVER_SERVER_H

#include "src/binds.h"
#include "src/dcc.h"
#include "src/set.h"
#include "module.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>

class ServerModule : public wraith::Module {
public:
  void init() override;
  const char *name() const override { return "server"; }
};

#define DEQ_RATE 200

char *fixcolon(char *x);
void write_to_server(const char *buf, size_t len);

namespace bd {
  class Stream;
}

struct server_list {
  struct server_list	*next;
  char			*name;
  char			*pass;
  in_port_t		 port;
  bool			 ssl;
};

class ServerList : public server_list {
public:
  static void destroy(server_list *head);
};

static_assert(sizeof(ServerList) == sizeof(server_list), "ServerList must not change server_list layout");

/* Available net types.  */
enum {
	NETT_EFNET		= 0,	/* EfNet except new +e/+I hybrid. */
	NETT_IRCNET		= 1,	/* Ircnet.			  */
	NETT_UNDERNET		= 2,	/* Undernet.			  */
	NETT_DALNET		= 3,	/* Dalnet.			  */
	NETT_HYBRID_EFNET	= 4	/* new +e/+I Efnet hybrid.	  */
};

typedef struct {
  bd::String sharedKey;
  bd::String myPrivateKey;
  bd::String myPublicKeyB64;
  time_t key_created_at;
  bool use_cbc;
} fish_data_t;

extern bind_table_t	*BT_ctcp, *BT_ctcr;
extern size_t		nick_len;
extern bool		trigger_on_ignore, floodless, keepnick, in_deaf, in_callerid, have_cprivmsg, have_cnotice;
extern int 		servidx, ctcp_mode, answer_ctcp, serv, curserv, default_alines, flood_count, burst;
extern unsigned int     rolls;
extern in_port_t		default_port, default_port_ssl, newserverport, curservport;
extern time_t		server_online, tried_jupenick, tried_nick, release_time, connect_bursting;
extern interval_t	cycle_time;
extern char		server_ipver[];
extern int		server_using_ssl;
extern int		ssl_use;

/* Peers with a lower numver only understand server-use-ssl as a 0/1 bool
 * (1.4.x parses it with VAR_BOOL and would clamp 2 -> 1 = SSL-only), so the
 * mode-2 value must never be delivered to them. */
#define SSL_MODE2_MIN_NUMVER 1005000

int effective_ssl_use();
int ssl_compat_value(int mode);
void send_ssl_to_child(int idx);
void distribute_ssl_var(int except = -1, int old_ssl_use = -1);
extern char		cursrvname[], botrealname[121], botuserhost[UHOSTLEN], ctcp_reply[1024],
			newserver[121], newserverpass[121], curnetwork[], botuserip[], altnick_char, deaf_char, callerid_char;
extern ServerList *serverlist;
extern struct dcc_table SERVER_SOCKET;
extern rate_t		flood_msg, flood_ctcp, flood_callerid;
extern bd::HashTable<bd::String, fish_data_t*> FishKeys;

int check_bind_ctcpr(const char *, const char *, struct userrec *, const char *, const char *, const char *, bind_table_t *);
void nicks_available(const char* buf, char delim = 0, bool buf_contains_available = 1);
void release_nick(const char* = NULL);

#define check_bind_ctcp(a, b, c, d, e, f) check_bind_ctcpr(a, b, c, d, e, f, BT_ctcp)
#define check_bind_ctcr(a, b, c, d, e, f) check_bind_ctcpr(a, b, c, d, e, f, BT_ctcr)

bool detect_avalanche(const char *) __attribute__((pure));
void server_report(int, int);
void server_init();
void queue_server(int, char *, int);
void server_die();
void add_server(char *, bool ssl = false);
void clearq(struct server_list *);
void nuke_server(const char *);
bool match_my_nick(const char *);
void rehash_server(const char *, const char *);
void rehash_monitor_list();
void replay_cache(int, bd::Stream*);
void join_chans();
void check_hostmask();
void next_server(int *, char *, in_port_t *, char *);
void server_send_ison();
void reset_flood();

#endif		/* _EGG_MOD_SERVER_SERVER_H */

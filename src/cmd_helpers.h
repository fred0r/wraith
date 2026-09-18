#ifndef _CMD_HELPERS_H
#define _CMD_HELPERS_H

#include <cstddef>
#include "cmds.h"

/* Type-safe cmd_t construction.
 *
 * Replaces (Function) casts with compile-time checks.
 * The actual cast to Function still happens internally -- the bind
 * system dispatches via HashFunc (void* args) and casts back.
 *
 * Usage:
 *   static const cmd_t my_cmds[] = {
 *     make_cmd("help", "-|-", cmd_help, 0),
 *     make_cmd("about", "", cmd_about, "cmd_about", 0),
 *     CMD_END
 *   };
 */
template<typename Func>
cmd_t make_cmd(const char *name, const char *flags, Func func, int type)
{
	cmd_t entry;
	entry.name = name;
	entry.flags = flags;
	entry.func = (Function)(func);
	entry.funcname = NULL;
	entry.type = type;
	return entry;
}

template<typename Func>
cmd_t make_cmd(const char *name, const char *flags, Func func,
	       const char *funcname, int type)
{
	cmd_t entry;
	entry.name = name;
	entry.flags = flags;
	entry.func = (Function)(func);
	entry.funcname = funcname;
	entry.type = type;
	return entry;
}

#define CMD_END { NULL, NULL, NULL, NULL, 0 }

#endif /* !_CMD_HELPERS_H */

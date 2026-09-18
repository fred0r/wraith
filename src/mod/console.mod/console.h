/*
 * console.h -- part of console.mod
 *
 */

#ifndef _EGG_MOD_CONSOLE_CONSOLE_H
#define _EGG_MOD_CONSOLE_CONSOLE_H

#include "module.h"

class ConsoleModule : public wraith::Module {
public:
  void init() override;
  const char *name() const override { return "console"; }
};

void console_dostore(int, bool = 1);
extern struct user_entry_type USERENTRY_CONSOLE;

#endif				/* _EGG_MOD_CONSOLE_CONSOLE_H */

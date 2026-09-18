/*
 * ctcp.h -- part of ctcp.mod
 *   all the defines for ctcp.c
 *
 */

#ifndef _EGG_MOD_CTCP_CTCP_H
#define _EGG_MOD_CTCP_CTCP_H

#include "module.h"

#include <string>

#define CLOAK_COUNT             11 /* The number of scripts currently existing */
#define CLOAK_PLAIN             1 /* This is your plain bitchx client behaviour */
#define CLOAK_CRACKROCK         2
#define CLOAK_NEONAPPLE         3
#define CLOAK_TUNNELVISION      4
#define CLOAK_ARGON             5
#define CLOAK_EVOLVER           6
#define CLOAK_PREVAIL           7
#define CLOAK_CYPRESS           8 /* Now with full theme and customization support */
#define CLOAK_MIRC              9
#define CLOAK_OTHER             10

class CtcpModule : public wraith::Module {
public:
  void init() override;
  const char *name() const override { return "ctcp"; }

  static void script_changed();
  static const char *kickprefix();
  static const char *bankickprefix();

private:
  static std::string cloak_bxver_, cloak_os_, cloak_osver_, cloak_host_;
  static std::string kickprefix_, bankickprefix_;
};

extern int		first_ctcp_check;

#endif				/* _EGG_MOD_CTCP_CTCP_H */

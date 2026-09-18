/*
 * update.h -- part of update.mod
 *
 */

#ifndef _EGG_MOD_update_update_H
#define _EGG_MOD_update_update_H

#include "module.h"

class UpdateModule : public wraith::Module {
public:
  void init() override;
  const char *name() const override { return "update"; }

  static bool is_bupdating();
  static void set_bupdating(bool value);
  static bool is_updated();
  static void set_updated(bool value);
  static int tick();
  static void reset_tick();

private:
  static bool bupdating_;
  static bool updated_;
  static int cnt_;
};

void finish_update(int);
void update_report(int, int);
void updatein(int, char *);

#endif				/* _EGG_MOD_update_update_H */

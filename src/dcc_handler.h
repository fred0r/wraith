#ifndef _DCC_HANDLER_H
#define _DCC_HANDLER_H

#include <cstddef>
#include "types.h"

struct dcc_table;

namespace wraith {

class DccHandler {
public:
	virtual ~DccHandler() = default;

	virtual void on_eof(int idx) = 0;
	virtual void on_activity(int idx, char *buf, int len) = 0;
	virtual void on_display(int idx, char *buf, size_t bufsiz) = 0;

	virtual void on_timeout(int idx) {}
	virtual void on_kill(int idx, void *u_other) {}
	virtual void on_output(int idx, char *buf, void *u_other) {}
	virtual void on_outdone(int idx) {}

	interval_t *timeout_val() const { return timeout_val_; }
	void set_timeout_val(interval_t *v) { timeout_val_ = v; }

private:
	interval_t *timeout_val_ = nullptr;
};

template <class Handler>
struct DccTableAdapter {
	static Handler& handler() {
		static Handler instance;
		return instance;
	}

	static void eof(int idx) { handler().on_eof(idx); }
	static void activity(int idx, char *buf, int len) { handler().on_activity(idx, buf, len); }
	static void timeout(int idx) { handler().on_timeout(idx); }
	static void display(int idx, char *buf, size_t bufsiz) { handler().on_display(idx, buf, bufsiz); }
	static void kill(int idx, void *u_other) { handler().on_kill(idx, u_other); }
	static void output(int idx, char *buf, void *u_other) { handler().on_output(idx, buf, u_other); }
	static void outdone(int idx) { handler().on_outdone(idx); }
};

} /* namespace wraith */

#endif /* !_DCC_HANDLER_H */

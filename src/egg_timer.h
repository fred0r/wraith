#ifndef _EGG_TIMER_H_
#define _EGG_TIMER_H_

#include "types.h"
#include <string>

typedef struct egg_timeval_b {
	long sec;
	long usec;
} egg_timeval_t;

#define TIMER_REPEAT 1

class Timer {
public:
	Timer(const char* name, int id, void* callback, void* client_data, int flags);
	~Timer();
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	const std::string& name() const { return name_; }
	int id() const { return id_; }
	void* callback() const { return callback_; }
	void* client_data() const { return client_data_; }
	int flags() const { return flags_; }
	bool repeats() const { return (flags_ & TIMER_REPEAT) != 0; }
	int called() const { return called_; }
	const egg_timeval_t& howlong() const { return howlong_; }
	const egg_timeval_t& trigger_time() const { return trigger_time_; }

	void set_trigger_time(long sec, long usec) {
		trigger_time_.sec = sec;
		trigger_time_.usec = usec;
	}
	void inc_called() { ++called_; }
	void clear_name() { name_.clear(); }

	void update_trigger(long now_sec, long now_usec) {
		trigger_time_.sec = now_sec + howlong_.sec;
		trigger_time_.usec = now_usec + howlong_.usec;
		if (trigger_time_.usec >= 1000000) {
			trigger_time_.usec -= 1000000;
			++trigger_time_.sec;
		}
	}

	void set_howlong(long sec, long usec) {
		howlong_.sec = sec;
		howlong_.usec = usec;
	}

private:
	std::string name_;
	int id_;
	void* callback_;
	void* client_data_;
	egg_timeval_t howlong_;
	egg_timeval_t trigger_time_;
	int flags_;
	int called_;
};

/* Create a simple timer with no client data and no flags. */
#define timer_create(howlong,name,callback) timer_create_complex(howlong, name, callback, NULL, 0)

/* Create a simple timer with no client data, but it repeats. */
#define timer_create_repeater(howlong,name,callback) timer_create_complex(howlong, name, callback, NULL, TIMER_REPEAT)

void timer_get_now(egg_timeval_t *_now);
int timer_get_now_sec(int *sec);
void timer_update_now(egg_timeval_t *_now);
int timer_diff(egg_timeval_t *from_time, egg_timeval_t *to_time, egg_timeval_t *diff);
long timeval_diff(const egg_timeval_t *tv1, const egg_timeval_t *tv2)
  __attribute__((pure));
int timer_create_secs(int, const char *, Function);
int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, void *client_data, int flags);
int timer_destroy(int timer_id);
#ifdef not_used
int timer_destroy_all();
#endif
int timer_get_shortest(egg_timeval_t *howlong);
void timer_run();
int timer_list(int **ids);
int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called);
#endif /* _EGG_TIMER_H_ */

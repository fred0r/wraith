/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include <stdio.h> /* For NULL */
#include <sys/time.h> /* For gettimeofday() */
#include <list>
#include <memory>
#include "common.h"

#include "egg_timer.h"

/* Timer class implementation */
Timer::Timer(const char* name, int id, void* callback, void* client_data, int flags)
	: id_(id), callback_(callback), client_data_(client_data), flags_(flags), called_(0)
{
	if (name) name_ = name;
	memset(&howlong_, 0, sizeof(howlong_));
	memset(&trigger_time_, 0, sizeof(trigger_time_));
}

Timer::~Timer() {
}

/* From main.c */
static egg_timeval_t now;

/* Internal timer lists */
static std::list<std::unique_ptr<Timer>> timer_repeat_list;
static std::list<std::unique_ptr<Timer>> timer_once_list;
static int timer_next_id = 1;

/* Based on TclpGetTime from Tcl 8.3.3 */
static inline int timer_get_time(egg_timeval_t *curtime)
{
	static struct timeval tv;

	(void) gettimeofday(&tv, NULL);
	curtime->sec = tv.tv_sec;
	curtime->usec = tv.tv_usec;
	return(0);
}

void timer_update_now(egg_timeval_t *_now)
{
	timer_get_time(&now);
	if (_now) timer_get_now(_now);
}

void timer_get_now(egg_timeval_t *_now)
{
	_now->sec = now.sec;
	_now->usec = now.usec;
}

int timer_get_now_sec(int *sec)
{
	if (sec) *sec = now.sec;
	return(now.sec);
}


/* Find difference between two timers. */
int timer_diff(egg_timeval_t *from_time, egg_timeval_t *to_time, egg_timeval_t *diff)
{
	diff->sec = to_time->sec - from_time->sec;
	if (diff->sec < 0) {
		diff->sec = 0;
		diff->usec = 0;
		return(1);
	}

	diff->usec = to_time->usec - from_time->usec;

	if (diff->usec < 0) {
		if (diff->sec == 0) {
			diff->usec = 0;
			return(1);
		}
		--(diff->sec);
		diff->usec += 1000000;
	}

	return(0);
}

/*
 * Return milliseconds difference between two timevals
 */
long timeval_diff(const egg_timeval_t *tv1, const egg_timeval_t *tv2)
{
	long secs = tv1->sec - tv2->sec, usecs = tv1->usec - tv2->usec;
	if (usecs < 0) {
		usecs += 1000000;
		--secs;
	}
	usecs = (usecs / 1000) + (secs * 1000);

	return usecs;
}

static void timer_add_to_list(std::list<std::unique_ptr<Timer>>& timer_list, std::unique_ptr<Timer> timer)
{
	auto it = timer_list.begin();
	while (it != timer_list.end()) {
		if (timer->trigger_time().sec < (*it)->trigger_time().sec ||
		    (timer->trigger_time().sec == (*it)->trigger_time().sec &&
		     timer->trigger_time().usec < (*it)->trigger_time().usec)) {
			break;
		}
		++it;
	}
	timer_list.insert(it, std::move(timer));
}

int timer_create_secs(int secs, const char *name, Function callback)
{
	egg_timeval_t howlong;

	howlong.sec = secs;
	howlong.usec = 0;

	return timer_create_repeater(&howlong, name, callback);
}

int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, void *client_data, int flags)
{
	int id = timer_next_id++;
	auto timer = std::make_unique<Timer>(name, id, (void*)callback, client_data, flags);
	timer->set_howlong(howlong->sec, howlong->usec);
	timer->update_trigger(now.sec, now.usec);

	if (timer->repeats())
		timer_add_to_list(timer_repeat_list, std::move(timer));
	else
		timer_add_to_list(timer_once_list, std::move(timer));

	return id;
}

static bool timer_destroy_list(std::list<std::unique_ptr<Timer>>& timer_list, int timer_id)
{
	for (auto it = timer_list.begin(); it != timer_list.end(); ++it) {
		if ((*it)->id() == timer_id) {
			timer_list.erase(it);
			return true;
		}
	}
	return false;
}

/* Destroy a timer, given an id. */
int timer_destroy(int timer_id)
{
	if (timer_destroy_list(timer_repeat_list, timer_id))
		return 0;
	if (timer_destroy_list(timer_once_list, timer_id))
		return 0;
	return 1;
}

#ifdef not_used
int timer_destroy_all()
{
	timer_repeat_list.clear();
	timer_once_list.clear();
	return(0);
}
#endif

int timer_get_shortest(egg_timeval_t *howlong)
{
	if (timer_repeat_list.empty()) return(1);

	auto& timer = timer_repeat_list.front();
	timer_diff(&now, (egg_timeval_t*)&timer->trigger_time(), howlong);
	return(0);
}

static bool process_timer(Timer& timer) {
	auto callback = (int(*)(void*))timer.callback();
	void *client_data = timer.client_data();
	bool deleted = false;

	if (timer.repeats()) {
		timer.update_trigger(now.sec, now.usec);
		timer.inc_called();
	} else {
		deleted = true;
	}

	if (!timer.name().empty())
		ContextNote("Timer", timer.name().c_str());

	callback(client_data);
	return deleted;
}

static void process_timer_list(std::list<std::unique_ptr<Timer>>& timer_list) {
	auto it = timer_list.begin();
	while (it != timer_list.end()) {
		auto& timer = *it;
		if (timer->trigger_time().sec > now.sec ||
		    (timer->trigger_time().sec == now.sec && timer->trigger_time().usec > now.usec))
			break;
		auto next = std::next(it);
		if (process_timer(*timer)) {
			it = timer_list.erase(it);
		} else {
			it = next;
		}
	}
}

void timer_run()
{
	process_timer_list(timer_once_list);
	process_timer_list(timer_repeat_list);
}

int timer_list(int **ids)
{
	int ntimers = 0;

	ntimers = static_cast<int>(timer_repeat_list.size());

	/* Fill in array. */
	*ids = (int *) calloc(1, sizeof(int) * (ntimers+1));
	ntimers = 0;
	for (auto& timer : timer_repeat_list) {
		(*ids)[ntimers++] = timer->id();
	}
	return(ntimers);
}

int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called)
{
	for (auto& t : timer_repeat_list) {
		if (t->id() == id) {
			if (name) *name = (char*)t->name().c_str();
			if (initial_len) memcpy(initial_len, &t->howlong(), sizeof(*initial_len));
			if (trigger_time) memcpy(trigger_time, &t->trigger_time(), sizeof(*trigger_time));
			if (called) *called = t->called();
			return 0;
		}
	}
	return(-1);
}

/* vim: set sts=0 sw=8 ts=8 noet: */

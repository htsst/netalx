/* ---------------------------------------------------------------------- *
 * This is part of NETAL.
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
 *
 * NETAL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * NETAL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NETAL; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "ulibc.h"

/* -----------------------------------------------------------------
 * double get_msecs(void)
 * unsigned long long get_usecs(void)
 * ----------------------------------------------------------------- */
double get_msecs(void) {
  struct timeval tv;
  assert( !gettimeofday(&tv, NULL) );
  return (double)tv.tv_sec*1e3 + (double)tv.tv_usec*1e-3;
}

unsigned long long get_usecs(void) {
  struct timeval tv;
  assert( !gettimeofday(&tv, NULL) );
  return (unsigned long long)tv.tv_sec*1000000 + tv.tv_usec;
}


/* -----------------------------------------------------------------
 * sync_counter(long long *counter, long long x)
 * ----------------------------------------------------------------- */
int sync_counter(long long *counter, long long x) {
  __sync_fetch_and_add(counter, -1);
  return __sync_bool_compare_and_swap(counter, 0, x);
}


/* -----------------------------------------------------------------
 * double tm_start(struct ivtimer_t *tm)
 * double tm_stop(struct ivtimer_t *tm)
 * double tm_time(struct ivtimer_t *tm)
 * double tm_elapsed(struct ivtimer_t *tm)
 * int tm_interval(struct ivtimer_t *tm, double delta, double bound)
 * ----------------------------------------------------------------- */
double tm_start(struct ivtimer_t *tm) {
  double t = get_msecs();
  if (tm) {
    tm->start = t;
    tm->prev = tm->start;
    tm->stop = 0;
  }
  return t;
}

double tm_stop(struct ivtimer_t *tm) {
  double t = get_msecs();
  if (tm) {
    tm->stop = t;
  }
  return t;
}

double tm_time(struct ivtimer_t *tm) {
  if (tm) {
    return tm->stop - tm->start;
  } else {
    return 0.0;
  }
}

double tm_elapsed(struct ivtimer_t *tm) {
  if (tm) {
    return get_msecs() - tm->start;
  } else {
    return get_msecs();
  }
}

int tm_interval(struct ivtimer_t *tm, double delta, double bound) {
  int r = 0;
  double t = get_msecs();
  if (t - tm->prev > delta) {
    r = 1;
    tm->prev = t;
  }
  if (0.0 < bound && bound < t - tm->start) {
    r = -1;
  }
  return r;
}



/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

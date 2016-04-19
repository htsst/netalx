/* ---------------------------------------------------------------------- *
 *
 * This is part of NETAL.
 *
 * Copyright (C) 2011-2015 Yuichiro Yasui
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
 * along with NETAL; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include "ulibc.h"

int main(int argc, char **argv) {
  long long i, ites = 10e8;
  double ivms = 1e2;
  double bdms = 2e3;

  printf("usage: %s interval(ms) bound(ms)\n", *argv);
  if (argc > 1) {
    --argc, ++argv;
    ivms = atof(*argv);
  }
  if (argc > 1) {
    --argc, ++argv;
    bdms = atof(*argv);
  }
  printf("#ites    is %e\n", (double)ites);
  printf("interval is %.1f ms\n", ivms);
  printf("bound    is %.1f ms\n", bdms);

  struct ivtimer_t tm;
  double sum = 0.0;

  tm_start(&tm);
  for (i = 0; i < ites; ++i) {
    sum += i;
    int t = tm_interval(&tm, ivms, bdms);
    if (t) {
      printf("sum = %12e, i = %12e, %.2f loop/s, elapsed: %7.2f s\n",
	     sum, (double)i, i/tm_elapsed(&tm), tm_elapsed(&tm));
    }
    if (t < 0) break;
  }
  tm_stop(&tm);
  printf("sum is %e (%f ms)\n", sum, tm_time(&tm));
  return 0;
}

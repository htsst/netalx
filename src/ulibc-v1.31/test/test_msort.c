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
#include <string.h>
#include <assert.h>
#include <time.h>
#include "ulibc.h"

int compar(const void *a, const void *b) {
  long _a, _b;
  _a = *(long *)a;
  _b = *(long *)b;
  return  _a - _b;
}

int main(int argc, char **argv) {
  long i, cs, len = 100, *a;
  double t1, t2;

  if (argc > 1) {
    len = atol(argv[1]);
  }
  setbuf(stdout, NULL);

  printf("len  is %ld\n", len);
  printf("\n");

  /* random sequence */
  printf("generate %ld elements (%.2f MBytes)\n",
	 len, (double)sizeof(long)*len/(1<<20));
  srand(time(NULL));
  a = malloc(sizeof(long)*len);
  cs = 0;
  srand(time(NULL));
  for (i = 0; i < len; ++i) {
    a[i] = -100 + rand() % (200);
    printf("%ld ", a[i]);
    cs += a[i];
  }
  printf("\n");
  printf("checksum is %ld\n", cs);
  printf("\n");


  /* msort */
  t1 = get_msecs();
  printf("msort() ... ");
  msort(a, len, sizeof(long), compar);
  t2 = get_msecs();
  printf("(%.2f msec.)\n", t2-t1);
  printf("\n");

  printf("verification\n");
  for (i = 0; i < len; ++i) {
    printf("%ld ", a[i]);
    cs -= a[i];
  }
  printf("\n");
  printf("checksum diff is %ld\n", cs);
  printf("\n");


  /* uniq */
  long nr = 0;
  t1 = get_msecs();
  printf("uniq() ... ");
  nr = uniq(a, len, sizeof(long), qsort, compar);
  t2 = get_msecs();
  printf("(%.2f msec.)\n", t2-t1);
  printf("\n");
  printf("verification(%ld elements)\n", nr);
  for (i = 0; i < nr; ++i) {
    printf("%ld ", a[i]);
  }
  printf("\n");

  free(a);

  return 0;
}

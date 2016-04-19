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

  extern char *basename(const char *string);
  extern char *mstrcat(char *first, ...);
  extern void error(const char *format, ... );


int main(int argc, char **argv) {
  unsigned long long n;
  int i, k;

  printf("[TEST] bit2num()/num2bit()\n");
  srand(time(NULL));
  for (k = 0; k < 2; ++k) {
    n = 0;
    n |= (unsigned long long int)(rand() & 0xffffffff)<<32;
    n |= (unsigned long long int)(rand() & 0xffffffff)<<0;
    printf("num2bit(%20llu, 1) : %s\n", n, num2bit(n, 1));
    printf("num2bit(%20llu, 0) : %s\n", n, num2bit(n, 0));
    printf("bit2num(num2bit(%20llu, 1)) : %llu\n", n, bit2num(num2bit(n, 1)));
    printf("bit2num(num2bit(%20llu, 0)) : %llu\n", n, bit2num(num2bit(n, 0)));
    assert(n == bit2num(num2bit(n, 1)));
    assert(n == bit2num(num2bit(n, 0)));
  }
  printf("\n");

  printf("[TEST] basename()\n");
  for (i = 0; i < argc; ++i) {
    printf("[%2d] basename(%s) is %s\n", i, argv[i], basename(argv[i]));
  }
  printf("\n");

  printf("[TEST] mstrcat()\n");
  char string[100] = "[gfdgfd]";
  char *res = mstrcat(string, "+1231", "-sadasd", NULL);
  printf("%s\n", res);
  printf("\n");

  printf("[TEST] cl_fprintf()\n");
  for (i = 0; i < UCL_COLORS; ++i) {
    cl_fprintf(i, stdout, "[color]\t");
  }
  printf("\n");
  fflush(stdout);

  printf("[TEST] error()\n");
  error("cannot allocate memory: %lu bytes\n", 12345UL);
  return 0;
}

/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

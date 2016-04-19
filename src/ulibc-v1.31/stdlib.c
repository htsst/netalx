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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "ulibc.h"

int intlog(int base, unsigned long long int n) {
  int log_n=0;
  unsigned long long int crr=1;
  do {
    ++log_n;
    crr *= base;
  } while (crr < n);
  return log_n;
}

long long int intsqrt(long long int n) {
  int sqrt_n;
  for (sqrt_n = 0; sqrt_n * sqrt_n < n; ++sqrt_n) ;
  return sqrt_n;
}

double fsqrt(const double x) {
  double xh = 0.5 * x;
  union {
    long long int i;
    double f;
  } xx;
  xx.f = x;
  xx.i = 0x5FE6EB50C7B537AALL - ( *(long long int*)&xx.i >> 1);
  double xr = * (double *)&xx.f;
  xr *= ( 1.5 - ( xh * xr * xr ) );
  xr *= ( 1.5 - ( xh * xr * xr ) );
  xr *= ( 1.5 - ( xh * xr * xr ) );
  return xr * x;
}

void print_prbar(double pcent, char *prefix, char symb, char lf) {
  const int n = 30;
  char buf[128];
  int i, j = (int)(pcent * n);
  for (i = 0; i < j; ++i) {
    buf[i] = symb;
  }
  for (; i < n; ++i) {
    buf[i] = ' ';
  }
  buf[i] = '\0';
  if (prefix) {
    printf("%s\t%s [%3.f%%]%c", prefix, buf, pcent * 100, lf);
  } else {
    printf("\t%s [%3.f%%]%c", buf, pcent * 100, lf);
  }
}

char *set_nsymb(char *s, char symb, int n) {
  int i;
  if (s) {
    for (i = 0; i < n; ++i) {
      s[i] = symb;
    }
    s[i] = '\0';
  }
  return s;
}

long *rand_perm(long n, long *seq) {
  /* 'ulibc' -- ascii --> 1171081059899UL -- sqrt() --> 1082165 */
  const long int ULIBC_MAGIC = 1082165UL;
  long i, j, k, log2_n=0;
  if (seq) {
    for (log2_n=0; log2_n*log2_n < n; ++log2_n) ; /* log2(n) */
    srand48(log2_n * ULIBC_MAGIC);

    for (i = 0; i < n; i++) {
      j = lrand48() % n;
      if (i == j) continue;
      k = seq[i]; seq[i] = seq[j]; seq[j] = k;
    }
  }
  return seq;
}

double *normalize(long n, double *X, double l, double h) {
  long j;
  double minX, maxX;
  if (X && (h-l) > 0.0 && n > 0) {
    minX = maxX = X[0];
    for (j = 1; j < n; ++j) {
      if (minX > X[j]) minX = X[j];
      if (maxX < X[j]) maxX = X[j];
    }
    for (j = 0; j < n; ++j) {
      X[j] = l + (X[j] - minX) * (h-l) / (maxX-minX);
    }
  }
  return X;
}

/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

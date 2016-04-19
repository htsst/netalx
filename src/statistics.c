/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version modified by The GraphCREST Project
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
 * Copyright (C) 2015 The GraphCREST Project, Tokyo Institute of Technology
 *
 * NETALX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * 2015/08/18 Chaned a header to include para_bfs_csr.h.
 * ------------------------------------------------------------------------ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "generation.h"
#include "construction.h"
#include "para_bfs_csr.h"
#include "validation.h"
#include "log.h"

int f64cmp(const void *a, const void *b) {
  const double _a = *(const double*)a;
  const double _b = *(const double*)b;
  if (_a  > _b) return  1;
  if (_b  > _a) return -1;
  if (_a == _b) return  0;
  return 0;
}

void statistics(double *stats, double *datas, I64_t n) {
  long double s, mean;
  double t;
  int k;

  qsort(datas, n, sizeof(double), f64cmp);

  /* 0: min */
  stats[0] = datas[0];

  /* 1: firstquartile */
  t = (n+1) / 4.0;
  k = (int) t;
  if (t == k) {
    stats[1] = datas[k];
  } else {
    stats[1] = 3*(datas[k]/4.0) + datas[k+1]/4.0;
  }

  /* 2: median */
  t = (n+1) / 2.0;
  k = (int) t;
  if (t == k) {
    stats[2] = datas[k];
  } else {
    stats[2] = datas[k]/2.0 + datas[k+1]/2.0;
  }

  /* 3: thirdquartile */
  t = 3*((n+1) / 4.0);
  k = (int) t;
  if (t == k) {
    stats[3] = datas[k];
  } else {
    stats[3] = datas[k]/4.0 + 3*(datas[k+1]/4.0);
  }

  /* 4: max */
  stats[4] = datas[n-1];

  /* 5: mean */
  s = datas[n-1];
  for (k = n-1; k > 0; --k) {
    s += datas[k-1];
  }
  mean = s/n;
  stats[5] = mean;

  /* 6:stddev */
  s = datas[n-1] - mean;
  s *= s;
  for (k = n-1; k > 0; --k) {
    long double tmp = datas[k-1] - mean;
    s += tmp * tmp;
  }
  stats[6] = sqrt (s/(n-1));

  /* 7: harmonic_mean */
  s = datas[0] ? 1.0L/datas[0] : 0;
  for (k = 1; k < n; ++k) {
    s += (datas[k]? 1.0L/datas[k] : 0);
  }
  stats[7] = n/s;
  mean = s/n;

  /*
    Nilan Norris, The Standard Errors of the Geometric and Harmonic
    Means and Their Application to Index Numbers, 1940.
    http://www.jstor.org/stable/2235723
  */
  /* 8: harmonic_stddev */
  s = (datas[0]? 1.0L/datas[0] : 0) - mean;
  s *= s;
  for (k = 1; k < n; ++k) {
    long double tmp = (datas[k]? 1.0L/datas[k] : 0) - mean;
    s += tmp * tmp;
  }
  s = (sqrt (s)/(n-1)) * stats[7] * stats[7];
  stats[8] = s;
}

#define NSTAT 9
#define PRINT_STATS(msg, label, israte)					\
  do {									\
    strcatfmt(msg, "min_%s: %20.17e\n", label, stats[0]);		\
    strcatfmt(msg, "firstquartile_%s: %20.17e\n", label, stats[1]);	\
    strcatfmt(msg, "median_%s: %20.17e\n", label, stats[2]);		\
    strcatfmt(msg, "thirdquartile_%s: %20.17e\n", label, stats[3]);	\
    strcatfmt(msg, "max_%s: %20.17e\n", label, stats[4]);		\
    if (!israte) {							\
      strcatfmt(msg, "mean_%s: %20.17e\n", label, stats[5]);		\
      strcatfmt(msg, "stddev_%s: %20.17e\n", label, stats[6]);		\
    } else {								\
      strcatfmt(msg, "harmonic_mean_%s: %20.17e\n", label, stats[7]);	\
      strcatfmt(msg, "harmonic_stddev_%s: %20.17e\n", label, stats[8]); \
    }									\
  } while (0)

int   statVERBOSE = 1;
double    minTEPS = 0.0;
double     fqTEPS = 0.0;
double medianTEPS = 0.0;
double     tqTEPS = 0.0;
double    maxTEPS = 0.0;

void output_graph500_results(struct stat_t *stat,
			     const I64_t SCALE, I64_t nvtx_scale, I64_t edgefactor,
			     const double A, const double B, const double C, const double D,
			     const double generation_time, const double construction_time,
			     const int NBFS) {
  int k;
  I64_t sz;
  double *tm;
  double *stats;

  tm = (double *)alloca(NBFS * sizeof (*tm));
  stats = (double *)alloca(NSTAT * sizeof (*stats));
  if (!tm || !stats) {
    perror("Error allocating within final statistics calculation.");
    abort();
  }

  char *results = (char *)calloc(1<<15, sizeof(char));
  strcatfmt(results, "%s", message);

  sz = (1L << SCALE) * edgefactor * 2 * sizeof (I64_t);
  strcatfmt(results,
	    "SCALE: %ld\n"
	    "nvtx: %ld\nedgefactor: %ld\n"
	    "terasize: %20.17e\n",
	    (long)SCALE, (long)nvtx_scale, (long)edgefactor, sz/1.0e12);
  strcatfmt(results, "A: %20.17e\nB: %20.17e\nC: %20.17e\nD: %20.17e\n", A, B, C, D);
  strcatfmt(results, "generation_time: %20.17e\n", generation_time);
  strcatfmt(results, "construction_time: %20.17e\n", construction_time);
  strcatfmt(results, "nbfs: %d\n", NBFS);

  for (k = 0; k < NBFS; ++k) {
    tm[k] = stat[k].bfs_time;
  }
  statistics(stats, tm, NBFS);
  PRINT_STATS(results, "time", 0);

  for (k = 0; k < NBFS; ++k) {
    tm[k] = stat[k].trav_edges;
  }
  statistics(stats, tm, NBFS);
  PRINT_STATS(results, "nedge", 0);

  for (k = 0; k < NBFS; ++k) {
    tm[k] = (double)stat[k].trav_edges / stat[k].bfs_time;
  }
  statistics(stats, tm, NBFS);
  PRINT_STATS(results, "TEPS", 1);

  minTEPS    = stats[0];
  fqTEPS     = stats[1];
  medianTEPS = stats[2];
  tqTEPS     = stats[3];
  maxTEPS    = stats[4];

  if (statVERBOSE) {
    fprintf(stdout, "%s", results);
    fprintf(logout, "%s", results);
  }
  free(results);
}

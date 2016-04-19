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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#if defined(_OPENMP)
#  define OMP(x) _Pragma(x)
#  include <omp.h>
#else
#  define OMP(x)
static int  omp_get_thread_num (void) { return 0; }
static int  omp_get_num_threads(void) { return 1; }
#endif

#include "ulibc.h"


int main(int argc, char **argv) {
  void test_omp_with_affinity(int threads, int use_htcores, int strategy);
  int threads = 1, use_htcores = 1, strategy = 0;;

  OMP("omp parallel") {
    if ( !omp_get_thread_num() )
      threads = omp_get_num_threads();
  }

  while (1) {
    int ch = getopt(argc, argv, "p:ns:vh?");
    if (ch == -1) break;
    switch (ch) {
    case 'p' : threads = atoi(optarg);  break;
    case 'n' : use_htcores = 0;         break;
    case 's' : strategy = atoi(optarg); break;
    case 'h' :
    case 'v' :
    default  :
      printf("usage: %s [-p NUM; #hreads] [-n ; disable HT] [-s NUM; 0:core_major, 1:node_major]\n",
	     *argv);
    return 1;
    }
  }

  if ( enable_affinity() ) {
    printf("affinity is enable.\n");
  } else {
    printf("affinity is disable.\n");
  }
  printf("\n");

  printf("#procs   is %d\n", get_numa_nprocs());
  printf("#nodes   is %d\n", get_numa_nnodes());
  printf("#cores   is %d\n", get_numa_ncores());
  printf("#physical cores is %d\n", get_numa_physical_ncores());
  printf("\n");

  test_omp_with_affinity(threads, use_htcores, strategy);

  return 0;
}



void test_omp_with_affinity(int threads, int use_htcores, int strategy) {
  int i, j;

  set_numa_mapping_strategy(strategy);

  if ( !use_htcores ) {
    printf("disable hyper threading\n");
    disable_hyper_threading_cores();
  }


  set_numa_num_threads(threads);

  printf("#online nodes is %d\n", get_numa_online_nodes());
  for (i = 0; i < get_numa_online_nodes(); ++i) {
    printf("[node%02d] (#online cores is %d)\t", i, get_numa_online_cores(i));
    for (j = 0; j < get_numa_num_threads(); ++j) {
      if (i == get_numa_nodeid(j)) {
	printf("%2d ", j);
      }
    }
    printf("\n");
  }
  print_mapping(stdout);
  printf("\n");

  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int np = omp_get_num_threads();
    int id = omp_get_thread_num();
    int procid = get_numa_procid(id);
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_coreid(id);
    int vircoreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    set_affinity(procid);
    if ( enable_affinity() ) {
      assert( get_numa_apicid(id) == get_apicid() );
    }
    printf("[%2d/%d], node%2d (core=%2d)"
	   ", procid is %2d, apicid is %3d, coreid is %2d, vircoreid is %2d\n",
	   id+1, np, nodeid, lcores, procid, get_apicid(), coreid, vircoreid);
    OMP("omp barrier");
    clear_affinity();
  }
}

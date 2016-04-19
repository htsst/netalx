/* Copyright (C) 2009-2010 The Trustees of Indiana University.             */
/*                                                                         */
/* Use, modification and distribution is subject to the Boost Software     */
/* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at */
/* http://www.boost.org/LICENSE_1_0.txt)                                   */
/*                                                                         */
/*  Authors: Jeremiah Willcock                                             */
/*           Andrew Lumsdaine                                              */

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdio.h>
#include <omp.h>

#include "make_graph.h"

int make_kron_edgelist(int scale, int edgefactor) {
  return 0;
}


uint64_t userseed;
uint_fast32_t prng_seed[5];
static mrg_state prng_state_store;
void *prng_state = &prng_state_store;

/* Spread the two 64-bit numbers into five nonzero values in the correct range. */
static void make_mrg_seed(uint64_t userseed, uint_fast32_t* seed) {
  seed[0] = (userseed & 0x3FFFFFFF) + 1;
  seed[1] = ((userseed >> 30) & 0x3FFFFFFF) + 1;
  seed[2] = (userseed & 0x3FFFFFFF) + 1;
  seed[3] = ((userseed >> 30) & 0x3FFFFFFF) + 1;
  seed[4] = ((userseed >> 60) << 4) + (userseed >> 60) + 1;
}

void init_random(void) {
  long seed = -1;
  if (getenv ("SEED")) {
    errno = 0;
    seed = strtol (getenv ("SEED"), NULL, 10);
    if (errno) seed = -1;
  }
  if (seed < 0) seed = 0xDECAFBAD;
  userseed = seed;
  make_mrg_seed (seed, prng_seed);
  mrg_seed(&prng_state_store, prng_seed);
}


int main(int argc, char* argv[]) {
  int log2_n;
  double start, time_taken;
  int64_t actual_nedges;
  packed_edge* result;

  log2_n = 16; /* In base 2 */
  if (argc >= 2) log2_n = atoi(argv[1]);
  printf("log2_n is %d\n", log2_n);
  
  /* if (seed < 0) seed = 0xDECAFBAD; */
  printf("seed = 0xDECAFBAD is %ld\n", 0xDECAFBADL);
  
  int edgefactor = 16;
  int64_t num_nodes = (1ULL << log2_n);
  int64_t num_edges = num_nodes * edgefactor;
  
  printf("n m edgefactor is %lld %lld %d\n", num_nodes, num_edges, edgefactor);
  
  start = omp_get_wtime();
  make_graph(log2_n, num_edges, 1, 2, &actual_nedges, &result);
  time_taken = omp_get_wtime() - start;
  
  fprintf(stderr, "%" PRIu64 " edge%s generated in %fs (%f Medges/s)\n", actual_nedges, (actual_nedges == 1 ? "" : "s"), time_taken, 1. * actual_nedges / time_taken * 1.e-6);

  free(result);

  return 0;
}

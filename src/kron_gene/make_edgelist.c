/* -*- mode: C; mode: folding; fill-column: 70; -*- */
/* Copyright 2010,  Georgia Institute of Technology, USA. */
/* See COPYING for license. */
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <alloca.h> /* Portable enough... */
#include <unistd.h>
#include <math.h>

#include "splittable_mrg.h"
#include "graph_generator.h"
#include "make_graph.h"
#include "utils.h"
#include "ulibc.h"
#include "defs.h"
#include "prng.h"
#include "utest.h"

/* --------------------------------------------------------------------------- */
/* graph 500 edgelist generator                                                */
/* --------------------------------------------------------------------------- */
static void partial_edge_range(I64_t size, I64_t offset, I64_t np, I64_t id,
			       I64_t *loop_start, I64_t *loop_end) {
  const I64_t qt = size / np;
  const I64_t rm = size - qt * np;
  *loop_start = qt * (id+0) + (id+0 < rm ? id+0 : rm) + offset;
  *loop_end   = qt * (id+1) + (id+1 < rm ? id+1 : rm) + offset;
}

I64_t make_edgelist(int scale, int edgefactor,
		    int num_lists, struct packed_edge **edge_list,
		    I64_t *nbfs_ptr, I64_t *bfs_root_ptr) {
  int64_t N = 1LL << scale, M = N * edgefactor;
  int64_t actual_edges = 0;
  
  /* printf("#lists is %d, #threads is %d\n", num_lists, get_numa_num_threads()); */
  
  /* Catch a few possible overflows. */
  assert(M >= N);
  assert(M >= edgefactor);
  
  /* Add restrict to input pointers. */
  /* Spread the two 64-bit numbers into five nonzero values in the correct range. */
  init_random();
  uint_fast32_t seed[5];
  make_mrg_seed(userseed, userseed, seed);

  double t1, t2;
  
  t1 = get_msecs() * 1e-3;
  printf("generating kronecker edge list ...\n");
  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:actual_edges)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    I64_t node_start, node_end, thread_start, thread_end;
    partial_edge_range(M, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, 0, lcores, coreid, &thread_start, &thread_end);
    actual_edges += generate_kronecker_edges(id == 0, seed, scale,
					     thread_start+node_start, thread_end+node_start,
					     &edge_list[nodeid][thread_start]);
    clear_affinity();
  }
  t2 = get_msecs() * 1e-3;
  printf("done. (generated %ld edges) (%.2f seconds)\n", (long)actual_edges, t2-t1);
  assert(actual_edges == M);
  
  int *has_adj = NULL;
  assert( has_adj = (int *)malloc(N * sizeof(int)) );
  
  OMP("omp parallel") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");
    
    I64_t node_start, node_end, thread_start, thread_end;
    partial_edge_range(N, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, node_start, lcores, coreid, &thread_start, &thread_end);
    /* printf("[%d] V[%8lld:%8lld] -> [%8lld:%8lld]\n", */
    /* 	   nodeid, node_start, node_end, thread_start, thread_end); */
    I64_t k;
    for (k = thread_start; k < thread_end; ++k) {
      has_adj[k] = 0;
    }
    OMP("omp barrier");

    partial_edge_range(actual_edges, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, node_start, lcores, coreid, &thread_start, &thread_end);
    /* printf("[%d] E[%8lld:%8lld] -> [%8lld:%8lld] %p -> [%d/%d] E[%d][%8lld:%8lld] %p\n", */
    /* 	   nodeid, node_start, node_end, */
    /* 	   thread_start, thread_end, */
    /* 	   edge_list[nodeid], */
    /* 	   coreid, lcores, nodeid, thread_start-node_start, thread_end-node_start, */
    /* 	   &edge_list[nodeid][thread_start - node_start]); */
    thread_start -= node_start;
    thread_end   -= node_start;
    for (k = thread_start; k < thread_end; ++k) {
      const int64_t i = get_v0_from_edge(&edge_list[nodeid][k]);
      const int64_t j = get_v1_from_edge(&edge_list[nodeid][k]);
      if (i != j) {
	has_adj[i] = has_adj[j] = 1;
      }
    }
    clear_affinity();
  }
  
  /* Sample from {0, ..., N-1} without replacement. */
  int64_t NBFS = NBFS_max;
  int64_t k = 0, v = 0;
  double R;
  while (k < NBFS && v < N) {
    R = (N - v) * mrg_get_double_orig(prng_state);
    if ( !has_adj[v] || R > NBFS - k ) {
      ++v;
    } else {
      bfs_root_ptr[k++] = v++;
    }
  }
  free(has_adj);

  /* verification */
  if ( N <= v && k < NBFS ) {
    if (0 < k) {
      fprintf(stderr, "Cannot find %lld sample roots of non-self degree > 0, using %lld.\n",
	      (long long)NBFS, (long long)k);
      NBFS = k;
    } else {
      fprintf(stderr, "Cannot find any sample roots of non-self degree > 0.\n");
      exit(EXIT_FAILURE);
    }
  }
  
  *nbfs_ptr = NBFS;
  
  return actual_edges;
}

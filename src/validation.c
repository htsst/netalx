/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version modified by The GraphCREST Project
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
 * Copyright (C) 2013-2015 The GraphCREST Project, Tokyo Institute of Technology
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


int valiVERBOSE = 1;

/* ------------------------------------------------------------ *
 * merge_local_bfs_tree
 * ------------------------------------------------------------ */
static void merge_local_bfs_tree(struct graph_t *G, struct bfs_t *BFS) {
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    int k;
    I64_t j, ls, le;
    struct bfs_local_t *LBFS = &BFS->bfs_local[nodeid];
    partial_range(LBFS->range, LBFS->offset, lcores, coreid, &ls, &le);
    I64_t *tree = LBFS->tree;

    for (k = 0; k < G->num_graphs-1; ++k) {
      I64_t *target = BFS->bfs_local[ (nodeid+k+1) % G->num_graphs ].tree;
      for (j = ls; j < le; ++j) {
        target[j] = tree[j];
      }
      OMP("omp barrier");
    }
    clear_affinity();
  }
}


static int compute_levels(int *hops, I64_t nv, I64_t root, struct bfs_t *BFS) {
  int err = 0;
  long sum_of_errors  = 0;
  long num_error_cycles = 0;	    /* Cycle. err:-1 */
  long num_error_notfound_root = 0; /* Ran off the end. err:-2 */

  OMP("omp parallel num_threads(get_numa_num_threads()) \
      reduction(+:num_error_cycles,num_error_notfound_root)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    pinned(USE_HYBRID_AFFINITY, id);
    const I64_t *bfs_tree = BFS->bfs_local[nodeid].tree;
    OMP("omp barrier");

    I64_t k;

    OMP("omp for schedule(guided)")
    for (k = 0; k < nv; ++k) {
      hops[k] = (k == root? 0 : -1);
    }

    OMP("omp for schedule(guided)")
    for (k = 0; k < nv; ++k) {
      if (hops[k] >= 0) continue;

      if (bfs_tree[k] >= 0 && k != root) {
        I64_t parent = k;
        I64_t nhop = 0;

        /* Run up the tree until we encounter an already-leveled vertex. */
        while (parent >= 0 && hops[parent] < 0 && nhop < nv) {
          parent = bfs_tree[parent];
          ++nhop;
        }
        if (nhop >= nv) ++num_error_cycles;
        if (parent < 0) ++num_error_notfound_root;

        if ( num_error_cycles )
          __sync_fetch_and_add(&sum_of_errors, num_error_cycles);
        if ( num_error_notfound_root )
          __sync_fetch_and_add(&sum_of_errors, num_error_notfound_root);

        if ( !num_error_cycles ) {
          /* Now assign levels until we meet an already-leveled vertex */
          /* NOTE: This permits benign races if parallelized. */
          nhop += hops[parent];
          parent = k;
          while (hops[parent] < 0) {
            hops[parent] = nhop--;
            parent = bfs_tree[parent];
          }
        }
      }
    }
    clear_affinity();
  }

  if (num_error_cycles) err = -1;
  if (num_error_notfound_root) err = -2;

  if (err) {
    printf("\n");
    printf("num_error_cycles is %ld\n", num_error_cycles);
    printf("num_error_notfound_root is %ld\n", num_error_notfound_root);
  }

  return err;
}



I64_t validate_bfs_tree(struct graph_t *G, struct bfs_t *BFS,
                        struct edgelist_t *edgelist, I64_t root) {
  const I64_t max_bfsvtx = G->n-1;
  const I64_t nv = G->n;

  /* merge local bfs tree */
  double tm = get_seconds();
  if (valiVERBOSE) printf("merging BFS tree ...");
  merge_local_bfs_tree(G, BFS);
  tm = get_seconds() - tm;
  if (valiVERBOSE) printf(" done. (%.3f seconds)\n", tm);

  /* This code is horrifically contorted because many compilers
   complain about continue, return, etc. in parallel sections. */
  if (root > max_bfsvtx || BFS->bfs_local[0].tree[root] != root) return -999;

#if USE_POOL_MEMORY == 0
  UL_t *seen_edge;
  assert( seen_edge = (UL_t *)malloc(sizeof(UL_t) * BIT_i(nv)) );
#else
  U8_t *pool = (U8_t *)&G->bfs_list[0].queue;
  UL_t *seen_edge;
  if (valiVERBOSE) printf("pool is %p\n", pool);
  assert( seen_edge = (UL_t *)&pool[sizeof(int) * nv] );
#endif

  I64_t trav_edges = 0;
  long wrong_edges_v  = 0; /* [err:-10] both i & j are on the same side of max_bfsvtx */
  long wrong_edges_w  = 0; /* [err:-11] both i & j are on the same side of max_bfsvtx */
  long disconnected_v = 0; /* [err:-12] All neighbors must be in the tree. */
  long disconnected_w = 0; /* [err:-13] All neighbors must be in the tree. */
  long not_adjacent   = 0; /* [err:-14] Check that the levels differ by no more than one. */
  long not_visited    = 0; /* [err:-15] */
  long multiple_root  = 0; /* [err:-16] */
  long sum_of_errors  = 0;
  long sum_of_wrong_nodes = 0;

  double level_time, tree1_time, tree2_time, t1, t2;

  t1 = get_seconds();
  int err = compute_levels(BFS->bfs_local[0].hops, nv, root, BFS);
  t2 = get_seconds();
  level_time = t2 - t1;
  if (valiVERBOSE) printf(" (Lv:%.3fs) ...", level_time);
  if (err) goto done;

  /* marge  */
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    I64_t v, n = G->n;
#if 0
    I64_t k;
    for (k = 1; k < G->num_graphs; ++k) {
      int *source_hops = BFS->bfs_local[0].hops;
      int *target_hops = BFS->bfs_local[k].hops;
      OMP("omp for schedule(guided)")
      for (v = 0; v < n; ++v) {
        target_hops[v] = source_hops[v];
      }
    }
#else
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    if (nodeid != 0) {
      I64_t ls, le;
      int *source_hops = BFS->bfs_local[0].hops;
      int *target_hops = BFS->bfs_local[nodeid].hops;
      partial_range(n, 0, lcores, coreid, &ls, &le);
      for (v = ls; v < le; ++v) {
        target_hops[v] = source_hops[v];
      }
    }
    clear_affinity();
#endif
  }

  t1 = get_seconds();
  OMP("omp parallel num_threads(get_numa_num_threads()) \
      reduction(+: wrong_edges_v, wrong_edges_w, disconnected_v, disconnected_w, \
      not_adjacent, trav_edges )") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    I64_t bn = BIT_i(nv), k;
    const I64_t *bfs_tree = (const I64_t *)BFS->bfs_local[nodeid].tree;
    const int   *hops     = (const int   *)BFS->bfs_local[nodeid].hops;

    OMP("omp for schedule(guided)")
    for (k = 0; k < bn; ++k)
      seen_edge[k] = 0;

    OMP("omp barrier");

    struct IJ_list_t *IJ_list = &edgelist->IJ_list[nodeid];
    struct packed_edge *IJ = IJ_list->edges;
    I64_t ls, le;
    partial_range(IJ_list->length, 0, lcores, coreid, &ls, &le);

    for (k = ls; k < le; ++k) {
      const I64_t v = get_v0_from_edge(&IJ[k]);
      const I64_t w = get_v1_from_edge(&IJ[k]);
      I64_t lvldiff;

      if (v < 0 || w < 0) continue;
      if (v > max_bfsvtx && w <= max_bfsvtx)
        ++wrong_edges_v, printf("E[%d][%lld] is %p, (%lld,%lld)\n", nodeid, k, &IJ[k], v, w);
      if (w > max_bfsvtx && v <= max_bfsvtx)
        ++wrong_edges_w, printf("E[%d][%lld] is %p, (%lld,%lld)\n", nodeid, k, &IJ[k], v, w);

      /* both v & w are on the same side of max_bfsvtx */
      if ( wrong_edges_v || wrong_edges_w || v > max_bfsvtx ) continue;

      /* All neighbors must be in the tree. */
      if (bfs_tree[v] >= 0 && bfs_tree[w] < 0) ++disconnected_v;
      if (bfs_tree[w] >= 0 && bfs_tree[v] < 0) ++disconnected_w;

      /* both i & j have the same sign */
      if ( disconnected_v || disconnected_w || bfs_tree[v] < 0 ) continue;

      /* Both i and j are in the tree, count as a traversed edge.
       * NOTE: This counts self-edges and repeated edges. They're part of the input data. */
      ++trav_edges;

      /* Mark seen tree edges. */
      if (v != w) {
        if (bfs_tree[v] == w) TEST_AND_SET_BITMAP(seen_edge, v);
        if (bfs_tree[w] == v) TEST_AND_SET_BITMAP(seen_edge, w);
      }
      lvldiff = hops[v] - hops[w];
      /* Check that the levels differ by no more than one. */
      if (lvldiff > 1 || lvldiff < -1) ++not_adjacent;
    }

    if ( wrong_edges_v) __sync_fetch_and_add(&sum_of_errors, wrong_edges_v);
    if ( wrong_edges_w) __sync_fetch_and_add(&sum_of_errors, wrong_edges_w);
    if (disconnected_v) __sync_fetch_and_add(&sum_of_errors, disconnected_v);
    if (disconnected_w) __sync_fetch_and_add(&sum_of_errors, disconnected_w);
    if (  not_adjacent) __sync_fetch_and_add(&sum_of_errors, not_adjacent);
    if (   not_visited) __sync_fetch_and_add(&sum_of_wrong_nodes, not_visited);
    if ( multiple_root) __sync_fetch_and_add(&sum_of_wrong_nodes, multiple_root);

    OMP("omp barrier");
    clear_affinity();
  }
  t2 = get_seconds();
  tree1_time = t2-t1;
  if (valiVERBOSE) printf(" (Tree1:%.3fs) ...", tree1_time);

  t1 = get_seconds();
  if ( !sum_of_errors && !sum_of_wrong_nodes ) {
    /* Check that every BFS edge was seen and that there's only one root. */
    OMP("omp parallel num_threads(get_numa_num_threads()) \
        reduction(+:not_visited,multiple_root)") {
      int id = omp_get_thread_num();
      int nodeid = get_numa_nodeid(id);
      int coreid = get_numa_vircoreid(id);
      int lcores = get_numa_online_cores(nodeid);
      pinned(USE_HYBRID_AFFINITY, id);
      OMP("omp barrier");
      const I64_t *bfs_tree = (const I64_t *)BFS->bfs_local[nodeid].tree;
      const I64_t     range = BFS->bfs_local[nodeid].range;
      const I64_t    offset = BFS->bfs_local[nodeid].offset;
      I64_t k, ls, le;
      partial_range(range, offset, lcores, coreid, &ls, &le);
      for (k = ls; k < le; ++k) {
        if ( !sum_of_wrong_nodes && k != root ) {
          if (bfs_tree[k] >= 0 && !ISSET_BITMAP(seen_edge, k)) ++not_visited;
          if (bfs_tree[k] == k) ++multiple_root;
        }
      }
      clear_affinity();
    }
  }
  t2 = get_seconds();
  tree2_time = t2-t1;
  if (valiVERBOSE) printf(" (Tree2:%.3fs) ...", tree2_time);
done:


#if USE_POOL_MEMORY == 0
  free(seen_edge);
#endif

  if ( wrong_edges_v) err = -10;
  if ( wrong_edges_w) err = -11;
  if (disconnected_v) err = -12;
  if (disconnected_w) err = -13;
  if (  not_adjacent) err = -14;
  if (   not_visited) err = -15;
  if ( multiple_root) err = -16;

  if (err) {
    printf("\n");
    printf(" wrong_edges_v is %ld\n",  wrong_edges_v);
    printf(" wrong_edges_w is %ld\n",  wrong_edges_w);
    printf("disconnected_v is %ld\n", disconnected_v);
    printf("disconnected_w is %ld\n", disconnected_w);
    printf("  not_adjacent is %ld\n",   not_adjacent);
    printf("   not_visited is %ld\n",    not_visited);
    printf(" multiple_root is %ld\n",  multiple_root);
  }

  if (err) {
    return err;
  } else {
    return trav_edges;
  }
}

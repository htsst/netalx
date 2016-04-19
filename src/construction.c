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
#include "atomic.h"

static I64_t *count_edgelist_size(struct edgelist_t *list);
static struct graph_t *allocate_graph(struct edgelist_t *list, I64_t num_nodes, I64_t *num_edges);
static I64_t count_node_degree(struct graph_t *G, struct edgelist_t *list);
static int parallel_prefix_sum(struct graph_t *G);
static void construct_subgraphs(struct graph_t *G, struct edgelist_t *list);
static void extract_duplicated_edges(struct graph_t *G);


struct graph_t *graph_construction(struct edgelist_t *list) {
  struct graph_t *G = NULL;

  I64_t *num_edges = count_edgelist_size(list);
  assert( G = allocate_graph(list, list->num_nodes, num_edges) );
  free(num_edges);

  printf("G = {\n");
  for (int k = 0; k < list->num_lists; ++k) {
    struct subgraph_t *BG = &G->BG_list[k];
    printf("  G[%d] = (BG: n=%lld (off=%*lld), m=%lld (n/N=%4.1f%%, m/M=%4.1f%%))\n",
           k,
           BG->n,
           intlog(10,BG->n), BG->offset, BG->m,
           100.0*BG->n/G->n, 100.0*BG->m/G->m);
  }
  printf("} = (N= %lld nodes, M= %lld edges)\n", G->n, G->m);

  printf("graph construction without self-loop and duplicated-edges ... \n");
  const double s_time = get_seconds();
  count_node_degree(G, list);
  printf("[elapsed: %6.2fs] finished: count each vertex degree\n", get_seconds()-s_time);
  parallel_prefix_sum(G);
  printf("[elapsed: %6.2fs] finished: parallel prefix sum\n", get_seconds()-s_time);
  construct_subgraphs(G, list);
  printf("[elapsed: %6.2fs] finished: construct sub-graphs\n", get_seconds()-s_time);
  extract_duplicated_edges(G);
  printf("[elapsed: %6.2fs] finished: extracting duplicated edges\n", get_seconds()-s_time);
  printf("done.\n");

  printf("G = {\n");
  for (int k = 0; k < list->num_lists; ++k) {
    struct subgraph_t *BG = &G->BG_list[k];
    printf("  G[%d] = (BG: n=%lld (off=%*lld), m=%lld (n/N=%4.1f%%, m/M=%4.1f%%))\n",
           k,
           BG->n,
           intlog(10,BG->n), BG->offset, BG->m,
           100.0*BG->n/G->n, 100.0*BG->m/G->m);
  }
  printf("} = (N= %lld nodes, M= %lld edges)\n", G->n, G->m);

  return G;
}




/* ------------------------------------------------------------
 * count_edgelist_size
 * ------------------------------------------------------------ */
static I64_t *count_edgelist_size(struct edgelist_t *list) {
  I64_t *ne = NULL;
  I64_t chunk = ROUNDUP(list->num_nodes/list->num_lists, 64);
  I64_t log_c = log2(chunk);
  I64_t self_loops = 0;

  assert( ne = (I64_t *)calloc(list->num_lists, sizeof(I64_t)) );

  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:self_loops)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    I64_t j, ls, le;
    I64_t *fm = (I64_t *)CALLOCA(list->num_lists * sizeof(I64_t));
    struct IJ_list_t *IJ_list = &list->IJ_list[nodeid];
    struct packed_edge *IJ = IJ_list->edges;
    partial_range(IJ_list->length, 0, lcores, coreid, &ls, &le);
    for (j = ls; j < le; ++j) {
      const I64_t v = get_v0_from_edge(&IJ[j]);
      const I64_t w = get_v1_from_edge(&IJ[j]);
      if (v != w) {
        ++fm[w >> log_c];
        ++fm[v >> log_c];
      } else {
        ++self_loops;
      }
    }

    int k;
    for (k = 0; k < list->num_lists; ++k) {
      SYNC_FETCH_AND_ADD(&ne[k], fm[k]);
    }

    clear_affinity();
  }
  printf("# of self-loops is %lld\n", self_loops);

  return ne;
}




/* ------------------------------------------------------------
 * allocate_graph
 * ------------------------------------------------------------ */
static struct graph_t *allocate_graph(struct edgelist_t *list, I64_t num_nodes, I64_t *num_edges) {
  int k;
  const size_t spacing = 64;
  I64_t *offset = (I64_t *)CALLOCA((list->num_lists+1) * sizeof(I64_t));
  I64_t chunk = ROUNDUP(list->num_nodes/list->num_lists, 64);
  I64_t sum_of_num_edges = 0;
  for (k = 0; k < list->num_lists; ++k) {
    offset[k] = chunk * k;
    sum_of_num_edges += num_edges[k];
  }
  offset[k] = num_nodes;

  struct graph_t *G = NULL;
  assert( G = (struct graph_t  *)calloc(1, sizeof(struct graph_t)) );
  G->n = num_nodes;
  G->m = sum_of_num_edges;
  G->chunk = chunk;
  G->num_graphs = list->num_lists;
  assert( G->BG_list  = (struct subgraph_t *)calloc(G->num_graphs+1, sizeof(struct subgraph_t)) );
  assert( G->pool     = (struct mempool_t  *)calloc(G->num_graphs+1, sizeof(struct mempool_t )) );

  double t1, t2;

  /* allocate local memory pool */
  t1 = get_seconds();
  for (k = 0; k < G->num_graphs; ++k) {
    const I64_t m = num_edges[k];
    const I64_t range = offset[k+1] - offset[k];
    size_t sz = 0;
    sz += ((range+1) * sizeof(I64_t) + spacing); /* backward1 */
    sz += (    (m+1) * sizeof(I64_t) + spacing); /* backward2 */
    sz  = ROUNDUP( sz, hugepage_size() );
    G->pool[k] = lmalloc(sz, k);
    printf("[node%02d] G[%d] (%p) %.2f GB using mmap with mbind(MPOL_BIND:%8s)\n",
           k, k, G->pool[k].pool, (double)(G->pool[k].memsize) / (1ULL<<30),
           &num2bit((1ULL << k), 0)[56]);
  }
  force_pool_page_faults(G->pool);
  t2 = get_seconds();
  printf("numa node local allocation takes %.3f seconds\n", t2-t1);


  /* graph representation */
  for (k = 0; k < G->num_graphs; ++k) {
    struct mempool_t *pool = &G->pool[k];
    struct subgraph_t *BG = &G->BG_list[k];
    size_t off = 0;
    BG->n      = offset[k+1] - offset[k];
    BG->m      = num_edges[k];
    BG->offset = offset[k];
    BG->start  = (I64_t *)&pool->pool[off];  off += (BG->n+1) * sizeof(I64_t) + spacing;
    BG->end    = (I64_t *)&pool->pool[off];  off += (BG->m+1) * sizeof(I64_t) + spacing;
    assert( ROUNDUP( off, hugepage_size() ) == pool->memsize );
  }

  return G;
}




/* ------------------------------------------------------------
 * construct_subgraphs
 * ------------------------------------------------------------ */
static I64_t count_node_degree(struct graph_t *G, struct edgelist_t *list) {
  I64_t fixed = 0;

  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    struct subgraph_t *BG = &G->BG_list[nodeid];
    I64_t ls, le, j;
    partial_range(BG->n+1, 0, lcores, coreid, &ls, &le);
    for (j = ls; j < le; ++j) {
      BG->start[j] = 0;
    }
    OMP("omp barrier");

    int k;
    for (k = 0; k < list->num_lists; ++k) {
      int target = (nodeid+k+1) % G->num_graphs;

      const I64_t log_c = log2(G->chunk);
      struct packed_edge *E = list->IJ_list[target].edges;
      I64_t lls, lle;
      I64_t BG_offset = BG->offset;
      I64_t *BG_start = BG->start;
      partial_range(list->IJ_list[target].length, 0, lcores, coreid, &lls, &lle);
      for (j = lls; j < lle; ++j) {
        const I64_t v = get_v0_from_edge(&E[j]);
        const I64_t w = get_v1_from_edge(&E[j]);
        if (v != w && (w >> log_c) == nodeid) {
          SYNC_FETCH_AND_ADD(&BG_start[w+1 - BG_offset], 1); /* v <- w */
        }
        if (v != w && (v >> log_c) == nodeid) {
          SYNC_FETCH_AND_ADD(&BG_start[v+1 - BG_offset], 1); /* w <- v */
        }
      }
    }
    OMP("omp barrier");
  }

  return fixed;
}




/* ------------------------------------------------------------
 * parallel_prefix_sum
 * ------------------------------------------------------------ */
static int parallel_prefix_sum(struct graph_t *G) {
  I64_t BG_ps_off[MAX_NODES][MAX_CPUS+1] = {{0}};


  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    /* constuct start[] */
    struct subgraph_t *BG = &G->BG_list[nodeid];
    I64_t j, ls, le;
    /* parallel prefix-sum */
    partial_range(BG->n+1, 0, lcores, coreid, &ls, &le);
    for (j = ls+1; j < le; ++j) {
      BG->start[j] += BG->start[j-1];
    }
    BG_ps_off[nodeid][coreid+1] = BG->start[le-1];
    OMP("omp barrier");

    if (coreid == 0) {
      I64_t p;
      for (p = 1; p <= lcores; ++p) {
        BG_ps_off[nodeid][p] += BG_ps_off[nodeid][p-1];
      }
    }
    OMP("omp barrier");

    const I64_t BG_ps_off_local = BG_ps_off[nodeid][coreid];
    if ( BG_ps_off_local ) {
      partial_range(BG->n+1, 0, lcores, coreid, &ls, &le);
      for (j = ls; j < le; ++j) {
        BG->start[j] += BG_ps_off_local;
      }
    }
    OMP("omp barrier");

    if ( BG->start[BG->n] != BG->m ) {
      printf("BG->n is %lld(%lld)\n", BG->m, BG->start[BG->n]);
    }
    assert( BG->start[BG->n] == BG->m );
  }
  return 0;
}




/* ------------------------------------------------------------
 * construct_subgraphs
 * ------------------------------------------------------------ */
static void construct_subgraphs(struct graph_t *G, struct edgelist_t *list) {
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    /* constuct start[] */
    struct subgraph_t *BG = &G->BG_list[nodeid];
    int k;
    I64_t j;
    /* construct adjacency_list */
    for (k = 0; k < list->num_lists; ++k) {
      int target = (nodeid+k+1) % G->num_graphs;

      const I64_t log_c = log2(G->chunk);
      struct packed_edge *E = list->IJ_list[target].edges;
      I64_t lls, lle;
      partial_range(list->IJ_list[target].length, 0, lcores, coreid, &lls, &lle);

      for (j = lls; j < lle; ++j) {
        const I64_t v = get_v0_from_edge(&E[j]);
        const I64_t w = get_v1_from_edge(&E[j]);
        if (v != w) {
          if ((w>>log_c) == nodeid) {
            BG->end[ SYNC_FETCH_AND_ADD(&BG->start[w - BG->offset], 1) ] = v;
          }
          if ((v>>log_c) == nodeid) {
            BG->end[ SYNC_FETCH_AND_ADD(&BG->start[v - BG->offset], 1) ] = w;
          }
        }
      }
    }
    OMP("omp barrier");

    if (coreid == 0) {
      for (j = BG->n; j > 0; --j) {
        BG->start[j] = BG->start[j-1];
      }
      BG->start[0] = 0;
    }
    OMP("omp barrier");
    clear_affinity();
  }
}




/* ------------------------------------------------------------
 * sorting by degree
 * ------------------------------------------------------------ */
static inline int ilog2(unsigned long long x) {
  int l = 0, u = (x & (x-1)) != 0;
  if (x >= 1ULL << 32) { x >>= 32; l |= 32; }
  if (x >= 1ULL << 16) { x >>= 16; l |= 16; }
  if (x >= 1ULL <<  8) { x >>=  8; l |=  8; }
  if (x >= 1ULL <<  4) { x >>=  4; l |=  4; }
  if (x >= 1ULL <<  2) { x >>=  2; l |=  2; }
  if (x >= 1ULL <<  1) {           l |=  1; }
  return l + u;
}

static I64_t *degree_table = NULL;
static I64_t  degree_max   = 0;
static I64_t  degree_min   = 0;

static void update_degree_table(struct graph_t *G) {
  memset(degree_table, 0x00, G->n * sizeof(I64_t));
  degree_max = 0, degree_min = G->m;

  I64_t degree_sum = 0;
  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:degree_sum)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    struct subgraph_t *BG = &G->BG_list[nodeid];
    I64_t j, ls, le, mindg = BG->m, maxdg = 0;
    partial_range(BG->n, 0, lcores, coreid, &ls, &le);
    for (j = ls; j < le; ++j) {
      const I64_t bs = BG->start[j], be = BG->start[j+1], dg = be-bs, v = j+BG->offset;
      degree_table[v] = dg;
      degree_sum += dg;
      if (maxdg < dg) maxdg = dg;
      if (mindg > dg) mindg = dg;
    }
    OMP("omp critical")
    if (degree_max < maxdg) degree_max = maxdg;
    OMP("omp critical")
    if (degree_min > mindg) degree_min = mindg;
    OMP("omp barrier");
    clear_affinity();
  }

  printf("sum([d for d in degree_list]) is %lld (= #edges: %lld)\n", degree_sum, G->m);
  printf("min([d for d in degree_list]) is %lld\n", degree_min);
  printf("max([d for d in degree_list]) is %lld\n", degree_max);
  assert( degree_sum == G->m );
}

static int intdegreecmp(const void *a, const void *b) {
  const I64_t _a = *(const I64_t *)a;
  const I64_t _b = *(const I64_t *)b;
  const I64_t _ia = degree_table[_a];
  const I64_t _ib = degree_table[_b];
#if 1
  /* descending order */
  /* Intel(R) Xeon(R) CPU E5-4640 2.40GHz */
  /*    Kronecker SCALE=26, edgefactor=16 */
  if (_ia > _ib) return -1;
  if (_ia < _ib) return  1;
  if ( _a >  _b) return -1;
  if ( _a <  _b) return  1;
#else
  /* ascending order */
  /* Intel(R) Xeon(R) CPU E5-4640 2.40GHz */
  /*    Kronecker SCALE=26, edgefactor=16 */
  if (_ia < _ib) return -1;
  if (_ia > _ib) return  1;
  if ( _a <  _b) return -1;
  if ( _a >  _b) return  1;
#endif
  return 0;
}

static void sort_adjacency_list_by_degree(struct graph_t *G) {
  I64_t duplicates = 0;
  const double t1 = get_seconds();
  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:duplicates)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    I64_t ls, le;
    struct subgraph_t *BG = &G->BG_list[nodeid];
    partial_range(BG->n, 0, lcores, coreid, &ls, &le);
    for (I64_t j = ls; j < le; ++j) {
      I64_t bs = BG->start[j], be = BG->start[j+1], dg = be-bs;
      if (dg) {
        I64_t *array = &BG->end[bs];
        I64_t uniq_dg = uniq(array, dg, sizeof(I64_t), qsort, intdegreecmp);
        for (I64_t i = uniq_dg; i < dg; ++i) array[i] = -1; /* filled -1 */
        duplicates += dg - uniq_dg;
      }
    }

    OMP("omp barrier");
    clear_affinity();
  }
  const double t2 = get_seconds();

  printf("found %lld duplicated edges (%.3f %%) (%.3f seconds)\n",
         duplicates, 1.0*duplicates/G->m, t2-t1);
}


static void dump_degree_distribution(struct graph_t *G);

static void extract_duplicated_edges(struct graph_t *G) {
  assert( degree_table = (I64_t *)calloc(G->n, sizeof(I64_t)) );
  update_degree_table(G);
  sort_adjacency_list_by_degree(G);

  const double t1 = get_seconds();
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    pinned(USE_HYBRID_AFFINITY, id);
    if (coreid == 0) {
      I64_t crr;
      /* Backward Graph */
      crr = 0;
      struct subgraph_t *BG = &G->BG_list[nodeid];
      for (I64_t j = 0; j < BG->n; ++j) {
        const I64_t ls = BG->start[j], le = BG->start[j+1];
        BG->start[j] = crr;
        for (I64_t w = ls; w < le; ++w) {
          I64_t v = BG->end[w];
          if (v != -1) {
            BG->end[crr++] = v;
          }
        }
      }
      BG->start[BG->n] = crr;
      BG->m = crr;
    }
  }

  /* update # of edges */
  G->m = 0;
  for (int k = 0; k < G->num_graphs; ++k) {
    G->m += G->BG_list[k].m;
  }
  update_degree_table(G);

  const double t2 = get_seconds();
  printf("extract duplicated edges (#threads=1, %.3f seconds)\n", t2-t1);

  /* dump degree */
  dump_degree_distribution(G);
  free(degree_table);
}




/* ------------------------------------------------------------
 * dump degree distribution
 * ------------------------------------------------------------ */
static void dump_degree_distribution(struct graph_t *G) {
  const char *dump_DC = getenv("DUMP_DC");
  if (dump_DC) {
    FILE *fp = NULL;
    assert( fp = fopen(dump_DC, "w") );
    double t1 = get_seconds();
    fprintf(fp, "c file name: %s\n", dump_DC);
    fprintf(fp, "c instance name: %s Graph, SCALE:%d, edgefactor:%d\n",
            use_RMAT_generator?"R-MAT":"Kronecker", SCALE, edgefactor);
    time_t now = time(NULL);
    fprintf(fp, "c created: %s", ctime(&now));
    fprintf(fp, "c\n");
    fprintf(fp, "p sp %lld %lld\n", G->n, G->m);
    fprintf(fp, "l Degree\n");
    fprintf(fp, "l Degree in log2 scale\n");
    fprintf(fp, "c\n");
    for (I64_t i = 0; i < G->n; ++i) {
      fprintf(fp, "d %lld %lld %d\n", i+1, degree_table[i], ilog2(degree_table[i]));
    }
    fclose(fp);
    double t2 = get_seconds();
    printf("dump degree distribution (%.3f seconds)\n", t2-t1);
  }


  const char *dump_dist = getenv("DUMP_DEGREE");
  if (dump_dist) {
    I64_t *deg = NULL, i, sumup = 0;
    assert( deg = (I64_t *)calloc(degree_max+1, sizeof(I64_t)) );
    double t1 = get_seconds();
    OMP("omp parallel num_threads(get_numa_num_threads())") {
      int id = omp_get_thread_num();
      int nodeid = get_numa_nodeid(id);
      int coreid = get_numa_vircoreid(id);
      int lcores = get_numa_online_cores(nodeid);
      pinned(USE_HYBRID_AFFINITY, id);
      struct subgraph_t *BG = &G->BG_list[nodeid];
      I64_t j, ls, le;
      partial_range(BG->n, BG->offset, lcores, coreid, &ls, &le);
      for (j = ls; j < le; ++j) {
        SYNC_FETCH_AND_ADD(&deg[ degree_table[j] ], 1);
      }
      OMP("omp barrier");
      clear_affinity();
    }
    FILE *fp = NULL;
    assert( fp = fopen(dump_dist, "w") );
    fprintf(fp, "# degree #nodes\n");
#if 0
    for (i = 0; i <= degree_max; ++i) {
      sumup += deg[i];
      if (deg[i] != 0)
        fprintf(fp, "%lld %lld %lld\n", i, deg[i], sumup);
    }
#else
    for (i = degree_max; i >= 0; --i) {
      sumup += deg[i];
      if (deg[i] != 0)
        fprintf(fp, "%lld %lld %lld\n", i, deg[i], sumup);
    }
#endif
    printf("sumup is %lld\n", sumup);
    fclose(fp);
    free(deg);
    double t2 = get_seconds();
    printf("dump degree distribution (%.3f seconds)\n", t2-t1);
    exit(1);
  }
}




/* ------------------------------------------------------------
 * dump graph
 * ------------------------------------------------------------ */
int dump_graph(const char *filename, struct graph_t *G) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    return 1;
  }

  fprintf(fp, "p sp %lld %lld\n", G->n, G->m);
  for (int k = 0; k < G->num_graphs; ++k) {
    struct subgraph_t *BG = &G->BG_list[k];
    for (I64_t i = 0; i < BG->n; ++i) {
      const I64_t w = i + BG->offset;
      for (I64_t j = BG->start[i]; j < BG->start[i+1]; ++j) {
        const I64_t v = BG->end[j];
        fprintf(fp, "a %lld %lld 1\n", v+1, w+1);
      }
    }
  }
  fclose(fp);
  return 0;
}




/* ------------------------------------------------------------
 * free graph
 * ------------------------------------------------------------ */
void free_graph(struct graph_t *G) {
  int k;
  for (k = 0; k < G->num_graphs; ++k) {
    lfree(G->pool[k]);
  }
  free(G->BG_list);
  free(G->pool);
  free(G);
}

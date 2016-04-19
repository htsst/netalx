/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version considerably modified by The GraphCREST Project
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
 * Copyright (C) 2014-2015 The GraphCREST Project, Tokyo Institute of Technology
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
 * ------------------------------------------------------------------------ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "generation.h"
#include "construction.h"
#include "common.h"
#include "graph_generator.h"
#include "defs.h"
#include "dump.h"
#include "std_sort.h"
#include "external_full_construction_bucket.h"

static void mmap_edges (struct edgelist_t *list, struct dumpfiles_t *DF_E);

static I64_t *count_edgebucket_size(struct edgelist_t *list, I64_t num_bucket);
static struct edgelist_t *allocate_edgebucket_mmap(struct edgelist_t *list, int num_buckets,
 struct dumpfiles_t *DF_B, I64_t *bucket_size_list);
static I64_t *divide_edges_into_buckets (struct edgelist_t *list, struct edgelist_t *list_bucket);

static I64_t *graph_construction_onebyone(struct edgelist_t *list,
 struct edgelist_t *list_full, I64_t *num_edges,
 struct dumpfiles_t *DF_B);

static struct graph_t *allocate_graph_info(struct edgelist_t *list, I64_t num_nodes, I64_t *num_edges);
static void allocate_subgraph(struct graph_t *G, int graph_no);
static I64_t count_node_degree(struct graph_t *G, struct edgelist_t *list, int graph_no);
static int parallel_prefix_sum(struct graph_t *G, int graph_no);
static void construct_subgraphs(struct graph_t *G, struct edgelist_t *list, int graph_no);
static void extract_duplicated_edges(struct graph_t *G);

static void dump_subgraph_with_merging_files(struct graph_t *G, struct graph_t *G_full,
                                             int subgraph_no, I64_t offset);

static void free_graph_info(struct graph_t *G);


I64_t *external_full_graph_construction(struct edgelist_t *list, struct dumpfiles_t *DF_E_dmy)
{

  DF_E_dmy = DF_E_dmy; // dummy

  int k;
  struct dumpfiles_t *DF_E_sth = init_dumpfile_info_edgelist("", 1);
  open_files(DF_E_sth);
  mmap_edges(list, DF_E_sth);

  printf("open edgelist bucket files\n");
  I64_t num_buckets = ROUNDUP(get_numa_num_threads(), list->num_lists);
  struct dumpfiles_t *DF_B = init_dumpfile_info_edgelist_bucket("", num_buckets);
  open_files_new(DF_B);


  printf("counting edgebucket size ....\n");
  I64_t *bucket_size_list = count_edgebucket_size(list, num_buckets);
  printf("length of edgebucket = {\n");
  for (k=0; k < num_buckets; k++) {
    printf ("[%2d] = %lld\n", k, bucket_size_list[k]);
  }
  printf ("}\n");
  struct edgelist_t *list_bucket = allocate_edgebucket_mmap(list, num_buckets, DF_B, bucket_size_list);

  printf("divide edges into buckets ....\n");
  double t1 = get_seconds();
  I64_t *ne = divide_edges_into_buckets(list, list_bucket);
  double t2 = get_seconds();
  printf ("done. takes %6.2f seconds\n", (t2-t1));
  printf("drop page caches and unmap pools for edgelist\n\n");
  for (k = 0; k < list->num_lists; ++k) {
    drop_pagecaches_pool(&(list->pool[k]));
    unmap_pool(&(list->pool[k]));
  }
  close_files(DF_E_sth);

  I64_t *ne_onmem = graph_construction_onebyone(list_bucket, list, ne, DF_B);

  // printf("drop page caches and unmap pools for edgelist\n");
  // for (k = 0; k < list_bucket->num_lists; ++k) {
  //   drop_pagecaches_pool(&(list_bucket->pool[k]));
  //   unmap_pool(&(list_bucket->pool[k]));
  // }

  close_files(DF_B);
  free_files(DF_B);


  return ne_onmem;
}




static void mmap_edges (struct edgelist_t *list, struct dumpfiles_t *DF_E)
{
  int k;

  /* mmap */
  for (k = 0; k < list->num_lists; ++k) {
    struct IJ_list_t *IJ_list = list->IJ_list;
    int fd = DF_E->file_info_list[k].fd;
    assert(fd >= 0);
    list->pool[k] = mmap_pool(0, fd, 0, NULL);
    printf("[node%02d] E[%d]=[%.2e,%.2e] (%p) %.2f GB using mmap(no mbind : %s)\n",
     k, k,
     (double)(IJ_list[k].offset), (double)(IJ_list[k].offset+IJ_list[k].length),
     list->pool[k].pool, (double)(list->pool[k].memsize) / (1ULL<<30),
     DF_E->file_info_list[k].fname);
  }

  for (k = 0; k < list->num_lists; ++k) {
    list->IJ_list[k].edges = (struct packed_edge *)list->pool[k].pool;
  }


}






/* ------------------------------------------------------------
 * count_edgelist_size
 * ------------------------------------------------------------ */
 static I64_t *count_edgebucket_size(struct edgelist_t *list, I64_t num_buckets) {
  I64_t *ne = NULL;
  I64_t chunk = ROUNDUP(list->num_nodes/num_buckets, 64);
  I64_t log_c = log2(chunk);

  assert( ne = (I64_t *)calloc(num_buckets, sizeof(I64_t)) );

  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    struct IJ_list_t *IJ_list = &list->IJ_list[nodeid];
    struct packed_edge *E = IJ_list->edges;
    I64_t *fm = (I64_t *)CALLOCA(num_buckets * sizeof(I64_t));

    I64_t j, ls, le;
    partial_range(IJ_list->length, 0, lcores, coreid, &ls, &le);

    if (coreid == 0) {
      int err = madvise((void *)E, IJ_list->length * sizeof(struct packed_edge), MADV_SEQUENTIAL);
      if (err == -1)
        perror("madvice seaquential count_edgebucket_size");
    }
    OMP("omp barrier");

    for (j = ls; j < le; ++j) {
      const I64_t v = get_v0_from_edge(&E[j]);
      const I64_t w = get_v1_from_edge(&E[j]);

      ++fm[w >> log_c];
      ++fm[v >> log_c];
    }

    for (int k = 0; k < num_buckets; ++k) {
      SYNC_FETCH_AND_ADD(&ne[k], fm[k]);
    }

    OMP("omp barrier");
  }

  return ne;
}



static struct edgelist_t *allocate_edgebucket_mmap(struct edgelist_t *list_source, int num_buckets,
 struct dumpfiles_t *DF_B, I64_t *bucket_size_list)
{
  struct edgelist_t *list_bucket = NULL;
  assert( list_bucket = (struct edgelist_t *)calloc(1, sizeof(struct edgelist_t)) );

  list_bucket->num_nodes = list_source->num_nodes;
    // list_bucket->num_edges = list->num_edges;
  list_bucket->num_lists = num_buckets;
  assert( list_bucket->IJ_list = (struct IJ_list_t *)calloc(list_bucket->num_lists+1,
                                                            sizeof(struct IJ_list_t)));
  assert( list_bucket->pool    = (struct mempool_t *)calloc(list_bucket->num_lists+1,
                                                            sizeof(struct mempool_t)) );

  int k;
  I64_t offset = 0;
    //I64_t chunk = ROUNDUP( list_bucket->num_edges/list_bucket->num_lists, 64);
  for (k = 0; k < list_bucket->num_lists; ++k) {
    list_bucket->IJ_list[k].offset = offset;
    offset += bucket_size_list[k];
  }
  list_bucket->num_edges = list_bucket->IJ_list[k].offset = offset;
  for (k = 0; k < list_bucket->num_lists; ++k) {
    list_bucket->IJ_list[k].length = list_bucket->IJ_list[k+1].offset - list_bucket->IJ_list[k].offset;
  }
  list_bucket->IJ_list[k].length = 0;

    /* mmap */
  for (k = 0; k < num_buckets; ++k) {

    struct IJ_list_t *IJ_list = list_bucket->IJ_list;
    size_t sz = 0;
    sz += IJ_list[k].length * sizeof(struct packed_edge);
    //sz  = ROUNDUP( sz, hugepage_size() );

    int fd = DF_B->file_info_list[k].fd;
    assert(fd >= 0);
    list_bucket->pool[k] = mmap_pool(sz, fd, 0, NULL);
    printf("[node%02d] E[%d]=[%.2e,%.2e] (%p) %.2f GB using mmap(no mbind : %s)\n",
     k, k,
     (double)(IJ_list[k].offset), (double)(IJ_list[k].offset+IJ_list[k].length),
     list_bucket->pool[k].pool, (double)(list_bucket->pool[k].memsize) / (1ULL<<30),
     DF_B->file_info_list[k].fname);
  }

  for (k = 0; k < list_bucket->num_lists; ++k) {
    list_bucket->IJ_list[k].edges = (struct packed_edge *)list_bucket->pool[k].pool;
  }

  return list_bucket;
}



static I64_t *divide_edges_into_buckets (struct edgelist_t *list_source, struct edgelist_t *list_bucket)
{

  I64_t chunk_b = ROUNDUP(list_bucket->num_nodes/list_bucket->num_lists, 64ULL);
  I64_t log_c_b = log2(chunk_b);

  I64_t self_loops = 0;
  I64_t *ne;
  assert( ne = (I64_t *)calloc(list_bucket->num_lists, sizeof(I64_t)) );

  I64_t *bucket_length_list;
  assert( bucket_length_list = (I64_t *)CALLOCA(list_bucket->num_lists * sizeof(I64_t)) );

  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:self_loops)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    I64_t i, ls, le;
    I64_t *fm = (I64_t *)CALLOCA(list_bucket->num_lists * sizeof(I64_t));
    struct IJ_list_t *IJ_list = &list_source->IJ_list[nodeid];
    partial_range(IJ_list->length, 0, lcores, coreid, &ls, &le);
    struct packed_edge *E = IJ_list->edges;
    if (coreid == 0) {
      int err = madvise((void *)E, IJ_list->length * sizeof(struct packed_edge), MADV_SEQUENTIAL);
      if (err == -1)
        perror("madvice seaquential divide_edges_into_buckets");
    }
    OMP("omp barrier");

    for (i = ls; i < le; i++) {

      I64_t v = get_v0_from_edge(&E[i]);
      I64_t target_no_1 = v >> log_c_b;
      struct IJ_list_t *IJ_list_bucket_1 = &(list_bucket->IJ_list[target_no_1]);
      IJ_list_bucket_1->edges[ SYNC_FETCH_AND_ADD(&bucket_length_list[target_no_1], 1) ] = E[i];

      I64_t w = get_v1_from_edge(&E[i]);
      struct packed_edge r_edge; write_edge(&r_edge, w, v);
      I64_t target_no_2 = w >> log_c_b;
      struct IJ_list_t *IJ_list_bucket_2 = &(list_bucket->IJ_list[target_no_2]);
      IJ_list_bucket_2->edges[ SYNC_FETCH_AND_ADD(&bucket_length_list[target_no_2], 1) ] = r_edge;

      if (v != w) {

        ++fm[w >> log_c_b];
        ++fm[v >> log_c_b];

      } else {
        ++self_loops;
      }
    }

    int k;
    for (k = 0; k < list_bucket->num_lists; ++k) {
      SYNC_FETCH_AND_ADD(&ne[k], fm[k]);
    }

    clear_affinity();
  }
  printf("# of self-loops is %lld\n", self_loops);

  for (int k = 0; k < list_bucket->num_lists; k++) {
    assert( list_bucket->IJ_list[k].length == bucket_length_list[k] );
  }

  return ne;

}


static I64_t  degree_max = 0;
static I64_t  degree_min = 0;

I64_t *ne_onm;
static I64_t *graph_construction_onebyone(struct edgelist_t *list, struct edgelist_t *list_full,
                                          I64_t *num_edges, struct dumpfiles_t *DF_B) {


  assert( ne_onm = (I64_t *)calloc(list_full->num_lists, sizeof(I64_t)) );

  struct graph_t *G = NULL;
  struct graph_t *G_full = NULL;
  G = allocate_graph_info(list, list->num_nodes, num_edges);
  G_full = allocate_graph_info(list_full, list_full->num_nodes, num_edges);




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


  printf("graph construction into NVM without self-loop and duplicated-edges ... \n");

  degree_max = 0, degree_min = list->num_edges;
  I64_t offset = 0;
  const double s_time0 = get_seconds();
  for (int k = 0; k < list->num_lists; k++) {
    const double s_time1 = get_seconds();
    printf("-- construct subgraph [%d] --\n", k);
    allocate_subgraph(G, k);
    count_node_degree(G, list, k);
    printf("[elapsed: %6.2fs] finished: count each vertex degree\n", get_seconds()-s_time1);
    parallel_prefix_sum(G, k);
    printf("[elapsed: %6.2fs] finished: parallel prefix sum\n", get_seconds()-s_time1);
    construct_subgraphs(G, list, k);
    printf("[elapsed: %6.2fs] finished: construct sub-graphs\n", get_seconds()-s_time1);

    printf("drop page cache, unmap pool and delete file for bucket edgelist\n");
    erasure_pool_file(&list->pool[k], DF_B->file_info_list[k].fd, DF_B->file_info_list[k].fname);
    dump_subgraph_with_merging_files(G, G_full, k, offset);
    offset = G->BG_list[k].start[G->BG_list[k].n];
    printf("[elapsed: %6.2fs] finished: dump sub-graphs\n", get_seconds()-s_time1);

    struct subgraph_t *BG = &G->BG_list[k];
    printf("G[%d] = (BG: n=%lld (off=%*lld), m=%lld (n/N=%4.1f%%, m/M=%4.1f%%))\n",
     k,
     BG->n,
     intlog(10,BG->n), BG->offset, BG->m,
     100.0*BG->n/G->n, 100.0*BG->m/G->m);

    printf("free subgraph\n\n");
    lfree(G->pool[k]);

  }
  printf("finished: construct all subgraphs [elapsed: %6.2fs]\n\n", get_seconds() - s_time0);


  for(int k = 0; k < G_full->num_graphs; k++)
    ne_onm[k] = G->m;


  printf("-- Extract duplicated edges --\n");
  const double s_time2 = get_seconds();
  extract_duplicated_edges(G);
  printf("finished: extracting duplicated edges [elapsed: %6.2fs]\n", get_seconds()-s_time2);


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


  free_graph_info(G);
  free_graph_info(G_full);

  return ne_onm;
}


/* ------------------------------------------------------------
 * allocate_graph
 * ------------------------------------------------------------ */
 static struct graph_t *allocate_graph_info(struct edgelist_t *list, I64_t num_nodes, I64_t *num_edges) {
  int k;
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

  /* graph representation */
  for (k = 0; k < G->num_graphs; ++k) {
    struct subgraph_t *BG = &G->BG_list[k];
    BG->n      = offset[k+1] - offset[k];
    BG->m      = num_edges[k];
    BG->offset = offset[k];
  }

  return G;
}



static void allocate_subgraph(struct graph_t *G, int graph_no)
{

  const size_t spacing = 64;
  /* allocate local memory pool */
  double t1 = get_seconds();

  const I64_t m = G->BG_list[graph_no].m;
  const I64_t range = G->BG_list[graph_no].n;
  size_t sz = 0;
  sz += ((range+1) * sizeof(I64_t) + spacing); /* backward1 */
  sz += (    (m+1) * sizeof(I64_t) + spacing); /* backward2 */
  sz  = ROUNDUP( sz, hugepage_size() );
  G->pool[graph_no] = lmalloc(sz, -1);
  printf("[node%02d] G[%d] (%p) %.2f GB using mmap with mbind(MPOL_BIND:%8s)\n",
   -1, graph_no, G->pool[graph_no].pool, (double)(G->pool[graph_no].memsize) / (1ULL<<30),
   &num2bit((1ULL << graph_no), 0)[56]);
  force_page_faults_single(G->pool[graph_no].pool, G->pool[graph_no].memsize);
  double t2 = get_seconds();
  printf("numa node local allocation takes %.3f seconds\n", t2-t1);

  struct mempool_t *pool = &G->pool[graph_no];
  struct subgraph_t *BG = &G->BG_list[graph_no];
  size_t off = 0;
  BG->start  = (I64_t *)&pool->pool[off];  off += (BG->n+1) * sizeof(I64_t) + spacing;
  BG->end    = (I64_t *)&pool->pool[off];  off += (BG->m+1) * sizeof(I64_t) + spacing;
  assert( ROUNDUP( off, hugepage_size() ) == pool->memsize );

}


/* ------------------------------------------------------------
 * construct_subgraphs
 * ------------------------------------------------------------ */
 static I64_t count_node_degree(struct graph_t *G, struct edgelist_t *list, int graph_no) {
  I64_t fixed = 0;

  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int threads = get_numa_num_threads();
    OMP("omp barrier");

    struct subgraph_t *BG = &G->BG_list[graph_no];
    I64_t ls, le, j;
    partial_range(BG->n+1, 0, threads, id, &ls, &le);
    for (j = ls; j < le; ++j) {
      BG->start[j] = 0;
    }
    OMP("omp barrier");

      //int target = (nodeid+k+1) % G->num_graphs;

      //const I64_t log_c = log2(G->chunk);
    struct packed_edge *E = list->IJ_list[graph_no].edges;
    I64_t lls, lle;
    I64_t BG_offset = BG->offset;
    I64_t *BG_start = BG->start;
    partial_range(list->IJ_list[graph_no].length, 0, threads, id, &lls, &lle);
    for (j = lls; j < lle; ++j) {
      const I64_t v = get_v0_from_edge(&E[j]);
      const I64_t w = get_v1_from_edge(&E[j]);
      if (v != w) {
          SYNC_FETCH_AND_ADD(&BG_start[v+1 - BG_offset], 1); /* w <- v */
          //SYNC_FETCH_AND_ADD(&BG_start[w+1 - BG_offset], 1); /* v <- w */
      }
    }
    OMP("omp barrier");
  }

  return fixed;
}




/* ------------------------------------------------------------
 * parallel_prefix_sum
 * ------------------------------------------------------------ */
 static int parallel_prefix_sum(struct graph_t *G, int graph_no) {
  I64_t BG_ps_off[MAX_THREADS+1] = {0};


  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int threads = get_numa_num_threads();
    OMP("omp barrier");

    /* constuct start[] */
    struct subgraph_t *BG = &G->BG_list[graph_no];
    I64_t j, ls, le;
    /* parallel prefix-sum */
    partial_range(BG->n+1, 0, threads, id, &ls, &le);
    for (j = ls+1; j < le; ++j) {
      BG->start[j] += BG->start[j-1];
    }
    BG_ps_off[id+1] = BG->start[le-1];
    OMP("omp barrier");

    if (id == 0) {
      I64_t p;
      for (p = 1; p <= threads; ++p) {
        BG_ps_off[p] += BG_ps_off[p-1];
      }
    }
    OMP("omp barrier");

    const I64_t BG_ps_off_local = BG_ps_off[id];
    if ( BG_ps_off_local ) {
      partial_range(BG->n+1, 0, threads, id, &ls, &le);
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
 static void construct_subgraphs(struct graph_t *G, struct edgelist_t *list, int graph_no) {
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int threads = get_numa_num_threads();
    OMP("omp barrier");

    /* constuct start[] */
    struct subgraph_t *BG = &G->BG_list[graph_no];
    I64_t j;
    /* construct adjacency_list */
    struct packed_edge *E = list->IJ_list[graph_no].edges;
    I64_t lls, lle;
    partial_range(list->IJ_list[graph_no].length, 0, threads, id, &lls, &lle);

    for (j = lls; j < lle; ++j) {
      const I64_t v = get_v0_from_edge(&E[j]);
      const I64_t w = get_v1_from_edge(&E[j]);
      if (v != w) {
        BG->end[ SYNC_FETCH_AND_ADD(&BG->start[v - BG->offset], 1) ] = w;
        //BG->end[ SYNC_FETCH_AND_ADD(&BG->start[w - BG->offset], 1) ] = v;
      }
    }
    OMP("omp barrier");

    if (id == 0) {

      for (j = BG->n; j > 0; --j) {
        const I64_t dg = BG->start[j] = BG->start[j-1];
        if (degree_max < dg) degree_max = dg;
        if (degree_min > dg) degree_min = dg;

        BG->start[j] = BG->start[j-1];
      }
      BG->start[0] = 0;
    }
    OMP("omp barrier");
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


static unsigned char *degree_table_code = NULL;

static void update_degree_table(struct graph_t *G) {


  struct dumpfiles_t *DF_s = init_dumpfile_info_graph("", 1, 1);
  open_files(DF_s);

  const I64_t lower_limit = 1;

  memset(degree_table_code, 0x00, G->n * sizeof(unsigned char));

  I64_t max = degree_max;
  I64_t min = degree_min;
  I64_t chunk = ROUNDUP( ( max - min + 1) / (256LL-lower_limit-1LL), 2);
  I64_t log_c = log2(chunk);

  degree_max = 0, degree_min = G->m;

  I64_t degree_sum = 0;

  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:degree_sum)") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    int nodes = get_numa_online_nodes();
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");

    int lgraphs = G->num_graphs / nodes;
    I64_t mindg = G->m, maxdg = 0;
    size_t base_offset = G->BG_list[lgraphs * nodeid].offset;
    I64_t l_onmem_degree_sum = 0;
    for (int k = 0; k < lgraphs; k++) {
      int subgraph_no = lgraphs*nodeid + k;
      struct subgraph_t *BG = &G->BG_list[subgraph_no];

      if (coreid == 0) {
        int fd = DF_s->file_info_list[nodeid].fd;
        const I64_t range = BG->n;
        size_t sz = (range+1) * sizeof(I64_t);
        //sz = ROUNDUP( sz, hugepage_size() );
        size_t chip_size;
        G->pool[subgraph_no] = mmap_pool(sz, fd, (BG->offset - base_offset) * sizeof(I64_t), &chip_size);
        BG->start = (I64_t *)&G->pool[subgraph_no].pool[chip_size];
        int err = madvise((void *)G->pool[subgraph_no].pool, G->pool[subgraph_no].memsize, MADV_SEQUENTIAL);
        if (err == -1) perror("madvice seaquential update_degree_table");
      }
      OMP("omp barrier");

      I64_t j, ls, le;
      partial_range(BG->n, 0, lcores, coreid, &ls, &le);
      for (j = ls; j < le; ++j) {
        const I64_t bs = BG->start[j], be = BG->start[j+1], dg = be-bs, v = j+BG->offset;

        if (dg <= lower_limit) degree_table_code[v] = dg;
        else degree_table_code[v] = (dg >> log_c) + lower_limit + 1;

        degree_sum += dg;
        if (maxdg < dg) maxdg = dg;
        if (mindg > dg) mindg = dg;

        if (dg >= baseline_fully_onmem_edges) {
          l_onmem_degree_sum += dg;
        } else if (dg <= max_onmem_edges) {
          l_onmem_degree_sum += dg;
        } else {
          l_onmem_degree_sum += max_onmem_edges;
        }
      }
      OMP("omp barrier");

      if (coreid == 0) {
        drop_pagecaches_pool(&G->pool[subgraph_no]);
        unmap_pool(&G->pool[subgraph_no]);
      }
      OMP("omp barrier");
    }

    OMP("omp critical")
    if (degree_max < maxdg) degree_max = maxdg;
    OMP("omp critical")
    if (degree_min > mindg) degree_min = mindg;
    OMP("omp barrier");

    if (id == 0)
      for (int j=0; j < nodes; j++) ne_onm[j] = 0;
        OMP("omp barrier");

      SYNC_FETCH_AND_ADD(&ne_onm[nodeid], l_onmem_degree_sum);
    }

    printf("sum([d for d in degree_list]) is %lld (= #edges: %lld)\n", degree_sum, G->m);
    printf("min([d for d in degree_list]) is %lld\n", degree_min);
    printf("max([d for d in degree_list]) is %lld\n", degree_max);
    assert( degree_sum == G->m );



    close_files(DF_s);
    free_files(DF_s);

  }

  static int intdegreecmp(const void *a, const void *b) {
    const I64_t _a = *(const I64_t *)a;
    const I64_t _b = *(const I64_t *)b;
    const unsigned char _ia = degree_table_code[_a];
    const unsigned char _ib = degree_table_code[_b];
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

static void sort_adjacency_list_by_degree(struct graph_t *G, int subgraph_no) {
  I64_t duplicates = 0;

  const double t1 = get_seconds();
  OMP("omp parallel num_threads(get_numa_num_threads()) reduction(+:duplicates)") {
    int id = omp_get_thread_num();
    int threads = get_numa_num_threads();
    I64_t ls, le;
    struct subgraph_t *BG = &G->BG_list[subgraph_no];
    size_t offset = BG->start[0];
    partial_range(BG->n, 0, threads, id, &ls, &le);
    for (I64_t j = ls; j < le; ++j) {
      I64_t bs = BG->start[j], be = BG->start[j+1], dg = be-bs;
      if (dg) {
        I64_t *array = &BG->end[bs - offset];
        I64_t uniq_dg = uniq(array, dg, sizeof(I64_t), qsort, intdegreecmp);
        for (I64_t i = uniq_dg; i < dg; ++i) array[i] = -1; /* filled -1 */
        duplicates += dg - uniq_dg;
      }
    }

    OMP("omp barrier");
  }
  const double t2 = get_seconds();

  printf("found %lld duplicated edges (%.3f %%) (%.3f seconds)\n",
   duplicates, 1.0*duplicates/G->BG_list[subgraph_no].m, t2-t1);
}



static void load_subgraph(struct graph_t *G, int subgraph_no, I64_t n_offset, I64_t m_offset)
{
  int nodes = get_numa_online_nodes();
  int lgraphs = G->num_graphs / nodes;
  int target_graph = subgraph_no / lgraphs;

  struct dumpfiles_t *DF_s = init_dumpfile_info_graph("", 1, 1);
  open_files_readmode(DF_s);
  struct dumpfiles_t *DF_e = init_dumpfile_info_graph("", 0, 1);
  open_files_readmode(DF_e);


  //printf("inputfile = %s\n", DF_s->file_info_list[target_graph].fname);
  //printf("inputfile = %s\n", DF_e->file_info_list[target_graph].fname);

  // I64_t n_offset = 0;
  // I64_t m_offset = 0;
  // for (int k = target_graph*lgraphs; k < subgraph_no; k++) {
  //   n_offset += G->BG_list[k].n;
  //   m_offset += G->BG_list[k].m;
  // }

  lseek(DF_s->file_info_list[target_graph].fd, n_offset * sizeof(I64_t), SEEK_SET);
  read_large_size(DF_s->file_info_list[target_graph].fd,
    G->BG_list[subgraph_no].start,
    (G->BG_list[subgraph_no].n + 1) * sizeof(I64_t));
#if 0
  for (I64_t k = 0; k < G->BG_list[subgraph_no].n + 1; k++) {
    fprintf(stderr, "%lld\n", G->BG_list[subgraph_no].start[k]);
  }
#endif



  lseek(DF_e->file_info_list[target_graph].fd, m_offset * sizeof(I64_t), SEEK_SET);
  read_large_size(DF_e->file_info_list[target_graph].fd,
    G->BG_list[subgraph_no].end,
    (G->BG_list[subgraph_no].m) * sizeof(I64_t));
#if 0
  for (I64_t k = 0; k < G->BG_list[subgraph_no].m; k++) {
    printf("%lld\n", G->BG_list[subgraph_no].end[k]);
  }
#endif

  close_files(DF_s);
  close_files(DF_e);
  free_files(DF_s);
  free_files(DF_e);
}

static void write_subgraph(struct graph_t *G, int subgraph_no)
{
  int nodes = get_numa_online_nodes();
  int lgraphs = G->num_graphs / nodes;
  int target_graph = subgraph_no / lgraphs;

  struct dumpfiles_t *DF_s = init_dumpfile_info_graph("", 1, 1);
  open_files(DF_s);
  struct dumpfiles_t *DF_e = init_dumpfile_info_graph("", 0, 1);
  open_files(DF_e);


  I64_t n_offset = 0;
  I64_t m_offset = 0;
  for (int k = target_graph*lgraphs; k < subgraph_no; k++) {
    n_offset += G->BG_list[k].n;
    m_offset += G->BG_list[k].m;
  }

  int is_last = (subgraph_no % lgraphs) == (lgraphs - 1);
  size_t length = 0;
  if (is_last) length = G->BG_list[subgraph_no].n+1;
  else         length = G->BG_list[subgraph_no].n;
  lseek(DF_s->file_info_list[target_graph].fd, n_offset * sizeof(I64_t), SEEK_SET);
  write_large_size(DF_s->file_info_list[target_graph].fd,
    G->BG_list[subgraph_no].start,
    length * sizeof(I64_t));

  lseek(DF_e->file_info_list[target_graph].fd, m_offset * sizeof(I64_t), SEEK_SET);
  write_large_size(DF_e->file_info_list[target_graph].fd,
    G->BG_list[subgraph_no].end,
    (G->BG_list[subgraph_no].m) * sizeof(I64_t));

  close_files(DF_s);
  close_files(DF_e);
  free_files(DF_s);
  free_files(DF_e);

}

static void dump_degree_distribution(struct graph_t *G);

static void extract_duplicated_edges(struct graph_t *G) {

  assert( degree_table_code = (unsigned char *)calloc(G->n, sizeof(unsigned char)) );

  const double s_time1 = get_seconds();
  printf("updating degree table...\n");
  update_degree_table(G);
  printf("done [elapsed: %6.2fs]\n\n", get_seconds()-s_time1);


  int nodes = get_numa_online_nodes();
  int lgraphs = G->num_graphs / nodes;

  I64_t local_n_offset = 0;
  I64_t local_m_offset = 0;

  size_t offset = 0;
  for (int k = 0; k < G->num_graphs; k++) {
    printf("-- extract duplicated edges for subgraph [%d] --\n", k);
    allocate_subgraph(G, k);
    load_subgraph(G, k, local_n_offset, local_m_offset);
    struct subgraph_t *BG = &G->BG_list[k];
    if ((k + 1) % lgraphs == 0) {
      local_n_offset = 0;
      local_m_offset = 0;
    } else {
      local_n_offset += BG->n;
      local_m_offset += BG->m;
    }
    sort_adjacency_list_by_degree(G, k);

    /* Backward Graph */
    I64_t crr = 0;

    I64_t ost = BG->start[0];
    for (I64_t j = 0; j < BG->n; ++j) {
      const I64_t ls = BG->start[j], le = BG->start[j+1];
      BG->start[j] = crr + offset;
      for (I64_t w = ls; w < le; ++w) {
        I64_t v = BG->end[w - ost];
        if (v != -1) {
          BG->end[crr++] = v;
        }
      }
    }
    BG->start[BG->n] = crr + offset;
    BG->m = crr;

    if ((k + 1) % lgraphs == 0) {
      offset = 0;
    } else {
      offset += crr;
    }

    write_subgraph(G, k);

    lfree(G->pool[k]);

  }

  /* update # of edges */
  G->m = 0;
  for (int k = 0; k < G->num_graphs; ++k) {
    G->m += G->BG_list[k].m;
  }

  const double s_time2 = get_seconds();
  printf("updating degree table...\n");
  update_degree_table(G);
  printf("done [elapsed: %6.2fs]\n\n", get_seconds()-s_time2);


  const double e_time = get_seconds();
  printf("extract duplicated edges (#threads=1, %.3f seconds)\n", e_time - s_time1);

  /* dump degree */
  dump_degree_distribution(G);
  printf("free degree table\n");
  free(degree_table_code);

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
      fprintf(fp, "d %lld %d %d\n", i+1, degree_table_code[i], ilog2(degree_table_code[i]));
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
        SYNC_FETCH_AND_ADD(&deg[ degree_table_code[j] ], 1);
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

static void dump_subgraph_with_merging_files(struct graph_t *G, struct graph_t *G_full,
                                             int subgraph_no, I64_t offset){



  int lgraphs = (G->num_graphs / G_full->num_graphs);
  int target_no = subgraph_no / lgraphs;


  if (subgraph_no % lgraphs != 0) {
    OMP("omp parallel num_threads(get_numa_num_threads())") {
      int id = omp_get_thread_num();
      int threads = get_numa_num_threads();
      OMP("omp barrier");

      I64_t ls, le;
      struct subgraph_t *BG = &G->BG_list[subgraph_no];
      partial_range(BG->n+1, 0, threads, id, &ls, &le);
      for (I64_t i = ls; i < le; i++) {
        BG->start[i] += offset;
      }

      clear_affinity();
    }

  }

  dump_subgraph_mergingmode(&G->BG_list[subgraph_no],
    G->num_graphs, G_full->num_graphs,
    subgraph_no);

  if (subgraph_no == 0) {
    G_full->n = G->n;
    G_full->m = G->m;
    for (int k = 0; k < G_full->num_graphs; k++) {
      G_full->BG_list[k].n = 0;
      G_full->BG_list[k].m = 0;
    }
  }

  G_full->BG_list[target_no].n += G->BG_list[subgraph_no].n;
  G_full->BG_list[target_no].m += G->BG_list[subgraph_no].m;

}


/* ------------------------------------------------------------
 * dump graph
 * ------------------------------------------------------------ */
//  int dump_graph(const char *filename, struct graph_t *G) {
//   FILE *fp = fopen(filename, "w");
//   if (!fp) {
//     return 1;
//   }

//   fprintf(fp, "p sp %lld %lld\n", G->n, G->m);
//   for (int k = 0; k < G->num_graphs; ++k) {
//     struct subgraph_t *BG = &G->BG_list[k];
//     for (I64_t i = 0; i < BG->n; ++i) {
//       const I64_t w = i + BG->offset;
//       for (I64_t j = BG->start[i]; j < BG->start[i+1]; ++j) {
//         const I64_t v = BG->end[j];
//         fprintf(fp, "a %lld %lld 1\n", v+1, w+1);
//       }
//     }
//   }
//   fclose(fp);
//   return 0;
// }




/* ------------------------------------------------------------
 * free graph
 * ------------------------------------------------------------ */
//  void free_graph(struct graph_t *G) {
//   int k;
//   for (k = 0; k < G->num_graphs; ++k) {
//     lfree(G->pool[k]);
//   }
//   free(G->BG_list);
//   free(G->pool);
//   free(G);
// }

 static void free_graph_info(struct graph_t *G) {
  free(G->BG_list);
  free(G->pool);
  free(G);
}
/* ------------------------------------------------------------
 * free subgraph
 * ------------------------------------------------------------ */
// void free_subgraph(struct graph_t *G, int subgraph_no) {
//   lfree(G->pool[subgraph_no]);
// }











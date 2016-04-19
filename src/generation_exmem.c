/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version considerably modified by The GraphCREST Project
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
 * ------------------------------------------------------------------------ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "generation.h"
#include "dump.h"


/* ------------------------------------------------------------
 * graph_generation
 * ------------------------------------------------------------ */
static struct edgelist_t *allocate_edgelist_mmap(int scale, int edgefactor, int num_lists, I64_t nbfs,
                                                 struct dumpfiles_t *DF_E);
static I64_t generate_kron_edges(int scale, int edgefactor, struct edgelist_t *list);
static I64_t generate_rmat_edges(int scale, int edgefactor, struct edgelist_t *list);

struct edgelist_t *graph_generation(int scale, int edgefactor, int num_lists, I64_t nbfs) {
  struct edgelist_t *list = NULL;
  double t1, t2;

  /* initialize and open files for mmap*/
  struct dumpfiles_t *DF_E = NULL;
  DF_E = init_dumpfile_info_edgelist("", 0);
  open_files_new(DF_E);


  /* allocation with mmap */
  t1 = get_seconds();
  assert( list = allocate_edgelist_mmap(scale, edgefactor, num_lists, nbfs, DF_E) );
  t2 = get_seconds();
  printf("numa node local allocation takes %.3f seconds\n", t2-t1);

  /* generation */
  t1 = get_seconds();
  I64_t generated = 0;
  if (use_RMAT_generator) {
    assert( generated = generate_rmat_edges(scale, edgefactor, list) );
    assert( generated == list->num_edges );
  } else {
    assert( generated = generate_kron_edges(scale, edgefactor, list) );
    assert( generated == list->num_edges );
  }
  t2 = get_seconds();
  list->time = t2 - t1;
  printf("kronecker graph generation takes %.3f seconds\n", t2-t1);


#if STORE_BFS_SOURCES == 1
  dump_srcs_fwrite(list);
#endif

  printf("drop page caches and unmap pools for edgelist\n");
  int k;
  for (k = 0; k < list->num_lists; ++k) {
    drop_pagecaches_pool(&(list->pool[k]));
    unmap_pool(&(list->pool[k]));
  }
  close_files(DF_E);
  free_files(DF_E);


  return list;
}

static struct edgelist_t *allocate_edgelist_mmap(int scale, int edgefactor, int num_lists, I64_t nbfs,
                                                 struct dumpfiles_t *DF_E) {
  struct edgelist_t *list = NULL;
  assert( list = (struct edgelist_t *)calloc(1, sizeof(struct edgelist_t)) );

  list->num_nodes = 1ULL << scale;
  list->num_edges = list->num_nodes * edgefactor;
  list->num_lists = num_lists;
  assert( list->IJ_list = (struct IJ_list_t *)calloc(list->num_lists+1, sizeof(struct IJ_list_t)) );
  assert( list->pool    = (struct mempool_t *)calloc(list->num_lists+1, sizeof(struct mempool_t)) );

  list->numsrcs = nbfs;
  assert( list->srcs = (I64_t *)calloc(nbfs, sizeof(I64_t)) );

  int k;
  I64_t chunk = ROUNDUP( list->num_edges/list->num_lists, nbfs );
  for (k = 0; k < list->num_lists; ++k) {
    list->IJ_list[k].offset = chunk * k;
  }
  list->IJ_list[k].offset = list->num_edges;
  for (k = 0; k < list->num_lists; ++k) {
    list->IJ_list[k].length = list->IJ_list[k+1].offset - list->IJ_list[k].offset;
  }
  list->IJ_list[k].length = 0;

  /* mmap */
  for (k = 0; k < num_lists; ++k) {
    struct IJ_list_t *IJ_list = list->IJ_list;
    size_t sz = 0;
    sz += IJ_list[k].length * sizeof(struct packed_edge);
    sz  = ROUNDUP( sz, hugepage_size() );

    int fd = DF_E->file_info_list[k].fd;
    assert(fd >= 0);
    list->pool[k] = mmap_pool(sz, fd, 0, NULL);
    printf("[node%02d] E[%d]=[%.2e,%.2e] (%p) %.2f GB using mmap(no mbind : %s)\n",
           k, k,
           (double)(IJ_list[k].offset), (double)(IJ_list[k].offset+IJ_list[k].length),
           list->pool[k].pool, (double)(list->pool[k].memsize) / (1ULL<<30),
           DF_E->file_info_list[k].fname);
  }
  //force_pool_page_faults(list->pool);
  for (k = 0; k < num_lists; ++k) {
    list->IJ_list[k].edges = (struct packed_edge *)list->pool[k].pool;
  }

  return list;
}


//static struct edgelist_t *allocate_edgelist(int scale, int edgefactor, int num_lists, I64_t nbfs) {
//  struct edgelist_t *list = NULL;
//  assert( list = (struct edgelist_t *)calloc(1, sizeof(struct edgelist_t)) );
//
//  list->num_nodes = 1ULL << scale;
//  list->num_edges = list->num_nodes * edgefactor;
//  list->num_lists = num_lists;
//  assert( list->IJ_list = (struct IJ_list_t *)calloc(list->num_lists+1, sizeof(struct IJ_list_t)) );
//  assert( list->pool    = (struct mempool_t *)calloc(list->num_lists+1, sizeof(struct mempool_t)) );
//
//  list->numsrcs = nbfs;
//  assert( list->srcs = (I64_t *)calloc(nbfs, sizeof(I64_t)) );
//
//  int k;
//  I64_t chunk = ROUNDUP( list->num_edges/list->num_lists, nbfs );
//  for (k = 0; k < list->num_lists; ++k) {
//    list->IJ_list[k].offset = chunk * k;
//  }
//  list->IJ_list[k].offset = list->num_edges;
//  for (k = 0; k < list->num_lists; ++k) {
//    list->IJ_list[k].length = list->IJ_list[k+1].offset - list->IJ_list[k].offset;
//  }
//  list->IJ_list[k].length = 0;
//
//  /* mmap */
//  for (k = 0; k < num_lists; ++k) {
//    struct IJ_list_t *IJ_list = list->IJ_list;
//    size_t sz = 0;
//    sz += IJ_list[k].length * sizeof(struct packed_edge);
//    sz  = ROUNDUP( sz, hugepage_size() );
//    list->pool[k] = lmalloc(sz, k);
//    printf("[node%02d] E[%d]=[%.2e,%.2e] (%p) %.2f GB using mmap with mbind(MPOL_BIND:%8s)\n",
//           k, k,
//           (double)(IJ_list[k].offset), (double)(IJ_list[k].offset+IJ_list[k].length),
//           list->pool[k].pool, (double)(list->pool[k].memsize) / (1ULL<<30),
//           &num2bit((1ULL << k), 0)[56]);
//  }
//  force_pool_page_faults(list->pool);
//  for (k = 0; k < num_lists; ++k) {
//    list->IJ_list[k].edges = (struct packed_edge *)list->pool[k].pool;
//  }
//
//  return list;
//}

static I64_t generate_kron_edges(int scale, int edgefactor, struct edgelist_t *list) {

  struct packed_edge **IJ_list
  = (struct packed_edge **)CALLOCA(list->num_lists * sizeof(struct packed_edge *));
  int k;
  for (k = 0; k < list->num_lists; ++k) {
    assert( list->IJ_list[k].edges );
    IJ_list[k] = list->IJ_list[k].edges;
  }
  I64_t generated = make_edgelist(scale, edgefactor,
                                  list->num_lists, IJ_list, &list->numsrcs, list->srcs);

  printf("[%s] generate undirected %lld (i,j)-pairs"
         " (SCALE %d: n=%lld, m=%lld)\n", __FUNCTION__, generated,
         scale, list->num_nodes, list->num_edges);
  printf("[%s] generate %lld BFS sources\n", __FUNCTION__, list->numsrcs);
  assert( generated == list->num_edges );

  return generated;
}

static I64_t generate_rmat_edges(int scale, int edgefactor, struct edgelist_t *list) {

  struct packed_edge **IJ_list
  = (struct packed_edge **)CALLOCA(list->num_lists * sizeof(struct packed_edge *));
  int k;
  for (k = 0; k < list->num_lists; ++k) {
    assert( list->IJ_list[k].edges );
    IJ_list[k] = list->IJ_list[k].edges;
  }

#define A_PARAM 0.57
#define B_PARAM 0.19
#define C_PARAM 0.19

  double A=0.57, B=0.19, C=0.19;
  I64_t generated = rmat_edgelist(scale, edgefactor,
                                  list->num_lists, IJ_list,
                                  A, B, C,
                                  &list->numsrcs, list->srcs);

  printf("[%s] generate undirected RMAT %lld (i,j)-pairs"
         " (SCALE %d: n=%lld, m=%lld)\n", __FUNCTION__, generated,
         scale, list->num_nodes, list->num_edges);
  printf("[%s] generate %lld BFS sources\n", __FUNCTION__, list->numsrcs);
  assert( generated == list->num_edges );

  return generated;
}




/* ------------------------------------------------------------
 * dump edgelist
 * ------------------------------------------------------------ */
int dump_edgelist(const char *filename, struct edgelist_t *list) {
  FILE *fp = NULL;
  assert( fp = fopen(filename, "w") );
  if (!fp) {
    return 1;
  } else {
    int k;
    fprintf(fp, "p sp %lld %lld\n", list->num_nodes, list->num_edges);
    for (k = 0; k < list->num_lists; ++k) {
      I64_t e;
      for (e = 0; e < list->IJ_list[k].length; ++e) {
        const I64_t v = get_v0_from_edge(&list->IJ_list[k].edges[e]);
        const I64_t w = get_v1_from_edge(&list->IJ_list[k].edges[e]);
        fprintf(fp, "a %lld %lld 1\n", v+1, w+1);
      }
    }
    fclose(fp);
    return 0;
  }
}




/* ------------------------------------------------------------
 * free edge list
 * ------------------------------------------------------------ */
void free_edgelist(struct edgelist_t *list) {
  int k;
  for (k = 0; k < list->num_lists; ++k) {
    lfree(list->pool[k]);
  }
  free(list->IJ_list);
  free(list->pool);
  free(list->srcs);
  free(list);
}

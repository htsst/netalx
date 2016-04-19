/* Copyright (C) 2009-2010 The Trustees of Indiana University.             */
/*                                                                         */
/* Use, modification and distribution is subject to the Boost Software     */
/* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at */
/* http://www.boost.org/LICENSE_1_0.txt)                                   */
/*                                                                         */
/*  Authors: Jeremiah Willcock                                             */
/*           Andrew Lumsdaine                                              */

#ifndef GRAPH_GENERATOR_H
#define GRAPH_GENERATOR_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "user_settings.h"
#include "ulibc.h"
#include "defs.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#define NBFS_max 64

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GENERATOR_USE_PACKED_EDGE_TYPE

  typedef struct packed_edge {
    uint32_t v0_low;
    uint32_t v1_low;
    uint32_t high; /* v1 in high half, v0 in low half */
  } packed_edge;

  static inline int64_t get_v0_from_edge(const packed_edge* p) {
    return (p->v0_low | ((int64_t)((int16_t)(p->high & 0xFFFF)) << 32));
  }

  static inline int64_t get_v1_from_edge(const packed_edge* p) {
    return (p->v1_low | ((int64_t)((int16_t)(p->high >> 16)) << 32));
  }

  static inline void write_edge(packed_edge* p, int64_t v0, int64_t v1) {
    p->v0_low = (uint32_t)v0;
    p->v1_low = (uint32_t)v1;
    p->high = ((v0 >> 32) & 0xFFFF) | (((v1 >> 32) & 0xFFFF) << 16);
  }

#else

  typedef struct packed_edge {
    int64_t v0;
    int64_t v1;
  } packed_edge;

  static inline int64_t get_v0_from_edge(const packed_edge* p) {
    return p->v0;
  }

  static inline int64_t get_v1_from_edge(const packed_edge* p) {
    return p->v1;
  }

  static inline void write_edge(packed_edge* p, int64_t v0, int64_t v1) {
    p->v0 = v0;
    p->v1 = v1;
  }

#endif

  /* Generate a range of edges (from start_edge to end_edge of the total graph),
   * writing into elements [0, end_edge - start_edge) of the edges array.  This
   * code is parallel on OpenMP and XMT; it must be used with
   * separately-implemented SPMD parallelism for MPI. */
  void generate_kronecker_range(const uint_fast32_t seed[5] /* All values in [0, 2^31 - 1) */,
				int logN /* In base 2 */,
				/* Indices (in [0, M)) for the edges to generate */
				int64_t start_edge, int64_t end_edge,
				packed_edge* edges /* Size >= end_edge - start_edge */
				);
  
  /* implemented by Y.Yasui. */
  int64_t generate_kronecker_edges(int verbose, const uint_fast32_t seed[5], int logN,
				   int64_t start_edge, int64_t end_edge, packed_edge *edges);
  I64_t make_edgelist(int scale, int edgefactor,
		      int num_lists, struct packed_edge **edge_list,
		      I64_t *nbfs_ptr, I64_t *bfs_root_ptr);
  I64_t rmat_edgelist(int scale, int edgefactor,
		      int num_lists, struct packed_edge **edge_list,
		      double A, double B, double C,
		      I64_t *nbfs_ptr, I64_t *bfs_root_ptr);
  
  
#ifdef __cplusplus
}
#endif

#endif /* GRAPH_GENERATOR_H */

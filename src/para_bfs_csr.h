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

#ifndef PARA_BFS_CSR_H
#define PARA_BFS_CSR_H

#include "ulibc.h"
#include "mempol.h"
#include "defs.h"
#include "atomic.h"

struct stat_t {
  double bfs_time;
  I64_t trav_edges;
};

struct bfs_local_t {
  I64_t n, range, offset;
  I64_t bit_n, bit_range, bit_offset;

  /* local */
  I64_t local_queue_size;
  I64_t *local_queue[MAX_CPUS];	/* THREAD_LQ_SIZE */

  /* bitmaps */
  UL_t  *visited;		/* bit(range) */
  UL_t  *neighbors;		/* bit(range) */
  UL_t  *frontier;		/* bit(n) */

  /* local tree, shared */
  I64_t *tree;			/* n */
  I64_t *queue;			/* n */

  int *hops;

  /* edgelist buffer */
  I64_t *edgebuf;		/* range */

  /* shared counter */
  I64_t *queue_count;
  I64_t *queue_offset;
};

struct bfs_t {
  int num_locals;
  struct bfs_local_t *bfs_local;
  struct mempool_t *pool;
};

extern struct stat_t *parallel_breadth_first_search(struct graph_t *G, struct edgelist_t *list);
extern void free_stat(struct stat_t *stat);

#endif /* PARA_BFS_CSR_H */

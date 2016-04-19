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

#ifndef GENERATION_H
#define GENERATION_H

#include "ulibc.h"
#include "mempol.h"
#include "defs.h"
#include "graph_generator.h"

struct IJ_list_t {
  I64_t offset;
  I64_t length;
  struct packed_edge *edges;
};

struct edgelist_t {
  /* edgelist */
  I64_t num_nodes;
  I64_t num_edges;
  int num_lists;
  struct IJ_list_t *IJ_list;
  double time;

  /* BFS root */
  I64_t numsrcs;
  I64_t *srcs;
  struct mempool_t *pool;
};

extern struct edgelist_t *graph_generation(int scale, int edgefactor, int num_lists, I64_t nbfs);
extern int dump_edgelist(const char *filename, struct edgelist_t *list);
extern void free_edgelist(struct edgelist_t *list);

#endif /* GENERATION_H */

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

#ifndef CONSTRUCTION_H
#define CONSTRUCTION_H

#include "ulibc.h"
#include "mempol.h"
#include "defs.h"
#include "atomic.h"

struct subgraph_t {
  I64_t n;
  I64_t m;
  I64_t offset;
  I64_t *start;
  I64_t *end;
};

struct graph_t {
  I64_t n;
  I64_t m;
  I64_t chunk;
  int num_graphs;
  struct subgraph_t *FG_list;
  struct subgraph_t *BG_list;
  struct mempool_t *pool;
};

extern struct graph_t *graph_construction(struct edgelist_t *list);
extern int dump_graph(const char *filename, struct graph_t *G);
extern void free_graph(struct graph_t *G);

#endif /* CONSTRUCTION_H */

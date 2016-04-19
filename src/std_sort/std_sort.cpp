/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
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

#include <iostream>     // std::cout
#include <algorithm>    // std::sort
#include <vector>       // std::vector

#include "std_sort.h"

#include "defs.h"
#include "generation.h"
#include "dump.h"

I64_t *degree_table;

class packed_edge_cmp {
public:
  bool operator() (const packed_edge& Ea, const packed_edge& Eb) {

    I64_t v0a = get_v0_from_edge(&Ea);
    I64_t v0b = get_v0_from_edge(&Eb);
    /* ascending order */
    if (v0a < v0b) return 1;
    if (v0a > v0b) return 0;



    I64_t v1a = get_v1_from_edge(&Ea);
    I64_t v1b = get_v1_from_edge(&Eb);
    /* descending order */
    if (degree_table[v1a] > degree_table[v1b]) return 1;
    if (degree_table[v1a] < degree_table[v1b]) return 0;

    /* ascending order */
    if (v1a < v1b) return 1;
    if (v1a > v1b) return 0;

    return 0;
  }
};

void std_sort (unsigned char *src_raw, size_t length, I64_t *dgr_tbl) {

  // ???: vector type dose not work.
  //std::vector<packed_edge> myvector (src, src+length);

  packed_edge *src = (packed_edge *)src_raw;

  degree_table = dgr_tbl;

  std::sort(src, src + length, packed_edge_cmp());

}

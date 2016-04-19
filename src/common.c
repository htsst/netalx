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

#include <assert.h>

#include "ulibc.h"
#include "defs.h"
#include "atomic.h"
#include "graph_generator.h"
#include "mempol.h"


double get_seconds(void) {
  return get_msecs() * 1e-3;
}


void partial_range(I64_t size, I64_t offset, I64_t np, I64_t id,
		   I64_t *loop_start, I64_t *loop_end) {
  const I64_t qt = size / np;
  const I64_t rm = size - qt * np;
  *loop_start = qt * (id+0) + (id+0 < rm ? id+0 : rm) + offset;
  *loop_end   = qt * (id+1) + (id+1 < rm ? id+1 : rm) + offset;
}


void force_page_faults_single(unsigned char *x, size_t sz) {
  const size_t pgsz = page_size();
  size_t j;
  for (j = 0; j < sz; j += pgsz) {
    x[j] = -1;
  }
}


void force_page_faults(unsigned char *x, size_t sz, int coreid, int lcores) {
  const I64_t pgsz = page_size();
  I64_t k, ls, le;
  partial_range(sz, 0, lcores, coreid, &ls, &le);
  for (k = ls; k < le; k += pgsz) {
    x[k] = -1;
  }
}


void force_pool_page_faults(struct mempool_t *pool_list) {
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");
    struct mempool_t *pool = &pool_list[nodeid];
    force_page_faults(pool->pool, pool->memsize, coreid, lcores);
    clear_affinity();
  }
}


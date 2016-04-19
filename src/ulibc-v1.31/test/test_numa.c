/* ---------------------------------------------------------------------- *
 *
 * This is part of NETAL.
 *
 * Copyright (C) 2011-2015 Yuichiro Yasui
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
 * along with NETAL; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ulibc.h"

int main(void) {
  if ( enable_affinity() ) {
    printf("affinity is enable.\n");
  } else {
    printf("affinity is disable.\n");
  }
  printf("\n");

  printf("enable hyper threading\n");
  printf("#procs   is %d\n", get_numa_nprocs());
  printf("#nodes   is %d\n", get_numa_nnodes());
  printf("#cores   is %d\n", get_numa_ncores());
  print_mapping(stdout);

  printf("disable hyper threading\n");
  disable_hyper_threading_cores();
  printf("#procs   is %d\n", get_numa_nprocs());
  printf("#nodes   is %d\n", get_numa_nnodes());
  printf("#cores   is %d\n", get_numa_ncores());
  print_mapping(stdout);

  return 0;
}

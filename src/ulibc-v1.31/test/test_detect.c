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
#include <string.h>
#include "ulibc.h"

int main(int argc, char **argv) {
  printf("this OS  is L:%d, M:%d, W:%d\n", is_linux(), is_macosx(), is_windows());
  printf("CPU name is %s\n", cpuname());
  printf("CPU freq is %.3f MHz\n", mfreq());
  printf("#cores   is %d (physical: %d)\n", get_num_cores(), get_numa_physical_ncores());
  printf("RAM size is %.2f GB\n", (double)get_ramsize()/(1<<30));

  if ( argc != 2 || strcmp(argv[1], "-v") || !is_linux() ) {
    printf("\n%s -v: show /proc/meminfo datas.\n", ubasename(argv[0]));
  } else {
    int i;
    for (i = MF_MEMTOTAL; i <= MF_HUGEPAGESIZE; ++i) {
      printf("/proc/meminfo:\t%-15s\t%ld\n", get_meminfo_name(i), get_meminfo(i));
    }
  }

  return 0;
}

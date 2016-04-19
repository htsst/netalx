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
#include <time.h>
#include "ulibc.h"

int main(void) {
  printf("      pagesize is %lu\n", page_size());
  printf("large pagesize is %lu\n", hugepage_size());
  printf("\n");
  printf("  total memory is %5.2f GB\n", (double)total_memsize() / (1<<30));
  printf("  alloc memory is %5.2f GB\n", (double)alloc_memsize() / (1<<30));
  printf("   free memory is %5.2f GB\n", (double) free_memsize() / (1<<30));
  printf("\n");
  printf("total hugepage is %5.2f GB, %4lu pages\n",
	 (double)total_hugepages() * hugepage_size() / (1<<30), total_hugepages());
  printf("alloc hugepage is %5.2f GB, %4lu pages\n",
	 (double)alloc_hugepages() * hugepage_size() / (1<<30), alloc_hugepages());
  printf(" free hugepage is %5.2f GB, %4lu pages\n",
	 (double) free_hugepages() * hugepage_size() / (1<<30),  free_hugepages());
  printf("\n");
  return 0;
}

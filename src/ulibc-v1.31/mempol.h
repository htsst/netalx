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

#ifndef MEMPOL_H
#define MEMPOL_H

#ifdef __linux__
#include <syscall.h>
#include <sys/sysinfo.h>
int sysinfo(struct sysinfo *info);
extern long int syscall(long int __sysno, ...);
#endif
#include <sys/mman.h>
static inline int mbind(void *addr, unsigned long len, int mode,
			unsigned long *nodemask, unsigned long maxnode, unsigned flags) {
#ifdef __linux__
  return syscall(SYS_mbind, addr, len, mode, nodemask, maxnode, flags);
#else
  addr = addr;
  len = len;
  mode = mode;
  nodemask = nodemask;
  maxnode = maxnode;
  mode = mode;
  flags = flags;
  return 0;
#endif
}
static inline
int set_mempolicy(int mode, const unsigned long *addr, unsigned long flag) {
#ifdef __linux__
  return syscall(SYS_set_mempolicy, mode, addr, flag);
#else
  mode = mode;
  addr = addr;
  flag = flag;
  return 0;
#endif
}
static inline
int get_mempolicy(int *mode, unsigned long *nodemask, unsigned long maxnode,
		  unsigned long addr, unsigned long flags) {
#ifdef __linux__
  return syscall(SYS_get_mempolicy, mode, nodemask, maxnode, addr, flags);
#else
  addr = addr;
  mode = mode;
  nodemask = nodemask;
  maxnode = maxnode;
  mode = mode;
  flags = flags;
  return 0;
#endif
}
#ifndef MPOL_MF_STRICT
#define MPOL_MF_STRICT   (1<<0)	/* Verify existing pages in the mapping */
#endif
#ifndef MPOL_MF_MOVE
#define MPOL_MF_MOVE     (1<<1)	/* Move pages owned by this process to conform to mapping */
#endif
#ifndef MPOL_MF_MOVE_ALL
#define MPOL_MF_MOVE_ALL (1<<2)	/* Move every page to conform to mapping */
#endif
enum {
  MPOL_DEFAULT = 0,
  MPOL_PREFERRED = 1,
  MPOL_BIND = 2,
  MPOL_INTERLEAVE = 3,
  MPOL_MAX = 4,       /* always last member of enum */
};


/* for mmap */
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Flags for get_mempolicy */
#ifndef MPOL_F_NODE
#define MPOL_F_NODE         (1<<0)  /* return next IL mode instead of node mask */
#endif
#ifndef MPOL_F_ADDR
#define MPOL_F_ADDR         (1<<1)  /* look up vma using address */
#endif
#ifndef MPOL_F_MEMS_ALLOWED
#define MPOL_F_MEMS_ALLOWED (1<<2)  /* return allowed memories */
#endif

#ifndef PROTECTION
#define PROTECTION (PROT_READ | PROT_WRITE)
#endif
#ifndef MAP_ANONYMOUS
#  ifdef  MAP_ANON
#  define MAP_ANONYMOUS MAP_ANON
#  else
#  define MAP_ANONYMOUS 0
#  endif
#endif
#ifndef MAP_HUGETLB
#  ifdef ALLOC_HUGEPAGE
#  define MAP_HUGETLB 0x40
#  else
#  define MAP_HUGETLB 0
#  endif
#endif

#ifdef __ia64__
#  define ADDR (void *)(0x8000000000000000UL)
#  define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)
#else
#  define ADDR (void *)(0x0UL)
#  define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS)
#endif

#endif /* MEMPOL_H */

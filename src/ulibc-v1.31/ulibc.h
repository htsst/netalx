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

#ifndef ULIBC_H
#define ULIBC_H

#ifndef ULIBC_VERSION
#define ULIBC_VERSION "ULIBC (version 1.31)"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#if defined (__cplusplus)
extern "C" {
#endif
  /* ------------------------------------------------------------
   * string.c
   * ------------------------------------------------------------ */
  extern unsigned long long bit2num(char *bit);
  extern char *num2bit(unsigned long long x, int is_space);
  extern char *ubasename(const char *string);
  extern char *extract_suffix(char *org, char *suffix);
  extern long long getenvi(char *env, long long def);
  extern double getenvf(char *env, double def);
  extern char *mstrcat(char *first, ...);
  extern int strcatfmt(char *string, const char *format, ...);
  extern void error(const char *format, ... );
  enum {
    UCL_DEFAULT,
    UCL_BLACK,      UCL_RED,     UCL_GREEN,    UCL_BROWN, UCL_BLUE,
    UCL_MAGENTA,    UCL_CYAN,    UCL_GRAY,
    UCL_BG_BLACK,   UCL_BG_RED,  UCL_BG_GREEN, UCL_BG_BROWN, UCL_BG_BLUE,
    UCL_BG_MAGENTA, UCL_BG_CYAN, UCL_BG_GRAY,
    UCL_COLORS
  };
  extern int cl_fprintf(int color, FILE *stream, const char *format, ...);
  extern int cl_sprintf(int color, char *buffer, const char *format, ...);

  /* ------------------------------------------------------------
   * detect.c
   * ------------------------------------------------------------ */
  extern int is_linux(void);
  extern int is_macosx(void);
  extern int is_windows(void);
  extern int get_apicid(void);
  extern char *cpuname(void);
  extern double mfreq(void);
  extern int get_num_cores(void);
  extern size_t get_ramsize(void);
  enum {
    MF_MEMTOTAL, MF_MEMFREE,  MF_BUFFERS,   MF_CACHED, MF_SWAPCACHED,
    MF_ACTIVE,   MF_INACTIVE,
    MF_HIGHTOTAL, MF_HIGHFREE, MF_LOWTOTAL, MF_LOWFREE,
    MF_SWAPTOTAL, MF_SWAPFREE,
    MF_DIRTY,     MF_WRITEBACK,
    MF_ANONPAGES, MF_MAPPED, MF_SLAB, MF_PAGETABLES,
    MF_NFS_UNSTABLE, MF_BOUNCE,
    MF_COMMITLIMIT, MF_COMMITTED_AS,
    MF_VMALLOCTOTAL, MF_VMALLOCUSED, MF_VMALLOCCHUNK,
    MF_HUGEPAGES_TOTAL, MF_HUGEPAGES_FREE, MF_HUGEPAGES_RSVD, MF_HUGEPAGESIZE,
  };
  extern const char *get_meminfo_name(int flag);
  extern size_t get_meminfo(int flag);

  /* ------------------------------------------------------------
   * stdlib.c
   * ------------------------------------------------------------ */
  extern int intlog(int base, unsigned long long int n);
  extern long long int intsqrt(long long int n);
  extern double fsqrt(const double x);
  extern void print_prbar(double pcent, char *prefix, char symb, char lf);
  extern char *set_nsymb(char *s, char symb, int n);
  extern long *rand_perm(long n, long *seq);
  extern double *normalize(long n, double *X, double l, double h);

  /* ------------------------------------------------------------
   * timer.c
   * ------------------------------------------------------------ */
  extern double get_msecs(void);
  extern unsigned long long get_usecs(void);
  extern int sync_counter(long long *counter, long long x);
  struct ivtimer_t {
    double start;
    double prev;
    double stop;
  };
  extern double tm_start(struct ivtimer_t *tm);
  extern double tm_stop(struct ivtimer_t *tm);
  extern double tm_time(struct ivtimer_t *tm);
  extern double tm_elapsed(struct ivtimer_t *tm);
  extern int tm_interval(struct ivtimer_t *tm, double delta, double bound);

  /* ------------------------------------------------------------
   * sort.c
   * ------------------------------------------------------------ */
  extern int hsort(void *base, size_t nmemb, size_t size,
		   int (*compar)(const void *, const void *));
  extern int msort(void *base, size_t nmemb, size_t size,
		   int (*compar)(const void *, const void *));
  extern int krqsort(void *base, size_t nmemb, size_t size,
		     int (*compar)(const void *, const void *));
  extern int isort(void *base, size_t nmemb, size_t size,
		   int (*compar)(const void *, const void *));
  extern int bsort(void *base, size_t nmemb, size_t size,
		   long (*getkey)(const void *));
  extern int ipbsort(void *base, size_t nmemb, size_t size,
		     long (*getkey)(const void *));
  extern size_t uniq(void *base, size_t nmemb, size_t size,
		     void (*sort)(void *base, size_t nmemb, size_t size,
				  int(*compar)(const void *, const void *)),
		     int (*compar)(const void *, const void *));

  /* ------------------------------------------------------------
   * numa.c
   * ------------------------------------------------------------ */
  extern void construct_affinity(void) __attribute__((constructor));
  extern int enable_affinity(void);
  extern void pinned(int level, int id);
  extern void set_affinity(int coreid);
  extern void clear_affinity(void);

  extern int get_numa_nprocs(void);
  extern int get_numa_nnodes(void);
  extern int get_numa_ncores(void);
  extern int get_numa_physical_ncores(void);

  extern int get_numa_procid(int tid);
  extern int get_numa_apicid(int tid);
  extern int get_numa_nodeid(int tid);
  extern int get_numa_coreid(int tid);
  extern int get_numa_vircoreid(int tid);
  extern int get_numa_is_root(int tid);

  extern int set_numa_nthreads(int threads);
  extern int get_numa_nthreads(void);
  extern int get_numa_mapping_strategy(void);

  extern void disable_hyper_threading_cores(void);
  extern int set_numa_mapping_strategy(int strategy);
  extern int get_numa_mapping_strategy(void);
  extern int set_numa_num_threads(int threads);
  extern int get_numa_num_threads(void);
  extern int get_numa_online_nodes(void);
  extern int get_numa_online_cores(int nodeid);

  extern void print_mapping(FILE *fp);

  struct mempool_t {
    int nodeid;
    unsigned char *pool;
    size_t memsize;
  };
  extern struct mempool_t lmalloc(size_t sz, int nodeid);
  extern struct mempool_t anonpage_alloc(size_t sz);
  extern void lfree(struct mempool_t pool);

  /* ------------------------------------------------------------
   * malloc.c
   * ------------------------------------------------------------ */
#ifndef MEMINFO_FILE
#define MEMINFO_FILE "/proc/meminfo"
#endif

  extern void __attribute__ ((constructor)) umalloc_constructor(void);

  /* size */
  extern size_t page_size(void);
  extern size_t hugepage_size(void);
  extern size_t total_memsize(void);
  extern size_t alloc_memsize(void);
  extern size_t  free_memsize(void);
  extern size_t total_hugepages(void);
  extern size_t alloc_hugepages(void);
  extern size_t  free_hugepages(void);
#if defined (__cplusplus)
}
#endif
#endif /* ULIBC_H */

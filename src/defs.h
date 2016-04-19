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

#ifndef DEFS_H
#define DEFS_H

#define VERSION "Parallel Breadth-First Search Benchmark version 5.00"
#define LOGFILE "graph500_v500_log.txt"

#define I8_t    signed char
#define U8_t  unsigned char
#define I64_t   signed long long int
#define U64_t unsigned long long int
#define U64_MAX ((U64_t)(~0))
#define I64_MAX ((I64_t)(U64_MAX >> 1))

#define LINEMAX     256
#define MAX_THREADS 256
#define MAX_NODES    16
#define MAX_CPUS    256

#ifndef THREAD_LQ_SIZE
#define THREAD_LQ_SIZE (long long)(1ULL<<14)
#endif

#ifndef USE_HYBRID_AFFINITY
#define USE_HYBRID_AFFINITY 0
#endif

#ifndef USE_POOL_MEMORY
#define USE_POOL_MEMORY 0
#endif

#ifndef USE_REDUCE_ADJACENCY
#define USE_REDUCE_ADJACENCY 1
#endif

#ifndef PROFILE
#define PROFILE 1
#endif

#ifndef SKIP_EVERY_VALIDATION
#define SKIP_EVERY_VALIDATION 0
#endif

#ifndef STRDUP
#  define STRDUP(x) strcpy( alloca(strlen(x)+1), x )
#endif
#ifndef ROUNDUP
#  define ROUNDUP(x,a) (((x)+(a)-1) & ~((a)-1))
#endif

#if defined(_OPENMP)
#  define       OMP(x) _Pragma(x)
#  define  INITLOCK(x) omp_init_lock(x)
#  define   SETLOCK(x) omp_set_lock(x)
#  define UNSETLOCK(x) omp_unset_lock(x)
#  define  TESTLOCK(x) omp_test_lock(x)
#  define   DELLOCK(x) omp_destroy_lock(x)
#  include <omp.h>
#else
#  define       OMP(x)
#  define  INITLOCK(x)
#  define   SETLOCK(x)
#  define UNSETLOCK(x)
#  define  TESTLOCK(x) (1)
#  define   DELLOCK(x)
static int  omp_get_thread_num (void) { return 0; }
static int  omp_get_num_threads(void) { return 1; }
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

#define CALLOCA(x) ( memset(alloca(x), 0x00, x) )

extern double generation_time;
extern double construction_time;
extern const double kron_param_A;
extern const double kron_param_B;
extern const double kron_param_C;
extern const double kron_param_D;

extern int SCALE;
extern int edgefactor;
extern int use_RMAT_generator;
extern int enable_energy_loop;
extern int verbose;

extern int use_parameter_tuning;
extern int alpha_start;
extern int alpha_end;
extern int  beta_start;
extern int  beta_end;

extern char *message;
extern int ALPHA_param;
extern int BETA_param;

extern const char *version(void);
extern double get_seconds(void);
extern void partial_range(I64_t size, I64_t offset, I64_t np, I64_t id, I64_t *start, I64_t *end);
extern void force_page_faults_single(unsigned char *x, size_t sz);
extern void force_page_faults(unsigned char *x, size_t sz, int coreid, int lcores);
extern void force_pool_page_faults(struct mempool_t *pool_list);

#endif /* DEFS_H */

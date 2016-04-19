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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <assert.h>

#if defined(_OPENMP)
# define OMP(x) _Pragma(x)
# include <omp.h>
#else
# define OMP(x)
static int omp_in_parallel    (void) { return 0; }
static int omp_get_thread_num (void) { return 0; }
#endif

#include "ulibc.h"
#include "common.h"
#include "mempol.h"


struct cpuinfo_t {
  int id, apic, node, core, vircore;
};

struct sysdev_table_t {
  int enable;
  int nprocs;
  int nnodes;
  int ncores;
  struct cpuinfo_t cpuinfo[MAX_CPUS];
};

/* -------------------------------------------------------
 * |       core-major mapping |       node-major mapping |
 * -------------------------------------------------------
 * |       core0 core1 core2  |        core0 core1 core2 |
 * | node0     0     2     4  |  node0     0     1     2 |
 * | node1     1     3     5  |  node1     3     4     5 |
 * ------------------------------------------------------- */
enum {
  CORE_MAJOR_MAPPING = 0x00,
  NODE_MAJOR_MAPPING = 0x01,
};

struct mapping_table_t {
  int mapping_strategy;
  int cpumap[MAX_CPUS];
  int is_root[MAX_CPUS];
  unsigned long long interleave;
};

struct online_table_t {
  int nthreads;
  int online_nodes;
  int online_cores[MAX_NODES];
};




int __verbose = 0;
#ifdef __linux__
static cpu_set_t __default_cpu_set;
#endif
static struct mapping_table_t __mapping_table;
static struct  online_table_t  __online_table;
static struct  sysdev_table_t  __sysdev_table;




/* ------------------------------------------------------------
 * constructor
 * ------------------------------------------------------------ */
static struct  sysdev_table_t initialize_sysdev_table (int use_hyper_threading);
static struct mapping_table_t initialize_mapping_table(int mapping_strategy);
static struct  online_table_t initialize_online_table (int threads);
static int numa_apicid(int coreid);
static int get_cpuinfo(struct cpuinfo_t *cpuinfo);
static int  cpuid_comp(const void *a, const void *b);
static int htcore_comp(const void *a, const void *b);

void construct_affinity(void) {
#ifdef __linux__
  assert( !sched_getaffinity((pid_t)0, sizeof(cpu_set_t), &__default_cpu_set) );
#endif
  __sysdev_table  = initialize_sysdev_table(1);
  __mapping_table = initialize_mapping_table(CORE_MAJOR_MAPPING);
  __online_table  = initialize_online_table(__sysdev_table.nprocs);
}


/* ------------------------------------------------------------
 * interface functions
 * ------------------------------------------------------------ */
int enable_affinity(void) {
  return __sysdev_table.enable;
}


void pinned(int level, int id) {
  int nodeid = get_numa_nodeid(id);

#ifdef __linux__
  if (level == 0) {
    /* one-thread to one-core */
    set_affinity( get_numa_procid(id) );
    if ( enable_affinity() ) {
      assert( get_numa_apicid(id) == get_apicid() );
    }
  } else if (level == 1) {
    /* physical-cores: set one-thread to one-core */
    /* HT-cores      : set HT-threads to NUMA cores */
    if ( id < get_numa_physical_ncores() ) {
      set_affinity( get_numa_procid(id) );
      if ( enable_affinity() ) {
	assert( get_numa_apicid(id) == get_apicid() );
      }
    } else {
      int u;
      cpu_set_t mask;
      CPU_ZERO(&mask);
      for (u = 0; u < get_numa_num_threads(); ++u) {
	if ( get_numa_nodeid(u) == nodeid ) {
	  CPU_SET(get_numa_procid(u), &mask);
	}
      }
      assert( !sched_setaffinity((pid_t)0, sizeof(cpu_set_t), &mask) );
    }
  } else if (level == 2) {
    /* set threads to NUMA cores */
    cpu_set_t mask;
    CPU_ZERO(&mask);
    int u, physid = get_numa_coreid(id) % get_numa_ncores();
    for (u = 0; u < get_numa_num_threads(); ++u) {
      if ( ( get_numa_nodeid(u) == nodeid ) &&
	   ( get_numa_coreid(u) % get_numa_ncores() == physid ) ) {
	CPU_SET(get_numa_procid(u), &mask);
      }
    }
    assert( !sched_setaffinity((pid_t)0, sizeof(cpu_set_t), &mask) );
  } else {
    /* do nothing */
  }
#else
  level = level;
  id = id;
  nodeid = nodeid;
#endif
}



void set_affinity(int coreid) {
  if (0 <= coreid && coreid < MAX_CPUS) {
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(coreid, &mask);
    assert( !sched_setaffinity((pid_t)0, sizeof(cpu_set_t), &mask) );
#endif
  }
}
void clear_affinity(void) {
#ifdef __linux__
  assert( !sched_setaffinity((pid_t)0, sizeof(cpu_set_t), &__default_cpu_set) );
#endif
}


/* /sys/dev/system */
int get_numa_nprocs(void) {
  return __sysdev_table.nprocs;
}
int get_numa_nnodes(void) {
  return __sysdev_table.nnodes;
}
int get_numa_ncores(void) {
  return __sysdev_table.ncores;
}

#if defined(__FreeBSD__) || defined(__APPLE__)
#   include <sys/sysctl.h>
#endif
int get_numa_physical_ncores(void) {
  static int num_cpus = 0;
  if (!num_cpus) {
    struct cpuinfo_t cpuinfo[MAX_CPUS];
    num_cpus = get_cpuinfo(cpuinfo);
    if (num_cpus) {
      num_cpus = uniq(cpuinfo, num_cpus, sizeof(struct cpuinfo_t), qsort, htcore_comp);
    }
#if defined(__APPLE__)
    size_t len = sizeof(int);
    assert( sysctlbyname("hw.physicalcpu", &num_cpus, &len, NULL, 0) >= 0 );
#endif
  }
  return num_cpus;
}

int get_numa_procid(int tid) {
  return (__sysdev_table.enable) ? __mapping_table.cpumap[tid] : tid;
}
int get_numa_apicid(int tid) {
  return ( (__sysdev_table.enable) ?
	   __sysdev_table.cpuinfo[ __mapping_table.cpumap[tid] ].apic : 0 );
}
int get_numa_nodeid(int tid) {
  return ( (__sysdev_table.enable) ?
	   __sysdev_table.cpuinfo[ __mapping_table.cpumap[tid] ].node : 0 );
}
int get_numa_coreid(int tid) {
  return ( (__sysdev_table.enable) ?
	   __sysdev_table.cpuinfo[ __mapping_table.cpumap[tid] ].core : tid );
}
int get_numa_vircoreid(int tid) {
  return ( (__sysdev_table.enable) ?
	   __sysdev_table.cpuinfo[ __mapping_table.cpumap[tid] ].vircore : tid );
}
int get_numa_is_root(int tid) {
  return ( (__sysdev_table.enable) ?
	   __mapping_table.is_root[ __mapping_table.cpumap[tid] ] : 0 );
}


void disable_hyper_threading_cores(void) {
  __sysdev_table = initialize_sysdev_table(0);
  __mapping_table = initialize_mapping_table(__mapping_table.mapping_strategy);
  __online_table = initialize_online_table(__online_table.nthreads);
}
int set_numa_mapping_strategy(int strategy) {
  int threads = __online_table.nthreads;
  __mapping_table = initialize_mapping_table(strategy);
  __online_table = initialize_online_table(threads);
  return __mapping_table.mapping_strategy;
}
int get_numa_mapping_strategy(void) {
  return __mapping_table.mapping_strategy;
}
int set_numa_num_threads(int threads) {
  __online_table = initialize_online_table(threads);
  return __online_table.nthreads;
}
int get_numa_num_threads(void) {
  return __online_table.nthreads;
}
int get_numa_online_nodes(void) {
  return __online_table.online_nodes;
}
int get_numa_online_cores(int nodeid) {
  return __online_table.online_cores[nodeid];
}

void print_mapping(FILE *fp) {
  int i;
  if (fp) {
    if (__sysdev_table.enable) {
      fprintf(fp, "affinity is enable.\n");
      fprintf(fp, "num of threads is %d (#cores: %d, #sockets: %d)\n",
  	      __online_table.nthreads, __sysdev_table.nprocs, __sysdev_table.nnodes);
      fprintf(fp, "#physical cores is %d\n", get_numa_physical_ncores());
      fprintf(fp, "mapping table (CPUID:NODEID:APICID:CORE:VIRCORE) [master]\n");
      for (i = 0; i < __online_table.nthreads; ++i) {
  	int j = __mapping_table.cpumap[i];
	int is_root = __mapping_table.is_root[j];
	struct cpuinfo_t cj = __sysdev_table.cpuinfo[j];
  	fprintf(fp, "%c%02d:%02d:%03d:%02d:%02d%c ",
  		is_root?'[':' ', cj.id, cj.node, cj.apic, cj.core, cj.vircore, is_root?']':' ');
  	if ( (i+1) % (__sysdev_table.nprocs/__sysdev_table.ncores) == 0 ) fprintf(fp, "\n");
      }
      fprintf(fp, "\n");
    } else {
      fprintf(fp, "numa is disable\n");
    }
  }
}



/* ------------------------------------------------------------
 * initialize_sysdev_table
 * initialize_mapping_table
 * initialize_online_table
 * ------------------------------------------------------------ */
static struct sysdev_table_t initialize_sysdev_table(int use_hyper_threading) {
  int i, j;

  struct sysdev_table_t table;
  memset(&table, 0x00, sizeof(struct sysdev_table_t));
  table.nprocs = get_num_cores();
  table.nnodes = 1;

  /* get cpuinfo */
  int num_cpus = get_cpuinfo(table.cpuinfo);
  table.enable = num_cpus ? 1 : 0;
  if ( !table.enable ) {
    table.ncores = get_num_cores();
    return table;
  }

  /* disable hyperthreading */
  if ( !use_hyper_threading ) {
    if (__verbose) printf("disable hyperthreading\n");
    num_cpus = uniq(table.cpuinfo, num_cpus, sizeof(struct cpuinfo_t), qsort, htcore_comp);
    qsort(table.cpuinfo, num_cpus, sizeof(struct cpuinfo_t), cpuid_comp);
  }

  /* detect #cpus, #nodes, and #cores */
  table.nprocs = num_cpus;
  for (i = 0; i < table.nprocs; ++i) {
    if (table.nnodes < table.cpuinfo[i].node+1)
      table.nnodes = table.cpuinfo[i].node+1;
  }
  for (i = 0; i < table.nnodes; ++i) {
    int vircoreid = 0;
    for (j = 0; j < table.nprocs; ++j) {
      if (i == table.cpuinfo[j].node)
	table.cpuinfo[j].vircore = vircoreid++;
    }
    if (table.ncores < vircoreid)
      table.ncores = vircoreid;
  }

  return table;
}


static struct mapping_table_t initialize_mapping_table(int mapping_strategy) {
  int i, j, k;

  struct mapping_table_t table;
  memset(&table, 0x00, sizeof(struct mapping_table_t));
  table.mapping_strategy = mapping_strategy;
  for (j = 0; j < __sysdev_table.nprocs; ++j) {
    table.cpumap[j] = j;
    table.is_root[j] = (j == 0) ? 1 : 0;
  }

  /* construct mapping table */
  int tmp_table[MAX_NODES][MAX_CPUS] = {{-1}};
  for (i = 0; i < __sysdev_table.nprocs; ++i) {
    tmp_table[ __sysdev_table.cpuinfo[i].node ][ __sysdev_table.cpuinfo[i].vircore ]
      = __sysdev_table.cpuinfo[i].id;
  }

  /* make up affinity setting, master cores, and local cores */
  if (mapping_strategy == CORE_MAJOR_MAPPING) {
    if (__verbose) printf("using core-majar layout\n");
    k = 0;
    for (j = 0; j < __sysdev_table.ncores; ++j) {
      for (i = 0; i < __sysdev_table.nnodes; ++i) {
  	int id = tmp_table[i][j];
  	int nodeid = __sysdev_table.cpuinfo[id].node;
  	table.cpumap[k++] = id;
  	table.interleave |= (1UL << nodeid);
  	if (__sysdev_table.cpuinfo[id].vircore == 0) table.is_root[id] = 1;
      }
    }
  } else if (mapping_strategy == NODE_MAJOR_MAPPING) {
    if (__verbose) printf("using node-majar layout\n");
    k = 0;
    for (i = 0; i < __sysdev_table.nnodes; ++i) {
      for (j = 0; j < __sysdev_table.ncores; ++j) {
  	int id = tmp_table[i][j];
  	int nodeid = __sysdev_table.cpuinfo[id].node;
  	table.cpumap[k++] = id;
  	table.interleave |= (1UL << nodeid);
  	if (__sysdev_table.cpuinfo[id].vircore == 0) table.is_root[id] = 1;
      }
    }
  } else {
    assert( !"[error] unknown mappibg strategy" );
  }

  return table;
}


static struct online_table_t initialize_online_table(int threads) {
  int i, j;
  struct online_table_t table;
  memset(&table, 0x00, sizeof(struct online_table_t));

  if (threads < 1 || __sysdev_table.nprocs < threads) {
    table.nthreads = __sysdev_table.nprocs;
  } else {
    table.nthreads = threads;
  }

  if ( __sysdev_table.enable ) {
    for (j = 0; j < table.nthreads; ++j) {
      int id = __mapping_table.cpumap[j];
      if ( __mapping_table.is_root[id] ) ++table.online_nodes;
      ++table.online_cores[ __sysdev_table.cpuinfo[id].node ];
    }
  } else {
    table.online_nodes = 1;
    for (i = 0; i < __sysdev_table.nnodes; ++i) {
      table.online_cores[i] = (i == 0) ? __sysdev_table.nprocs : 0;
    }
  }

  return table;
}




/* ------------------------------------------------------------
 * parsing cpu info files
 * ------------------------------------------------------------ */
static int parse_cpufile(const char *file) {
  int x = -1;
  FILE *fp = fopen(file, "r");
  if (fp) {
    fscanf(fp, "%d", &x);
    fclose(fp);
  }
  return x;
}


static int numa_apicid(int coreid) {
  set_affinity(coreid);
  sleep(0);
  int aid = get_apicid();
  clear_affinity();
  return aid;
}


static int get_cpuinfo(struct cpuinfo_t *cpuinfo) {
  char path[PATH_MAX], dirpath[PATH_MAX];
  DIR *dp = NULL, *ldp = NULL;
  struct dirent *dir = NULL, *ldir = NULL;
  int cpuid, coreid, nodeid, num_cpus=0;

  /* open cpu files */
  strcpy(dirpath, "/sys/devices/system/cpu");
  dp = opendir(dirpath);
  if (!dp) {
    return 0;
  } else {
    while ( (dir = readdir(dp)) != 0 ) {
      /* scan cpu(core) id */
      cpuid = -1;
      sscanf(dir->d_name, "cpu%d", &cpuid);
      if (cpuid < 0 || MAX_CPUS < cpuid) continue;

      /* read core_id */
      sprintf(path, "%s/%s/topology/core_id", dirpath, dir->d_name);
      coreid = parse_cpufile(path);
      if (coreid >= 0) {
	cpuinfo[cpuid].id      = cpuid;
	cpuinfo[cpuid].apic    = numa_apicid(cpuid);
	cpuinfo[cpuid].core    = coreid;
	cpuinfo[cpuid].vircore = coreid;
	++num_cpus;
      }
    }
    closedir(dp);
  }

  /* open node files */
  strcpy(dirpath, "/sys/devices/system/node");
  dp = opendir(dirpath);
  if (!dp) {
    return 0;
  } else {
    while ( (dir = readdir(dp)) != 0 ) {
      /* scan node(socket) id */
      nodeid = -1;
      sscanf(dir->d_name, "node%d", &nodeid);
      if (nodeid < 0 || MAX_NODES < nodeid) continue;

      /* read cpu(core) id */
      sprintf(path, "%s/%s", dirpath, dir->d_name);
      ldp = opendir(path);
      if ( !ldp ) {
	break;
      } else {
    	while ( (ldir = readdir(ldp)) != NULL ) {
	  if ( !ldir ) continue;
	  if ( !ldir->d_name ) continue;
	  if ( strncmp(ldir->d_name, "cpu", strlen("cpu")) ) continue;
	  if ( !isdigit( *(ldir->d_name+strlen("cpu")) ) ) continue;
	  cpuid = -1;
	  sscanf(ldir->d_name, "cpu%d", &cpuid);
	  if (cpuid < 0 || MAX_CPUS < cpuid) continue;
	  cpuinfo[cpuid].node = nodeid;
	}
	closedir(ldp);
      }
    }
    closedir(dp);
  }
  return num_cpus;
}




/* ------------------------------------------------------------
 * comparing functions
 * ------------------------------------------------------------ */
static int cpuid_comp(const void *a, const void *b) {
  struct cpuinfo_t _a = *(struct cpuinfo_t *)a;
  struct cpuinfo_t _b = *(struct cpuinfo_t *)b;
  return _a.id - _b.id;
}


static int htcore_comp(const void *a, const void *b) {
  struct cpuinfo_t _a = *(struct cpuinfo_t *)a;
  struct cpuinfo_t _b = *(struct cpuinfo_t *)b;
  return _a.node - _b.node ? _a.node - _b.node : _a.core - _b.core;
}




/* ------------------------------------------------------------
 * allocations
 * ------------------------------------------------------------ */
struct mempool_t anonpage_alloc(size_t sz) {
  struct mempool_t pool;
  memset(&pool, 0x00, sizeof(struct mempool_t));

  pool.memsize = ROUNDUP( sz, page_size() );
  pool.pool = (unsigned char *)mmap(0, pool.memsize, PROTECTION, FLAGS, 0, 0);
  assert(pool.pool != MAP_FAILED);

  return pool;
}

void anonpage_free(struct mempool_t *pool) {
  if ( !pool       ) return ;
  if ( !pool->pool ) return ;
  assert( !munmap(pool->pool, pool->memsize) );
  pool->pool = NULL;
}

struct mempool_t lmalloc(size_t sz, int nodeid) {
  struct mempool_t pool;
  if ( enable_affinity() && 0 <= nodeid && nodeid < get_numa_online_nodes() ) {
    pool = anonpage_alloc(sz);
    pool.nodeid = nodeid;
    unsigned long mask = (1ULL << nodeid);
    mbind(pool.pool, pool.memsize, MPOL_PREFERRED,
          &mask, sizeof(unsigned long)*8, MPOL_MF_MOVE);
  } else {
    pool.memsize = sz;
    pool.nodeid = -1;
    assert( pool.pool =(unsigned char *)malloc(pool.memsize) );
  }
  return pool;
}

struct mempool_t imalloc(size_t sz) {
  struct mempool_t pool;
  if ( enable_affinity() ) {
    unsigned long mask = 0;
    for (int i = 0; i < get_numa_num_threads(); ++i) {
      mask |= (1ULL << get_numa_nodeid(i));
    }
    set_mempolicy(MPOL_INTERLEAVE, &mask, sizeof(unsigned long)*8);
    pool = anonpage_alloc(sz);
    pool.nodeid = get_numa_online_nodes();
    /* fault */
    /* mbind(pool.pool, pool.memsize, MPOL_INTERLEAVE, */
    /*    &mask, sizeof(unsigned long)*8, 0); */
  } else {
    pool.memsize = sz;
    pool.nodeid = -1;
    assert( pool.pool =(unsigned char *)malloc(pool.memsize) );
  }
  return pool;
}

void *lalloc(size_t sz) {
  int id;
  if ( omp_in_parallel() ) {
    id = omp_get_thread_num();
    id = get_numa_nodeid(id);
  } else {
    id = -1;
  }
  struct mempool_t pool = lmalloc(sz, id);
  memset(pool.pool, 0x00, pool.memsize);
  return pool.pool;
}

void lfree(struct mempool_t pool) {
  if ( enable_affinity() && 0 <= pool.nodeid ) {
    anonpage_free(&pool);
  } else {
    free(pool.pool);
  }
}

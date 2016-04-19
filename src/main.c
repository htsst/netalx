/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version modified by The GraphCREST Project
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
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
 *
 * 2013/07/02 Changed main() and added global variables for supporting 
 *            out-of-core operations.
 * ------------------------------------------------------------------------ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "generation.h"
#include "construction.h"
#include "para_bfs_csr.h"
#include "statistics.h"
#include "dump.h"

double generation_time    = 0.0;
double construction_time  = 0.0;
const double kron_param_A = 0.57;
const double kron_param_B = 0.19;
const double kron_param_C = 0.19;
const double kron_param_D = 1.0 - (0.57+0.19+0.19);
char *message = NULL;

int use_RMAT_generator    =  0;
int enable_energy_loop    =  0;
int use_parameter_tuning  =  0;	/* for parameter tuning mode */
int alpha_start           =  0;	/* for parameter tuning mode */
int alpha_end             = 10;	/* for parameter tuning mode */
int  beta_start           =  0;	/* for parameter tuning mode */
int  beta_end             = 10;	/* for parameter tuning mode */
int ALPHA_param           = -1;
int  BETA_param           = -1;
int use_pinned_node_major =  0;
int SCALE                 = 16;
int edgefactor            = 16;
int verbose               =  1;
int VERBOSE               =  0;	/* for DEBUG */

I64_t max_onmem_edges     =  1; // for out-of-core mode
int is_nonnuma_tree       =  0; // for out-of-core mode
I64_t baseline_fully_onmem_edges = INT64_MAX; // for out-of-core mode
int is_construction_only  =  0;  // for out-of-core mode

void print_usage(FILE *stream) {
  if (stream) {
    fprintf(stream,
	    "Version: " VERSION "\n"
	    "Copyright: Copyright (C) 2012 Yuichiro Yasui\n"
	    "\n"
	    "Usage: ./graph500 [option] ...\n"
	    "\n"
	    "Options\n"
	    "  -s SCALE\t   generate 2^SCALE vertices\n"
	    "  -e edgefactor\t   generate n*edgefactor edges\n"
	    "  -R \t\t   generate R-MAT graph (default:Kronecker Graph)\n"
	    "  -E \t\t   enable energy loop mode\n"
	    "  -k alpha:beta\t   alpha/beta parameter of Hybrid Algorithm\n"
	    "  -P \t\t   parameter tuning mode (set alpha/beta range to env PARAMRANGE)\n"
	    "  -p NUMTHREADS\t   number of threads (default: #threads=#cpu)\n"
	    "  -N \t\t   set pinned config. as Node-Major (default:Core-Major))\n"
	    "  -A \t\t   disable HT cores\n"
	    "  -m ONMEDGES\t  max number of on-memory edges\n"
      "  -f ONMEDGES\t  underline of on-memory edges\n"
	    "  -l \t\t   disable numa allocated tree(lowmem, external, restore mode only)\n"
      "  -c \t\t   graph construction only(lowmem, external mode only)\n"
	    "\n"
	    "Environments\n"
	    "  OMP_NUM_THREADS=NUMTHREADS\t   number of threads (default: #threads=#cpu)\n"
	    "  DUMP_DC=FILE\t\t\t   dumping degree centrality\n"
	    "  DUMP_DEGREE=FILE\t\t   dumping degree distribution\n"
	    "  DUMPEDGE=FILE\t\t\t   dumping edgelist (ID: 1,...,n)\n"
	    "  DUMPGRAPH=FILE\t\t   dumping graph (ID: 1,...,n)\n"
	    "  ENERGY_LOOP_LIMIT=SECONDS\t   time limit of energy loops\n"
	    "  PARAMRANGE=As:Ae:Bs:Be\t   alpha=[2^{As},2^{Ae}], beta=[2^{Bs},2^{Be}] for parameter tuning mode\n");
  }
}

int main(int argc, char **argv) {
  int run_graph500(int scale, int edgefactor, int threads);
  int threads = getenvi((char *)"OMP_NUM_THREADS", omp_get_num_procs());

  if ( sizeof(I64_t) != sizeof(int64_t) ) {
    fprintf(stderr, "No 64-bit support.\n");
    return EXIT_FAILURE;
  }

  while (1) {
    int ch = getopt(argc, argv, "s:e:p:ERPibk:NAvhr:Lm:f:C?");
    if (ch == -1) break;
    switch (ch) {
    case 's' : SCALE        = atoi(optarg);           break;
    case 'e' : edgefactor   = atoi(optarg);           break;
    case 'R' : use_RMAT_generator    = 1;             break;
    case 'E' : enable_energy_loop    = 1;             break;
    case 'k' : assert( sscanf(optarg, "%d:%d", &ALPHA_param, &BETA_param) == 2 ); break;
    case 'P' : {
      use_parameter_tuning  = 1;
      if ( getenv("PARAMRANGE") ) {
	assert( sscanf(getenv("PARAMRANGE"),
		       "%d:%d:%d:%d",
		       &alpha_start, &alpha_end, &beta_start, &beta_end) == 4 );
      }
      break;
    }
    case 'p' : threads               = atoi(optarg);  break;
    case 'N' : use_pinned_node_major = 1;             break;
    case 'A' : disable_hyper_threading_cores();       break;
    case 'm' : max_onmem_edges       = atoll(optarg); break; // for out-of-core mode
    case 'L' : is_nonnuma_tree       = 1;             break; // for out-of-core mode
    case 'C' : is_construction_only  = 1;             break; // for out-of-core mode
    case 'f' : baseline_fully_onmem_edges = atoll(optarg);  break; // for out-of-core mode
    case 'v' :
    case 'h' :
    default  : print_usage(stdout); return 1;
    }
  }
  setbuf(stdout, NULL);

#if 1
  run_graph500(SCALE, edgefactor, threads);
#else
  if ( !( 1 <= edgefactor &&
	  edgefactor < (long)(1ULL << SCALE) &&
	  (1ULL << SCALE) * edgefactor * sizeof(I64_t) < get_ramsize() ) ) {
    printf("invalid parameters: SCALE and edgefactor\n");
    return 1;
  } else {
    run_graph500(SCALE, edgefactor, threads);
  }
#endif

  return 0;
}


int run_graph500(int scale, int edgefactor, int threads) {
  double get_seconds(void);
  char *software_defails(void);

  double t1, t2;

  /* warmup numa nodes */
  set_numa_mapping_strategy(use_pinned_node_major); /* 0: core-major, 1: node-major */
  set_numa_num_threads(threads);
  omp_set_num_threads(get_numa_num_threads());

  int nn = get_numa_online_nodes();
  if ( nn & 0x01  && nn > 1 ) {
    fprintf(stderr, "No odd number of NUMA nodes (#NUMA-nodes is '%d') support.\n",
	    get_numa_online_nodes());
    return EXIT_FAILURE;
  }

  if ( ALPHA_param == -1 || BETA_param == -1 ) {
    /* ALPHA_param = get_numa_num_threads() / get_numa_online_nodes(); */
    /* BETA_param = get_numa_online_nodes(); */
    if (use_RMAT_generator) {
      ALPHA_param = 256;
      BETA_param  =   2;
    } else {
      ALPHA_param = 64;
      BETA_param  =  4;
    }
  }

  message = software_defails();
  printf("%s\n", message);

  int k;
  printf("< CPU affinity >\n");
  for (k = 0; k < nn; ++k) {
    printf("[node%02d (%2dcores)] = [ ", k, get_numa_online_cores(k));
    int u;
    for (u = 0; u < get_numa_num_threads(); ++u)
      if ( get_numa_nodeid(u) == k )
	printf("%2d ", get_numa_procid(u));
    printf("]\n");
  }
  printf("\n");

  printf("< graph generation >\n");
  struct edgelist_t *list = NULL;
  t1 = get_seconds();
  assert( list = graph_generation(scale, edgefactor, nn, 64) );
  t2 = get_seconds();
  generation_time = t2-t1;
  printf("graph generation takes %f seconds.\n", t2-t1);
  printf("\n");

  if ( getenv("DUMPEDGE") ) {
    char *s = getenv("DUMPEDGE");
    printf("dumpping edge list into '%s' ... ", s);
    t1 = get_seconds();
    dump_edgelist(s, list);
    t2 = get_seconds();
    printf("done. (%.3f seconds)\n", t2-t1);
    exit(1);
  }

  printf("< graph construction >\n");
  struct graph_t *G = NULL;
  t1 = get_seconds();
  assert( G = graph_construction(list) );
  t2 = get_seconds();
  construction_time = t2 - t1;
  printf("graph construction takes %f seconds.\n", t2-t1);
  printf("\n");

  if ( getenv("DUMPGRAPH") ) {
    char *s = getenv("DUMPGRAPH");
    printf("dumpping graph into '%s' ... ", s);
    t1 = get_seconds();
    dump_graph(s, G);
    t2 = get_seconds();
    printf("done. (%.3f seconds)\n", t2-t1);
    exit(1);
  }

  /* return 0; */
  printf("< BFS >\n");
  struct stat_t *stat = NULL;
  assert( stat = parallel_breadth_first_search(G, list) );
  output_graph500_results(stat, scale, 1ULL<<scale, edgefactor,
  			  kron_param_A, kron_param_B, kron_param_C, kron_param_D,
  			  generation_time, construction_time, list->numsrcs);

  free(message);
  free(stat);
  free_edgelist(list);
  free_graph(G);

  return 0;
}




char *software_defails(void) {
  const size_t msg_max = (1ULL << 14);
  char *msg = (char *)calloc(msg_max+1, sizeof(char));
  strcatfmt(msg, "----------------------------------------------------------------------\n");
  strcatfmt(msg, "%s\n", version());
  strcatfmt(msg, "----------------------------------------------------------------------\n");
  strcatfmt(msg, "CPU name             is %s\n", cpuname());
  strcatfmt(msg, "freq / RAM           is %.3f MHz / %.2f GB\n",
	    mfreq(), (double)get_ramsize()/(1ULL<<30));
  strcatfmt(msg, "#cpu, #nodes, #cores is %d %d %d\n",
	  get_numa_nprocs(), get_numa_nnodes(), get_numa_ncores());
#ifdef __INTEL_COMPILER
  strcatfmt(msg, "COMPILER             is ICC (Intel Compiler) version %.2f (%d)\n",
	  __INTEL_COMPILER / 100.0, __INTEL_COMPILER_BUILD_DATE);
#elif __GNUC__
  strcatfmt(msg, "COMPILER             is GCC (GNU C Compiler) version %d.%d.%d\n",
	  __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
  strcatfmt(msg, "COMPILER             is %s\n", "unknown");
#endif
  strcatfmt(msg, "----------------------------------------------------------------------\n");

  /* warmup numa nodes */
  strcatfmt(msg, "generator            is %s\n", use_RMAT_generator ? "R-MAT":"Kronecker");
  strcatfmt(msg, "scale, edgefactor    is %d %d\n", SCALE, edgefactor);
  strcatfmt(msg, "energy_loop          is %s\n", enable_energy_loop ? "enable" : "disable");
  strcatfmt(msg, "#threads, #NUMAs     is %d %d\n",
	  get_numa_num_threads(), get_numa_online_nodes());
  strcatfmt(msg, "pinned major         is %s\n",
	    use_pinned_node_major ? "Node-Major" : "Core-Major");
  strcatfmt(msg, "ALPHA parameter      is %lld\n", ALPHA_param);
  strcatfmt(msg, "BETA  parameter      is %lld\n", BETA_param);
#ifdef THREAD_LQ_SIZE
  strcatfmt(msg, "queue buffer size    is %lld\n", THREAD_LQ_SIZE);
#else
  strcatfmt(msg, "queue buffer size    is %lld\n", 1ULL << SCALE);
#endif
  strcatfmt(msg, "----------------------------------------------------------------------\n");
  return msg;
}

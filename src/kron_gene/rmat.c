/* -*- mode: C; mode: folding; fill-column: 70; -*- */
/* Copyright 2010,  Georgia Institute of Technology, USA. */
/* See COPYING for license. */
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
/* #ifdef HAVE_ALLOCA_H */
#include <alloca.h>
/* #endif */

#include "prng.h"
#include "splittable_mrg.h"
#include "graph_generator.h"

#if defined(_OPENMP) || defined(__MTA__)
static int64_t take_i64 (volatile int64_t* p);
static void release_i64 (volatile int64_t* p, int64_t val);
static struct packed_edge take_pe (volatile struct packed_edge* p);
static void release_pe (volatile struct packed_edge* p, struct packed_edge val);
#endif


static void partial_edge_range(I64_t size, I64_t offset, I64_t np, I64_t id,
			       I64_t *loop_start, I64_t *loop_end) {
  const I64_t qt = size / np;
  const I64_t rm = size - qt * np;
  *loop_start = qt * (id+0) + (id+0 < rm ? id+0 : rm) + offset;
  *loop_end   = qt * (id+1) + (id+1 < rm ? id+1 : rm) + offset;
}

/* Recursively divide a grid of N x N by four to a single point, (i, j).
   Choose between the four quadrants with probability a, b, c, and d.
   Create an edge between node i and j.
*/

MTA("mta expect parallel context")
static void rmat_edge(struct packed_edge *out, int SCALE,
		      double A, double B, double C, double D, const double *rn) {
  size_t rni = 0;
  int64_t i = 0, j = 0;
  int64_t bit = ((int64_t)1) << (SCALE-1);

  while (1) {
    const double r = rn[rni++];
    if (r > A) { /* outside quadrant 1 */
      if (r <= A + B) /* in quadrant 2 */
	j |= bit;
      else if (r <= A + B + C) /* in quadrant 3 */
	i |= bit;
      else { /* in quadrant 4 */
	j |= bit;
	i |= bit;
      }
    }
    if (1 == bit) break;

    /*
      Assuming R is in (0, 1), 0.95 + 0.1 * R is in (0.95, 1.05).
      So the new probabilities are *not* the old +/- 10% but
      instead the old +/- 5%.
    */
    A *= 0.95 + rn[rni++]/10;
    B *= 0.95 + rn[rni++]/10;
    C *= 0.95 + rn[rni++]/10;
    D *= 0.95 + rn[rni++]/10;
    /* Used 5 random numbers. */

    {
      const double norm = 1.0 / (A + B + C + D);
      A *= norm; B *= norm; C *= norm;
    }
    /* So long as +/- are monotonic, ensure a+b+c+d <= 1.0 */
    D = 1.0 - (A + B + C);
	
    bit >>= 1;
  }
  /* Iterates SCALE times. */
  write_edge(out, i, j);
}

#if defined(_OPENMP)||defined(__MTA__)
#define MAKE_RANDPERMUTE(name, elt_type, take, release)			\
  static void								\
  name (elt_type *A_in, int64_t nelem,					\
	mrg_state * restrict st)					\
  {									\
    elt_type * restrict A = A_in;					\
    int64_t k;								\
									\
    OMP("omp for") MTA("mta assert nodep")				\
      for (k = 0; k < nelem; ++k) {					\
	int64_t place;							\
	elt_type Ak, Aplace;						\
									\
	Ak = take (&A[k]);						\
									\
	mrg_skip (st, 1, k, 0);						\
	place = k + (int64_t)floor (mrg_get_double_orig (st) * (nelem - k)); \
	if (k != place) {						\
	  assert (place > k);						\
	  assert (place < nelem);					\
									\
	  Aplace = take (&A[place]);					\
									\
	  {								\
	    elt_type t;							\
	    t = A[place];						\
	    A[place] = A[k];						\
	    A[k] = t;							\
	  }								\
									\
	  {								\
	    elt_type t;							\
	    t = Aplace;							\
	    Aplace = Ak;						\
	    Ak = t;							\
	  }								\
									\
	  release (&A[place], Aplace);					\
	}								\
	release (&A[k], Ak);						\
      }									\
  }
#else
#define MAKE_RANDPERMUTE(name, elt_type, take, release)			\
  static void								\
  name (elt_type *A_in, int64_t nelem,					\
	mrg_state * restrict st)					\
  {									\
    elt_type * restrict A = A_in;					\
    int64_t k;								\
									\
    for (k = 0; k < nelem; ++k) {					\
      int64_t place;							\
									\
      place = k + (int64_t)floor (mrg_get_double_orig (st) * (nelem - k)); \
									\
      if (k != place) {							\
	elt_type t;							\
	t = A[place];							\
	A[place] = A[k];						\
	A[k] = t;							\
      }									\
    }									\
  }
#endif

MAKE_RANDPERMUTE(randpermute_int64_t, int64_t, take_i64, release_i64)
MAKE_RANDPERMUTE(randpermute_packed_edge, struct packed_edge, take_pe, release_pe)
#undef MAKE_RANDPERMUTE

void permute_vertex_labels(int64_t max_nvtx, mrg_state * restrict st, int64_t * restrict newlabel) {
  int64_t k;

  OMP("omp for")
  for (k = 0; k < max_nvtx; ++k)
    newlabel[k] = k;

  randpermute_int64_t(newlabel, max_nvtx, st);

}

void permute_edgelist(struct packed_edge * restrict IJ, int64_t nedge, mrg_state *st) {
  randpermute_packed_edge(IJ, nedge, st);
}

#define NRAND(ne) (5 * SCALE * (ne))

I64_t rmat_edgelist(int scale, int edgefactor,
		    int num_lists, struct packed_edge **edge_list,
		    double A, double B, double C, I64_t *nbfs_ptr, I64_t *bfs_root_ptr) {
  int64_t N = 1LL << scale, M = N * edgefactor;
  double D = 1.0 - (A + B + C);
  int64_t *iwork = malloc(N * sizeof(int64_t));
  assert( iwork );
  
  printf("N is %ld, M is %ld\n", (long)N, (long)M);
  assert(M >= N);
  assert(M >= edgefactor);
  
  init_random();
  
  double t1, t2;
  
  t1 = get_msecs() * 1e-3;
  printf("generating RMAT edge list ...\n");
  OMP("omp parallel num_threads(get_numa_num_threads())") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    I64_t node_start, node_end, thread_start, thread_end;
    partial_edge_range(M, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, 0, lcores, coreid, &thread_start, &thread_end);
    double *Rlocal = alloca(NRAND(1) * sizeof(double));
    mrg_state new_st = *(mrg_state*)prng_state;
    for (int64_t k = thread_start; k < thread_end; ++k) {
      int k2;
      mrg_skip(&new_st, 1, NRAND(1), 0);
      for (k2 = 0; k2 < NRAND(1); ++k2)
	Rlocal[k2] = mrg_get_double_orig(&new_st);
      rmat_edge(&edge_list[nodeid][k], scale, A, B, C, D, Rlocal);
    }
    OMP("omp barrier");
    
    OMP("omp single")
      mrg_skip(prng_state, 1, NRAND(M), 0);
    OMP("omp barrier");
    
    new_st = *(mrg_state*)prng_state;
    permute_vertex_labels(N, &new_st, iwork);

    for (int64_t k = thread_start; k < thread_end; ++k)
      write_edge(&edge_list[nodeid][k],
		 iwork[get_v0_from_edge(&edge_list[nodeid][k])],
		 iwork[get_v1_from_edge(&edge_list[nodeid][k])]);
    OMP("omp barrier");
    
    OMP("omp single")
      mrg_skip(prng_state, 1, N, 0);
    OMP("omp barrier");
    
    new_st = *(mrg_state*)prng_state;
    /* permute_edgelist(IJ, thread_end-thread_start, &new_st); */
  }
  
  mrg_skip(prng_state, 1, M, 0);
  free(iwork);
  
  t2 = get_msecs() * 1e-3;
  printf("done. (generated %ld edges) (%.2f seconds)\n", (long)M, t2-t1);

  
  int *has_adj = NULL;
  assert( has_adj = (int *)malloc(N * sizeof(int)) );
  
  OMP("omp parallel") {
    int id = omp_get_thread_num();
    int nodeid = get_numa_nodeid(id);
    int coreid = get_numa_vircoreid(id);
    int lcores = get_numa_online_cores(nodeid);
    pinned(USE_HYBRID_AFFINITY, id);
    OMP("omp barrier");
    
    I64_t node_start, node_end, thread_start, thread_end, k;
    partial_edge_range(N, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, node_start, lcores, coreid, &thread_start, &thread_end);
    for (k = thread_start; k < thread_end; ++k) {
      has_adj[k] = 0;
    }
    OMP("omp barrier");

    partial_edge_range(M, 0, num_lists, nodeid, &node_start, &node_end);
    partial_edge_range(node_end-node_start, 0, lcores, coreid, &thread_start, &thread_end);
    struct packed_edge *IJ = edge_list[nodeid];
    /* printf("%2d: E[%d][%lld:%lld]\n", id, nodeid, thread_start, thread_end); */
    for (k = thread_start; k < thread_end; ++k) {
      const int64_t i = get_v0_from_edge(&IJ[k]);
      const int64_t j = get_v1_from_edge(&IJ[k]);
      /* printf("%ld, %ld\n", i, j); */
      /* if ( !(0 <= i && i < N) || !(0 <= j && j < N) ) printf("i, j is %ld, %ld\n", i, j); */
      /* assert( 0 <= i && i < N ); */
      /* assert( 0 <= j && j < N ); */
      if (i != j) {
	has_adj[i] = has_adj[j] = 1;
      }
    }
    clear_affinity();
  }
  
  /* Sample from {0, ..., N-1} without replacement. */
  int64_t k = 0, v = 0;
  double R;
  int NBFS = NBFS_max;
  while (k < NBFS && v < N) {
    R = (N - v) * mrg_get_double_orig(prng_state);
    if ( !has_adj[v] || R > NBFS - k ) {
      ++v;
    } else {
      bfs_root_ptr[k++] = v++;
    }
  }
  free(has_adj);

  /* verification */
  if ( N <= v && k < NBFS ) {
    if (0 < k) {
      fprintf(stderr, "Cannot find %lld sample roots of non-self degree > 0, using %lld.\n",
	      (long long)NBFS, (long long)k);
      NBFS = k;
    } else {
      fprintf(stderr, "Cannot find any sample roots of non-self degree > 0.\n");
      exit(EXIT_FAILURE);
    }
  }
  
  *nbfs_ptr = NBFS;
  
  return M;
}

#if defined(_OPENMP)
#if defined(__GNUC__)||defined(__INTEL_COMPILER)
int64_t take_i64 (volatile int64_t *p) {
  int64_t oldval;
  do { oldval = *p; } while (!(oldval >= 0 && __sync_bool_compare_and_swap (p, oldval, -1)));
  return oldval;
}
void release_i64 (volatile int64_t *p, int64_t val) {
  assert (*p == -1);
  *p = val;
}
struct packed_edge take_pe (volatile struct packed_edge *p) {
  struct packed_edge out;
  do {
    OMP("omp critical (TAKE)") {
      out = *p;
      if (get_v0_from_edge(&out) >= 0)
        write_edge((struct packed_edge*)p, -1, -1);
    }
  } while (get_v0_from_edge(&out) < 0);
  OMP("omp flush (p)");
  return out;
}
void release_pe (volatile struct packed_edge *p, struct packed_edge val) {
  assert (get_v0_from_edge((const struct packed_edge*)p) == -1);
  OMP("omp critical (TAKE)") {
    *p = val;
    OMP("omp flush (p)");
  }
  return;
}
#else
/* XXX: These suffice for the above uses. */
int64_t take_i64 (volatile int64_t *p) {
  int64_t out;
  do {
    OMP("omp critical (TAKE)") {
      out = *p;
      if (out >= 0) *p = -1;
    }
  } while (out < 0);
  OMP("omp flush (p)");
  return out;
}
void release_i64 (volatile int64_t *p, int64_t val) {
  assert (*p == -1);
  OMP("omp critical (TAKE)") {
    *p = val;
    OMP("omp flush (p)");
  }
  return;
}
struct packed_edge take_pe (volatile struct packed_edge *p) {
  struct packed_edge out;
  do {
    OMP("omp critical (TAKE)") {
      out = *p;
      if (get_v0_from_edge(&out) >= 0)
        write_edge((struct packed_edge*)p, -1, -1);
    }
  } while (get_v0_from_edge(&out) < 0);
  OMP("omp flush (p)");
  return out;
}
void release_pe (volatile struct packed_edge *p, struct packed_edge val) {
  assert (get_v0_from_edge(p) == -1);
  OMP("omp critical (TAKE)") {
    *p = val;
    OMP("omp flush (p)");
  }
  return;
}
#endif
#elif defined(__MTA__)
int64_t take_i64 (volatile int64_t *p) {
  return readfe (p);
}
void release_i64 (volatile int64_t *p, int64_t val) {
  writeef (p, val);
}
struct packed_edge take_pe (volatile struct packed_edge *p) {
  /* If you get an error here, make sure GENERATOR_USE_PACKED_EDGE_TYPE is not
   * defined in generator/user_settings.h. */
  int64_t i = readfe(&p->v0);
  int64_t j = readfe(&p->v1);
  struct packed_edge result = {i, j};
  return result;
}
void release_pe (volatile struct packed_edge *p, struct packed_edge val) {
  /* If you get an error here, make sure GENERATOR_USE_PACKED_EDGE_TYPE is not
   * defined in generator/user_settings.h. */
  writeef (&p->v0, val.v0);
  writeef (&p->v1, val.v1);
}
#else
/* double_cas isn't used sequentially. */
#endif

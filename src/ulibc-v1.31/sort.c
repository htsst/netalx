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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#include <sys/time.h>

#include "ulibc.h"

/* ------------------------------------------------------------
 * heap sort : O(nlogn)
 * ------------------------------------------------------------ */
#define HQROOT    (0)
#define HQPRNT(x) (((x)-1)>>1)
#define HQLFCH(x) (((x)<<1)+1)

static inline void heapify(void *base, size_t size, size_t i,
			   int (*compar)(const void *, const void *)) {
  void *crr = memcpy(alloca(size), base+i*size, size);
  for (; i > HQROOT; i = HQPRNT(i)) {
    if ( compar(base+HQPRNT(i)*size, crr) <= 0 ) break;
    memcpy(base+i*size, base+HQPRNT(i)*size, size);
  }
  memcpy(base+i*size, crr, size);
}

static inline void extractmin(void *base, size_t size, size_t hqsz,
			      int (*compar)(const void *, const void *)) {
  size_t i = HQROOT, left = HQLFCH(HQROOT);
  void *HQ_hqsz = memcpy(alloca(size), base+hqsz*size, size);

  /* left and right */
  while ( left+1 < hqsz ) {
    if ( compar(base+left*size, base+(left+1)*size) < 0 &&
	 compar(base+left*size, HQ_hqsz) < 0 ) {
      memcpy(base+i*size, base+left*size, size);
      i = left;
      left = HQLFCH(i);
    } else if ( !( compar(base+left*size, base+(left+1)*size) < 0 ) &&
		compar(base+(left+1)*size, HQ_hqsz) < 0 ) {
      memcpy(base+i*size, base+(left+1)*size, size);
      i = left+1;
      left = HQLFCH(i);
    } else {
      break;
    }
  }

  /* left only */
  if ( left+1 == hqsz &&
       compar(base+left*size, HQ_hqsz) < 0 ) {
    memcpy(base+i*size, base+left*size, size);
    i = left;
  }

  if (i != hqsz) {
    memcpy(base+i*size, HQ_hqsz, size);
  }
}

int hsort(void *base, size_t nmemb, size_t size,
	  int (*compar)(const void *, const void *)) {
  size_t i;
  void *scratch = malloc(size * nmemb);

  if (scratch) {
    memcpy(scratch, base, size * nmemb);

    /* heapify */
    for (i = 0; i < nmemb; ++i) {
      heapify(scratch, size, i, compar);
    }

    /* extractmin */
    for (i = 0; i < nmemb; i++) {
      memcpy(base+i*size, scratch, size);
      extractmin(scratch, size, nmemb-i-1, compar);
    }

    free(scratch);
    return 0;
  } else {
    return 1;
  }
}



/* ------------------------------------------------------------
 * merge sort : O(nlogn)
 * ------------------------------------------------------------ */
static void merge_recursive(void *base, size_t nmemb, size_t size,
			    int (*compar)(const void *, const void *),
			    long left, long right, void *scratch) {
  long i, l, r, len, pivot;
  if (left+1 < right) {
    len = right - left;
    pivot = len/2;
    l = left;
    r = left + pivot;
    merge_recursive(base, nmemb, size, compar, left, left + pivot, scratch);
    merge_recursive(base, nmemb, size, compar, left + pivot, right, scratch);
    for (i = 0; i < len; i++) {
      if ( (l < left + pivot) &&
	   (r == right || compar(base+l*size, base+r*size) < 0) ) {
	memcpy(scratch+i*size, base+l*size, size);
	l++;
      } else {
	memcpy(scratch+i*size, base+r*size, size);
	r++;
      }
    }
    memcpy(base+left*size, scratch, (right-left)*size);
  }
}


int msort(void *base, size_t nmemb, size_t size,
	  int (*compar)(const void *, const void *)) {
  void *scratch = malloc(size * nmemb);
  if (scratch) {
    merge_recursive(base, nmemb, size, compar, 0, nmemb, scratch);
    free(scratch);
    return 0;
  } else {
    return 1;
  }
}


/* ------------------------------------------------------------
 * quick sort : average-case O(nlogn), worst-case O(n^2)
 * ------------------------------------------------------------ */
static inline void krqsort_swap(void *base, size_t size, long i, long j) {
  if (i != j) {
    void *buf = memcpy(alloca(size), base+i*size, size);
    memcpy(base+i*size, base+j*size, size);
    memcpy(base+j*size, buf, size);
  }
}

void krqsort_recursive(void *base, size_t nmemb, size_t size,
		       int (*compar)(const void *, const void *),
		       long left, long right) {
  long i, last;
  if (left >= right) return;
  krqsort_swap(base, size, left, left+(right-left)/2);
  last = left;
  for (i = left+1; i <= right; i++) {
    if ( compar(base+i*size, base+left*size) < 0 ) {
      krqsort_swap(base, size, ++last, i);
    }
  }
  krqsort_swap(base, size, left, last);
  krqsort_recursive(base, nmemb, size, compar, left, last-1);
  krqsort_recursive(base, nmemb, size, compar, last+1, right);
}

int krqsort(void *base, size_t nmemb, size_t size,
	    int (*compar)(const void *, const void *)) {
  krqsort_recursive(base, nmemb, size, compar, 0, nmemb-1);
  return 0;
}


/* ------------------------------------------------------------
 * insertion sort : O(n^2)
 * ------------------------------------------------------------ */
int isort(void *base, size_t nmemb, size_t size,
	  int (*compar)(const void *, const void *)) {
  size_t i, j;
  void *buf = memset(alloca(size), 0x00, size);
  for (i = 1; i < nmemb; ++i) {
    memcpy(buf, base+i*size, size);
    if ( compar(base+(i-1)*size, buf) > 0 ) {
      j = i;
      do {
	memcpy(base+(j)*size, base+(j-1)*size, size);
	--j;
      } while ( j > 0 && compar(base+(j-1)*size, buf) > 0 );
      memcpy(base+(j)*size, buf, size);
    }
  }
  return 0;
}


/* ------------------------------------------------------------
 * bucket sort : O(n)
 * ------------------------------------------------------------ */
int bsort(void *base, size_t nmemb, size_t size, long (*getkey)(const void *)) {
  size_t n=0, i, j;

  void *buffer = malloc(size*nmemb);
  if ( !buffer ) return 1;
  memcpy(buffer, base, size*nmemb);

  long min, max, k;
  min = max = getkey(base+size);
  for (j = 0; j < nmemb; ++j) {
    k = getkey(base+size*j);
    if (k < min) min = k;
    if (k > max) max = k;
  }
  n = max-min+1;

  size_t *incident = calloc(n+1, sizeof(size_t));
  if ( !incident ) return 1;

  for (j = 0; j < nmemb; ++j) {
    incident[ getkey(base+size*j)+1 ]++;
  }
  for (i = 1; i <= n; ++i) {
    incident[i] += incident[i-1];
  }
  for (j = 0; j < nmemb; j++) {
    k = incident[ getkey( buffer+size*j ) ]++;
    memcpy(base+size*k, buffer+size*j, size);
  }

  free(incident);
  free(buffer);
  return 0;
}



int ipbsort(void *base, size_t nmemb, size_t size, long (*getkey)(const void *)) {
  size_t n=0, i, j;
  long min, max, k;
  min = max = getkey(base+size);
  for (j = 0; j < nmemb; ++j) {
    k = getkey(base+size*j);
    if (k < min) min = k;
    if (k > max) max = k;
  }
  n = max-min+1;

  size_t *incident = calloc(n+1, sizeof(size_t));
  if ( !incident ) return 1;

  for (j = 0; j < nmemb; ++j) {
    incident[ getkey(base+size*j)+1 ]++;
  }
  for (i = 1; i <= n; ++i) {
    incident[i] += incident[i-1];
  }

  size_t *degree = calloc(n+1, sizeof(size_t));
  if ( !degree ) return 1;

  memcpy(degree, incident, sizeof(size_t)*(n+1));

  size_t nx;
  void *pvbf = memset(alloca(size), 0x00, size);
  void *nxbf = memset(alloca(size), 0x00, size);

  for (i = 0; i < n; ++i) {
    for (j = degree[i]; j < incident[i+1]; ++j) {
      memcpy(pvbf, base+size*j, size);
      do {
	nx = degree[getkey(pvbf)]++;
	memcpy(nxbf, base+size*nx, size);
	memcpy(base+size*nx, pvbf, size);
	memcpy(pvbf, nxbf, size);
      } while (nx != j);
    }
  }

  free(degree);
  free(incident);
  return 0;
}


size_t uniq(void *base, size_t nmemb, size_t size,
		   void (*sort)(void *base, size_t nmemb, size_t size,
				int (*compar)(const void *, const void *)),
		   int (*compar)(const void *, const void *)) {
  size_t i, crr;
  sort(base, nmemb, size, compar);
  for (i = crr = 1; i < nmemb; i++) {
    if ( compar(base+(i-1)*size, base+i*size) != 0 ) {
      if (base+crr*size != base+i*size )
        memcpy(base+crr*size, base+i*size, size);
      crr++;
    }
  }
  return crr;
}


/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

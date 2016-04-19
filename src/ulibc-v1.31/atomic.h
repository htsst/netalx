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

#ifndef ATOMIC_H
#define ATOMIC_H

#ifndef I64_t
#define I64_t long long int
#endif

/* #define NOT_USE_GCC_EXT */

#ifdef NOT_USE_GCC_EXT
static inline I64_t I64_fetch_add(I64_t* p, I64_t incr) {
  I64_t t;
  OMP("omp critical") {
    t = *p;
    *p += incr;
  }
  OMP("omp flush (p)");
  return t;
}
static inline I64_t I64_fetch_or(I64_t* p, I64_t incr) {
  I64_t t;
  OMP("omp critical") {
    t = *p;
    *p |= incr;
  }
  return t;
}
static inline I64_t I64_add_fetch(I64_t *p, I64_t incr) {
  OMP("omp critical") {
    *p += incr;
  }
  OMP("omp flush (p)");
  return *p;
}

#define SYNC_FETCH_AND_ADD I64_fetch_add
#define SYNC_FETCH_AND_OR  I64_fetch_or
#define SYNC_ADD_AND_FETCH I64_add_fetch
#else
#define SYNC_FETCH_AND_ADD __sync_fetch_and_add
#define SYNC_FETCH_AND_OR  __sync_fetch_and_or
#define SYNC_ADD_AND_FETCH __sync_add_and_fetch
#endif


#define UL_t                unsigned long
#define UL_SHIFT            64ULL
#define UL_SHIFT2            6ULL
#define BIT_i(x)            (  (unsigned long)(x) >>  UL_SHIFT2     )
#define BIT_j(x)            (  (unsigned long)(x)  & (UL_SHIFT-1ULL) )
#define BIT_v(i,j)          ( ((unsigned long)(i) <<  UL_SHIFT2) + j)
#define BITMASK(a, b, c)    ((((unsigned long)(a) >> (b)) & (c))    )
#define   SET_BITMAP(map,x)   map[ BIT_i(x) ] |=  (1ULL << BIT_j(x))
#define UNSET_BITMAP(map,x)   map[ BIT_i(x) ] &= ~(1ULL << BIT_j(x))
#define ISSET_BITMAP(map,x) ( map[ BIT_i(x) ] &   (1ULL << BIT_j(x)) )
#define TEST_AND_SET_BITMAP(map,x) SYNC_FETCH_AND_OR(&map[BIT_i(x)], (1ULL << BIT_j(x)))
#define IS_TEST_AND_SET_BITMAP(map,x)					\
  (SYNC_FETCH_AND_OR(&map[BIT_i(x)], (1ULL << BIT_j(x))) & (1ULL << BIT_j(x)))

#endif /* ATOMIC_H */

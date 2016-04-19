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

/* -*- mode: C; mode: folding; fill-column: 70; -*- */
#if !defined(UTEST_HEADER_)
#define UTEST_HEADER_

#include <stdio.h>
#include <stdlib.h>

extern int VERBOSE;

#define CHECKPOINT() do { \
    if (VERBOSE) \
      fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __func__); \
  } while (0);


#endif /* UTEST_HEADER_ */

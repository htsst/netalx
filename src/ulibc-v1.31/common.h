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

#ifndef ULIBC_COMMON_H
#define ULIBC_COMMON_H

/* define data type (like a stdint.h) */
#define I32_t   signed int
#define U32_t unsigned int
#define I64_t   signed long long int
#define U64_t unsigned long long int
#define U32_MAX ((U32_t)(~0))
#define I32_MAX ((I32_t)(U32_MAX >> 1))
#define I32_MIN ((I32_t)(-I32_MAX - 1LL))
#define U64_MAX ((U64_t)(~0))
#define I64_MAX ((I64_t)(U64_MAX >> 1))
#define I64_MIN ((I64_t)(-I64_MAX - 1LL))
#define U32_SHIFT 32
#define U64_SHIFT 64

/* MAX value */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef LINE_MAX
#define LINE_MAX 4096
#endif
#ifndef MAX_NODES
#define MAX_NODES 16
#endif
#ifndef MAX_CPUS
#define MAX_CPUS 256
#endif

#ifndef ROUNDUP
#  define ROUNDUP(x,a) (((x)+(a)-1) & ~((a)-1))
#endif
#ifndef ALIGN_UP
#  define ALIGN_UP(x,a)   (((x) + (a)) & ~((a) - 1))
#endif
#ifndef ALIGN_DOWN
#  define ALIGN_DOWN(x,a) ((x) & ~((a) - 1))
#endif
#ifndef BITMASK
#  define BITMASK(a, b, c) ((((a) >> (b)) & (c)))
#endif

#ifndef HANDLE_ERROR
#  define HANDLE_ERROR(...)					\
  do {								\
    char err[256];						\
    sprintf(err, __VA_ARGS__);					\
    perror(err);						\
    fprintf(stderr, "[ULIBC] ERROR %s:%d:%s (errno:%d)\n",	\
	    __FILE__, __LINE__, __func__, errno);		\
    exit(EXIT_FAILURE);						\
  } while (0)
#endif

#endif /* ULIBC_COMMON_H */

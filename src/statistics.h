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

#ifndef STATISTICS_H
#define STATISTICS_H

#include "ulibc.h"
#include "mempol.h"
#include "defs.h"
#include "atomic.h"
#include "generation.h"
#include "construction.h"

extern int   statVERBOSE;
extern double    minTEPS;
extern double     fqTEPS;
extern double medianTEPS;
extern double     tqTEPS;
extern double    maxTEPS;

void output_graph500_results(struct stat_t *stat,
			     const I64_t SCALE, I64_t nvtx_scale, I64_t edgefactor,
			     const double A, const double B, const double C, const double D,
			     const double generation_time, const double construction_time,
			     const int NBFS);

#endif /* STATISTICS_H */

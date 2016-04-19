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

#include <stdio.h>
#include <math.h>
#include "ulibc.h"

/* static inline double f2sqrt(const double x) { */
/*   double xh = 0.5 * x; */
/*   long long int tmp = 0x5FE6EB50C7B537AALL - ( *(long long int*)&x >> 1); */
/*   double xr = * (double*)&tmp; */
/*   xr *= ( 1.5 - ( xh * xr * xr ) ); */
/*   xr *= ( 1.5 - ( xh * xr * xr ) ); */
/*   xr *= ( 1.5 - ( xh * xr * xr ) ); */
/*   return xr * x; */
/* } */

int main(void) {
  printf("  sqrt(2.0) = %.20f\n", sqrt(2.0));
  /* printf("f2sqrt(2.0) = %.20f\n", f2sqrt(2.0)); */
  printf(" fsqrt(2.0) = %.20f\n", fsqrt(2.0));
  return 0;
}

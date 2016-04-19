#!/usr/bin/awk -f

 # ----------------------------------------------------------------------
 #
 # This is part of NETAL.
 #
 # Copyright (C) 2011-2015 Yuichiro Yasui
 #
 # NETAL is free software; you can redistribute it and/or
 # modify it under the terms of the GNU General Public License
 # as published by the Free Software Foundation; either version 2
 # of the License, or (at your option) any later version.
 #
 # NETAL is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License
 # along with NETAL; if not, write to the Free Software
 # Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 #
 #  ----------------------------------------------------------------------


BEGIN {
    printf "generator  SCALE  edgefactor  alpha  beta  np  minTEPS  1/4TEPS  medianTEPS  3/4TEPS  maxTEPS\n"
}

$1 ~ /^Kronecker|^R-MAT/ {
    gene = $1
    split($2,      scale, "=")
    split($3, edgefactor, "=")
    split($4,      alpha, "=")
    split($5,       beta, "=")
    split($6,         np, "=")
    TEPS[1] = $7
    TEPS[2] = $8
    TEPS[3] = $9
    TEPS[4] = $10
    TEPS[5] = $11

    printf "%9s  %2d  %3d  %8d  %8d  %2d  %s  %s  %s  %s  %s\n",
	gene, scale[2], edgefactor[2],
	alpha[2], beta[2], np[2],
	TEPS[1], TEPS[2], TEPS[3], TEPS[4], TEPS[5]
}

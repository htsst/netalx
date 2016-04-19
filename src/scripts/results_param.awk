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
    printf "generator  NP  pinned  alpha  beta  SCALE  edgefactor  minTEPS  1/4TEPS  medianTEPS  3/4TEPS  maxTEPS\n"
}

$0 ~ /^generator/ {
    printf "%-10s  ", $3
}
$0 ~ /^pinned\ major/ {
    printf "%10s  ", $4
}
$0 ~ /^\#threads,\ \#NUMAs/ {
    printf "%3s  ", $4
}
$0 ~ /^ALPHA\ parameter|^BETA\  parameter/ {
    printf "%5s  ", $4
}
$1 ~ /^SCALE|^edgefactor/ {
    printf "%3s  ", $2
}
$1 ~ /^min_TEPS|^firstquartile_TEPS|^median_TEPS|^thirdquartile_TEPS/ {
    printf "%s  ", $2
}
$1 ~ /^max_TEPS/ {
    printf "%s\n", $2
}

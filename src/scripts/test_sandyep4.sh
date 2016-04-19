#!/bin/sh

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


echodo() {
   echo $*
   $*
}

# BIN="./graph500_single"
BIN="./graph500"

# for j in 8
# do
#     for i in 64
#     do
# 	echodo $BIN -s 26 -e $i -p $j >> graph500_v382_kron_scale26_sandyep4.txt
#     done
# done
# for j in 4 2 1
# do
#     for i in 8 16 32 64
#     do
# 	echodo $BIN -s 26 -e $i -p $j >> graph500_v382_kron_scale26_sandyep4.txt
#     done
# done


### ------------------------------------------------------------
### SCALE: 26
### #CPUs: 1,2,4,8,16,32,64
###   m/n: 8,16,32,64
### ------------------------------------------------------------
for j in 64 32 16 8 4 2 1
do
    for i in 8 16 32 64
    do
	echodo $BIN -s 26 -e $i -p $j >> graph500_v382_kron_scale26_sandyep4.txt
    done
done


### ------------------------------------------------------------
### SCALE: 20,21,22,23,24,25,26,27,28,29
### #CPUs: 64
###   m/n: 8,16,32,64
### ------------------------------------------------------------
for j in 8 16 32 64
do
    for i in 20 21 22 23 24 25 26 27
    do
	echodo $BIN -s $i -e $j >> graph500_v382_kron_scale_sandyep4.txt
    done
done
$BIN -s 28 -e  8 >> graph500_v382_kron_scale_sandyep4.txt
$BIN -s 28 -e 16 >> graph500_v382_kron_scale_sandyep4.txt
$BIN -s 28 -e 32 >> graph500_v382_kron_scale_sandyep4.txt
$BIN -s 29 -e  8 >> graph500_v382_kron_scale_sandyep4.txt
$BIN -s 29 -e 16 >> graph500_v382_kron_scale_sandyep4.txt

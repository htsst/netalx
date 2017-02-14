# NETALX
## Overview

NETALX is a customized Breadth First Search (BFS) implementation for the [Graph500](http://www.graph500.org) list.
Based on [the NUMA-optimized parallel BFS implementation developed by Yasui et al.](http://ieeexplore.ieee.org/xpl/login.jsp?tp=&arnumber=6691600&url=http%3A%2F%2Fieeexplore.ieee.org%2Fxpls%2Fabs_all.jsp%3Farnumber%3D6691600) and with the help of Non-volatile Memory (NVM) devices, NETALX can conduct BFS to large graphs whose size exceed the capacity of DRAM on a machine.
The basic idea of the NETALX implementation is described in the following paper.

+ [Keita Iwabuchi, Hitoshi Sato, Yuichiro Yasui, Katsuki Fujisawa, Satoshi Matsuoka, "NVM-based Hybrid BFS with memory efficient data structure", 2014 IEEE International Conference on Big Data, 529-538, Oct 2014.](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=7004270)

## Required Software
+ GCC (gnu99)
+ OpenMP

We have tested on CentOS 6.7 machines.

## Build
Just type the following commands.

```
$ cd src
$ make
```

Then, we obtain the following three binaries.

+ `graph500_exmem`
Out-of-core version with bucket-sort-based graph construction.
+ `graph500`
In-core version.
+ `graph500_restore`
Out-of-core version without graph construction. Instead, restore a given graph that is constructed using *graph500_exmem* from NVM.

## Algorithm

NETALX employs a hybrid BFS algorithm (a.k.a dierction-optimizing breadth first search) proposed by [Beamer et al. at SC12](http://dl.acm.org/citation.cfm?id=2389013).
The hybrid BFS algorithm can reduce waste edge traversals by changing two approaches, top-down approach and bottom-up approach.
The top-down approach is a conventional BFS that searches unvisited vertices from visited vertices (frontier).
The major drawback of the top-down approach is that a lot of waste edge traversals will be incurred when the size of frontier is large, since a few unvisited vertices exist in such situation and vertices in frontier tend to redundantly search visited vertices.
On the other hand, the bottom-up search searches alreadly visited vertices (frontier) from unvisited vertices to avoid the above drawback; however, the search efficiency is not improved when the size of frontier is small.
NETALX changes search directions based on the threshold parameters, α and ß, and the assesment of the expected numbers of vertices and edges.
Let i be a given level, n\_f(i) be the number of vertices in the frontier, m\_td(i) be the expected number of scanned edges in the top-down approach, m\_bu(i) be the expected number of scanned edges in the bottom-up approach.
Here, we obtain m\_td(i) as edge\_factor * n\_f(i), and m\_bu(i) as edge\_factor * n\_u(i) + n\_f(i).
Based on these parameters, we use the top-down approach when n\_f(i-1) > n\_f(i) and m\_td(i) < m\_bu(i) / ß, and the bottom-up approach when n\_f(i-1) < n\_f(i) and m\_td(i) > m\_bu(i) / α.
Note that α is used to change to the bottom-up approach, and ß is used to return to the top-down approach.

## Configurations
### Command Line Options

+ `-s SCALE(=16)`
  Generate 2^SCALE vertices.
+ `-e edgefactor(=16)`
  Generate num\_vertices\*edgefactor edges.
+ `-R`
  Use RMAT generator (default: Kronecker Graph).
+ `-E`
  Enable energy loop mode. See Examples for the details.
+ `-k α:ß (=64:4 for kronecker generator and =256:2 for RMAT generator)`
  Specify the threshold parameters α and ß. α is used to change to the bottom-up approach from the top-down approach, and ß is used to return to the top-down approach from the bottom-up approach.
+ `-P`
  Enable parameter turning mode. The PARAMRANGE environment variable is required for seeting the range of α and ß. See Examples for the details.
+ `-a`
  Skip validation.
+ `-P N`
  Set number of threads to N.
+ `-N`
  Set pinned config as Node-Major (default:Core-Major)).
+ `-A`
  Disable hyper threading.
+ `-m ONMEM_EDGES`
  Set the maximum number of edges stored on DRAM for each vertex. This option is valid for *graph500\_exm* and *graph500\_restore*.
+ `-L`
  Share BFS tree data structure among the NUMA nodes. This option will reduce the DRAM consumption by O(#vertices) * (#NUMAS-1) with small (less than 10%) performance degradation. This option is valid for *graph500\_exm* and *graph500\_restore*.
+ `-C`
  Disable to run BFS (lowmem, external, restore mode only). Only conduct graph generation and construction. This option is valid for *graph500\_exm* and *graph500\_restore* (lowmem, external, restore mode only).
+ `-h, -v`
  Print usage.

### Environment Variables

+ `OMP_NUM_THREADS=NUMTHREADS(default: #cpus)`
  Set the number of threads.
+ `DUMP_DC=FILE`
  Dump degree centrality to FILE.
+ `DUMP_DEGREE=FILE`
  Dump degree distribution to FILE.
+ `DUMPEDGE=FILE`
  Dump edgelist (ID: 1,...,n) to FILE.
+ `DUMPGRAPH=FILE`
  Dump graph (ID: 1,...,n) to FILE.
+ `ENERGY_LOOP_LIMIT=SECONDS`
  Set the time limit of energy loops.
+ `PARAMRANGE=As:Ae:Bs:Be`
  Set the range of parameters to α=[2^{As},2^{Ae}], ß=[2^{Bs},2^{Be}] for parameter tuning mode
+ `EXMEM_CONF_FILE=FILE`
  Read a configuration file (FILE) that specifies the data layout on NVMs. This variable is valid for *graph500\_exm* and *graph500\_restore*.

### Configuration File

The configuration file, which is valid for *graph500\_exm* and *graph500\_restore*, specifies the data layout on NVMs.
For a key, the corresponding value sets paths which are separated by colons(:).
Note that you can specify the file name by setting the EXMEM\_CONF\_FILE environment variable (default: exmem.conf).

Key|Description|Size
---|---|---
EDGELIST|Generated edgelists. #files is #NUMAs|3\*2^(SCALE+6) bytes
GRAPH|Constructed graphs.  #files is #NUMAs|start: 2^(SCALE+3) + end: 2^(SCALE+8) bytes
SRCS|A list of souce vertices of a benchmark. #files is always 1|520 bytes
EDGEBCKT|Work space used in graph construction step (*only graph500\_exm*).  #files is set to a value of -b option |edgelist\*2

## Examples

### Execute BFS to a graph with 2^26 vertices (SCALE26) using only DRAM with α = 128 and ß = 8

```
$ ./graph500 -s 26 -k 128:8
```
### Execute BFS under the parameter tuning mode (8 <= α <= 256, 2 <= ß <= 8)

Parameter Tuning Mode conducts BFS for all combinations of specified α and β parameter ranges withoug validiation steps.
If you specify the PARAMRANGE environment variable as PARAMRANGE=a1: a2: b1: b2,
this mode conducts 64 times BFS using the combinations of parameters for α in [2^a1:2^a2] and
β in [2^b1:2^b2].

```
$ PARAMRANGE=3:8:1:3 ./graph500 -s 26 -P
```

### Execute BFS to a SCALE28 graph using two NVM devices (/mnt/nvm0, /mnt/nvm1) with α = 128 and ß = 8, including out-of-core graph construction using 256 buckets

```
$ cat exmem.conf
EDGELIST /mnt/nvm0/data_edgelist:/mnt/nvm1/data_edgelist
GRAPH /mnt/nvm0/data_graph:/mnt/nvm1/data_graph
SRC /mnt/nvm/data_src
EDGEBCKT /mnt/nvm0/bucket:/mnt/nvm1/bucket
$ ./graph500_exmem -s 28 -k 128:8 -b 256
```

### Execute BFS to a SCALE28 graph using a NVM device (/mnt/nvm) under the energy loop mode (100 sec.) by loading the edge list, the constructed graph, and the source vertices

After graph construciton, Energy Loop Mode conducts BFS without validiation repeatedly during a specified time in order to accurately measure the power consumption of BFS.
This mode is useful for the Green Graph500 benchmark.

```
$ cat exmem_restore.conf
EDGELIST /mnt/nvm0/data_edgelist:/mnt/nvm1/data_edgelist
GRAPH /mnt/nvm0/data_graph:/mnt/nvm1/data_graph
SRC /mnt/nvm/data_src
EDGEBCKT /mnt/nvm0/bucket:/mnt/nvm1/bucket
$ EXMEM_CONF_FILE=exmem_restore.conf ENERGY_LOOP_LIMIT=100 ./graph500_restore -s 28 -E
```

## Known Problems
### About Page Size
NETALX assumes that the page size is approximately 4KB (2~4MB for huge page).
Therefore, if the size is much larger than our assumption, NETALX may consume huge memory spaces than the actual because memory spaces are rounded up to the page size.

-----------------------

## License
NETALX is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Copyright
Copyright (C) 2012 - 2015  The GraphCREST Project, Tokyo Institute of Technology

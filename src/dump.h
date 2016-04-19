/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Copyright (C) 2013-2015 The GraphCREST Project, Tokyo Institute of Technology
 *
 * NETALX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * ------------------------------------------------------------------------ */
#ifndef DUMP_H
#define DUMP_H

#include "ulibc.h"
#include "mempol.h"
#include "defs.h"
#include "generation.h"
#include "construction.h"

/* -----------------------------
 * mode select, parameter
 * ----------------------------- */
#define STORE_BFS_SOURCES             1
#define STORE_GRAPH_IN_LOWMEM_MODE    0
#define DUMP_BUF_LENGTH               (1ULL << 9)
#define DUMP_BUF_READ_LENGTH_TD       (1ULL << 9)
#define DUMP_BUF_READ_LENGTH_BU       (1ULL << 6)

#define DIRECT_IO_VALIDATION          0

#define ENV_EXMEM_CONF_FILE     "EXMEM_CONF_FILE"
#define FNAME_EXMEM_CONF        "exmem.conf"

/* -----------------------------
 * For Profiling
 * ----------------------------- */
#define PROFILE_DETAIL          0
#define DUMP_TE_PROFILE         0
#define ENV_DUMP_TE_PROFILE     "DUMP_TE_PROFILE"


/* -----------------------------
 * filename max length
 * ----------------------------- */
#define MAX_FNAME 512

/* -----------------------------
 * filename base list delimiter
 * ----------------------------- */
#define FNAME_DELIM ":"


/* -----------------------------
 * env name for dump mode
 * ----------------------------- */
#define CONFIG_EDGELIST "EDGELIST"
#define CONFIG_GRAPH    "GRAPH"
#define CONFIG_HOPS     "HOPS"
#define CONFIG_TREE     "TREE"
#define CONFIG_SRCS     "SRCS"
#define CONFIG_EDGEBCKT "EDGEBCKT"
/* -----------------------------
 * Macros
 * ----------------------------- */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SWAP(x, y) do { typeof(x) temp_##x##y = x; x = y; y = temp_##x##y; } while (0)


/* -----------------------------
 * max number fo on-memory edges
 * ----------------------------- */
extern I64_t max_onmem_edges;
extern I64_t baseline_fully_onmem_edges;

/* -----------------------------
 * flags for dumpping bfs status data
 * ----------------------------- */
extern int is_dump_hops;
extern int is_nonnuma_tree;
extern int is_dump_tree;
extern int is_construction_only;

/* -----------------------------
 * structure for dump files pointer
 * ----------------------------- */
struct file_info_t {
  int fd;
  FILE *fp;
  char fname[256];
};
struct dumpfiles_t {

  // -- num_files  -- //
  // edgelist   : num_threads * num_edgelists
  //              because each threads access to all edgefiles
  // graph data : num_threads
  //              because each threads access to a file which associated to numa node
  int num_files;

  struct file_info_t *file_info_list;
};

struct fname_base_list_t {
  int num_fname_bases;
  char **fname_bases;
};

/* -----------------------------
 * mmap for struct mempool_t
 * ----------------------------- */
struct mempool_t mmap_pool(size_t sz, int fd, size_t raw_offset_size, size_t *chip_size);
void unmap_pool(struct mempool_t *pool);
void drop_pagecaches_pool(struct mempool_t *pool);
void remove_file(char *fname);
void erasure_pool_file(struct mempool_t *pool, int fd, char *fname);

/* -----------------------------
 * operate dumpfiles_t
 * ----------------------------- */
struct dumpfiles_t *init_dumpfile_info_edgelist(char *suffix, int is_single_thrd);
struct dumpfiles_t *init_dumpfile_info_graph(char *suffix, int is_start, int is_single_thrd);
struct dumpfiles_t *init_dumpfile_info_tree(char *suffix);
struct dumpfiles_t *init_dumpfile_info_hops(char *suffix);
struct dumpfiles_t *init_dumpfile_info_edgelist_bucket(char *suffix, int num_bucket);

void open_files_new(struct dumpfiles_t *DF);
void open_files(struct dumpfiles_t *DF);
void open_files_direct(struct dumpfiles_t *DF);
void open_files_readmode(struct dumpfiles_t *DF);
void open_files_direct_readmode(struct dumpfiles_t *DF);
void fopen_files_new(struct dumpfiles_t *DF);
void fopen_files_readmode(struct dumpfiles_t *DF);

void flush_files(struct dumpfiles_t *DF);
void close_files(struct dumpfiles_t *DF);
void fclose_files(struct dumpfiles_t *DF);
void free_files(struct dumpfiles_t *DF);

/* -----------------------------
 * dump, free edgelist
 * ----------------------------- */
// FIXME: using LFS, dose not need.
#define SMALL_IO_BLOCK_SIZE (1ULL << 30)

void dump_edges(struct edgelist_t *list, struct dumpfiles_t *DF);
void dump_srcs(struct edgelist_t *list);
void dump_edges_fwrite(struct edgelist_t *list, struct dumpfiles_t *DF);
void dump_srcs_fwrite(struct edgelist_t *list);
void read_srcs_fread(struct edgelist_t *list);
void read_large_size(int fd, void *buf, size_t size);
void write_large_size(int fd, const void *buf, size_t size);
void free_edges(struct edgelist_t *list);

/* -----------------------------
 * dump, free subgraph
 * ----------------------------- */
void dump_subgraph(struct subgraph_t *subgraph_list, int num_graphs,
                   struct dumpfiles_t *DF_s, struct dumpfiles_t *DF_e);
void dump_subgraph_mergingmode(struct subgraph_t *subgraph,
                               int num_subgraphs, int num_fullgraphs,
                               int subgraph_no);
void dump_subgraph_fwrite(struct subgraph_t *subgraph_list, int num_graphs,
                          struct dumpfiles_t *DF_s, struct dumpfiles_t *DF_e);
void free_subgraph(struct mempool_t *mempool_list, int num_graphs);
void drop_pagecache_file(char *fname);

/* -----------------------------
 * dump buffer
 * ----------------------------- */
struct sub_dump_buffer_t {
  // I64_t *start; +start
  I64_t *buf;
  I64_t length;
};
struct dump_buffer_t{
  int num_buffer;
  struct mempool_t *buffer_pool;
  struct sub_dump_buffer_t *buffer_list;
};

struct dump_buffer_t *alloc_dump_buffer(int num_buffer);
struct dump_buffer_t *alloc_dump_buffer_with_size(int num_buffer, size_t size);
void free_dump_buffer(struct dump_buffer_t *BF);


/* -----------------------------
 * utility
 * ----------------------------- */
void get_fname_base_for_single_key(char *fname_base_result, const char *key);
void getprocinfo(char *fname, char *foutname, char *comment);
size_t get_free_memsize_byte(void);

#endif //DUMP_H

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#define _FILE_OFFSET_BITS 64


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "dump.h"
#include "common.h"
#include "generation.h"
#include "graph_generator.h"
#include "defs.h"
#include "std_sort.h"
#include "parse_conf_file.h"


/* ------------------------------------------------------------
* read conf file
* ------------------------------------------------------------ */
struct fname_base_list_t get_fname_base_list(const char *fname_base) {
  struct fname_base_list_t fname_base_list;
  const char *delim = FNAME_DELIM;
  char *saveptr = NULL;
  char *token = NULL;
  char *fname_base_work = NULL;
  char *temp_ptr = NULL;
  int i;
  assert(fname_base_work = strdup(fname_base));

  fname_base_list.num_fname_bases = 1;
  for(i = 0; *(fname_base_work + i) != '\0'; ++i) {
    if (*(fname_base_work + i) == *delim) {
      ++fname_base_list.num_fname_bases;
    }
  }
  temp_ptr = fname_base_work;
  assert(fname_base_list.fname_bases = (char **)malloc(fname_base_list.num_fname_bases * sizeof(char *)));
  for (i = 0; i < fname_base_list.num_fname_bases; ++i) {
    assert(fname_base_list.fname_bases[i] = (char *)malloc(MAX_FNAME * sizeof(char)));
  }
  i = 0;
  while((token = strtok_r(temp_ptr, delim, &saveptr)) !=NULL) {
    strcpy(fname_base_list.fname_bases[i], token);
    temp_ptr = NULL;
    ++i;
  }
  while(fname_base_list.num_fname_bases > i) {
    free(fname_base_list.fname_bases[--fname_base_list.num_fname_bases]);
  }
  free(fname_base_work);
#if 0
for(i = 0; i < fname_base_list.num_fname_bases; ++i) {
printf("%d:%s\n", i, fname_base_list.fname_bases[i]);
}
#endif
return fname_base_list;
}

char *get_first_fname_base(const char *fname_base) {
  const char *start, *end;
  start = fname_base;
  const char *delim = FNAME_DELIM;
  while(*start == *delim) {
    ++start;
  }
  end = strchrnul(fname_base, *delim);
  char *first_fname_base = (char *)malloc((end - start + 1) * sizeof(char));
  strncpy(first_fname_base, start, end - start);
  *(first_fname_base + (end - start)) = '\0';
  return first_fname_base;
}


void free_fname_base_list(struct fname_base_list_t fname_base_list) {
  for (int i = 0; i < fname_base_list.num_fname_bases; ++i) {
    free(fname_base_list.fname_bases[i]);
  }
  free(fname_base_list.fname_bases);
}

char *get_conf_file_name(void)
{
  if (getenv(ENV_EXMEM_CONF_FILE))
    return  getenv(ENV_EXMEM_CONF_FILE);
  else
    return  FNAME_EXMEM_CONF;
}

void get_fname_base(char *fname_base_result, const char **key_list)
{
  char *fname = get_conf_file_name();
  struct config_list_t config_list = make_configlist_from_conffile(fname, key_list, 1);

  memset(fname_base_result, '\0', MAX_FNAME);

  strncpy(fname_base_result,
    config_list.value_list[0],
    MIN(MAX_FNAME, strlen(config_list.value_list[0])) );

  free_config_list(config_list);
}

void get_fname_base_for_single_key(char *fname_base_result, const char *key)
{
  const char *key_list[1];
  key_list[0] = key;

  get_fname_base(fname_base_result, key_list);

}



/* ------------------------------------------------------------
* allocate buffer for read
* ------------------------------------------------------------ */
struct dump_buffer_t *alloc_dump_buffer(int num_buffer) {
  const size_t spacing = 64;

  struct dump_buffer_t *BF;
  assert( BF = (struct dump_buffer_t *)calloc(1, sizeof(struct dump_buffer_t)) );

  BF->num_buffer = num_buffer;

  assert( BF->buffer_pool  = (struct mempool_t  *)calloc(BF->num_buffer, sizeof(struct mempool_t )) );
  assert( BF->buffer_list  = (struct sub_dump_buffer_t *)calloc(BF->num_buffer, sizeof(struct sub_dump_buffer_t)) );

/* allocate local memory pool */
  size_t total_alloc_sz = 0;
  int k;
  double t1, t2;
  t1 = get_seconds();
  for (k = 0; k < BF->num_buffer; ++k) {
    size_t sz = 0;
sz += (DUMP_BUF_LENGTH + 1) * sizeof(I64_t) + spacing; // end
sz  = ROUNDUP( sz, hugepage_size() );
BF->buffer_pool[k] = lmalloc(sz, get_numa_nodeid(k));
total_alloc_sz += BF->buffer_pool[k].memsize;
#if 0
printf("[node%02d] BF[%d] (%p) %.2f MB using mmap with mbind(MPOL_BIND:%8s)\n",
       k, k, BF->buffer_pool[k].pool, (double)(BF->buffer_pool[k].memsize) / (1UL<<20)/*(1ULL<<30)*/,
       &num2bit((1ULL << k), 0)[56]);
#endif
}

force_pool_page_faults(BF->buffer_pool);
t2 = get_seconds();
#if 0
printf("numa node local allocation(%.2f MB   %d allocation's) takes %.3f seconds\n",
     (double)(total_alloc_sz / (1UL<<20)), k, t2-t1);
#endif

/*  representation */
for (k = 0; k < BF->num_buffer; ++k) {
  BF->buffer_list[k].length = DUMP_BUF_LENGTH;
  BF->buffer_list[k].buf = (I64_t *)&(BF->buffer_pool[k].pool[0]);
}

return BF;
}

/* ------------------------------------------------------------
* allocate buffer for read
* ------------------------------------------------------------ */
struct dump_buffer_t *alloc_dump_buffer_with_size(int num_buffer, size_t size) {

  struct dump_buffer_t *BF;
  assert( BF = (struct dump_buffer_t *)calloc(1, sizeof(struct dump_buffer_t)) );

  BF->num_buffer = num_buffer;

  assert( BF->buffer_pool  = (struct mempool_t  *)calloc(BF->num_buffer, sizeof(struct mempool_t )) );
  assert( BF->buffer_list  = (struct sub_dump_buffer_t *)calloc(BF->num_buffer, sizeof(struct sub_dump_buffer_t)) );

/* allocate local memory pool */
  size_t total_alloc_sz = 0;
  int k;
  double t1, t2;
  t1 = get_seconds();
  for (k = 0; k < BF->num_buffer; ++k) {
    size_t sz = 0;
sz += size + 1024ULL; // end, 1024byte for DIRECT I/O
sz  = ROUNDUP( sz, hugepage_size() );
BF->buffer_pool[k] = lmalloc(sz, get_numa_nodeid(k));
total_alloc_sz += BF->buffer_pool[k].memsize;
#if 0
printf("[node%02d] BF[%d] (%p) %.2f MB using mmap with mbind(MPOL_BIND:%8s)\n",
       k, k, BF->buffer_pool[k].pool, (double)(BF->buffer_pool[k].memsize) / (1UL<<20)/*(1ULL<<30)*/,
       &num2bit((1ULL << k), 0)[56]);
#endif
}
force_pool_page_faults(BF->buffer_pool);
t2 = get_seconds();
#if 0
printf("numa node local allocation(%.2f MB   %d allocation's) takes %.3f seconds\n",
     (double)(total_alloc_sz / (1UL<<20)), k, t2-t1);
#endif

/*  representation */
for (k = 0; k < BF->num_buffer; ++k) {
  BF->buffer_list[k].length = DUMP_BUF_LENGTH;
  BF->buffer_list[k].buf = (I64_t *)&(BF->buffer_pool[k].pool[0]);
}

return BF;
}


/* ------------------------------------------------------------
* free buffer
* ------------------------------------------------------------ */
void free_dump_buffer(struct dump_buffer_t *BF) {
  int k;
  for( k=0; k<BF->num_buffer; k++){
    lfree(BF->buffer_pool[k]);
  }
  free(BF->buffer_pool);
  free(BF->buffer_list);
  free(BF);
}


/* ------------------------------------------------------------
* allcate dumpfile_t and set file name
* ------------------------------------------------------------ */
// set dump file name to dumpfiles_t for Edgelists
struct dumpfiles_t *init_dumpfile_info_edgelist(char *suffix, int is_single_thrd)
{
  int edgelists = get_numa_online_nodes();
  int threads =   is_single_thrd ? 1 : get_numa_num_threads();
  int num_files = threads * edgelists; // each threads access to all edgelist files

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_EDGELIST);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);

  // allocate file info list and set basic info
  struct dumpfiles_t *DF = (struct dumpfiles_t *)calloc(1, sizeof(struct dumpfiles_t));
  DF->num_files = num_files;
  DF->file_info_list = (struct file_info_t *)calloc(num_files, sizeof(struct file_info_t));

  int id, e;
  for (id=0; id<threads; id++) {
    for (e=0; e<edgelists; e++) {
      sprintf(DF->file_info_list[id*edgelists+e].fname,
        "%.128s_edges_SCALE%d_%d_%.128s",
        fname_base_list.fname_bases[e % fname_base_list.num_fname_bases],
        SCALE,
        e,
        suffix);
    }
  }

  return DF;
}



// set dump file name to dumpfiles_t for graph
// is_start == TRUE  : [fname_base]_FG_start_[numa node ID]_[suffix]
// is_start == FALSE : [fname_base]_FG_end_[numa node ID]_[suffix]
struct dumpfiles_t *init_dumpfile_info_graph(char *suffix, int is_start, int is_single_thrd)
{
  int threads = get_numa_num_threads();
  int nodes = get_numa_online_nodes();
  int num_files = (is_single_thrd) ? nodes : threads;

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_GRAPH);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);


// allocate file info list and set basic info
  struct dumpfiles_t *DF = (struct dumpfiles_t *)calloc(1, sizeof(struct dumpfiles_t));
  DF->num_files = num_files;
  DF->file_info_list = (struct file_info_t *)calloc(num_files,
    sizeof(struct file_info_t));
  int k;
  char *prefix = (is_start) ? "%.128s_BG_start_SCALE%d_%d_%.128" : "%.128s_BG_end_SCALE%d_%d_%.128";

  for (k=0; k<num_files; k++) {
    int nodeid = (is_single_thrd) ? k : get_numa_nodeid(k);
    sprintf(DF->file_info_list[k].fname,
      prefix,
      fname_base_list.fname_bases[nodeid % fname_base_list.num_fname_bases],
      SCALE,
      nodeid,
      suffix);

  }

  return DF;
}

struct dumpfiles_t *init_dumpfile_info_tree(char *suffix)
{
  int threads = get_numa_num_threads();
  int num_files = threads;

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_TREE);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);

// allocate file info list and set basic info
  struct dumpfiles_t *DF = (struct dumpfiles_t *)calloc(1, sizeof(struct dumpfiles_t));
  DF->num_files = num_files;
  DF->file_info_list = (struct file_info_t *)calloc(num_files,
    sizeof(struct file_info_t));
  int k;
  for (k=0; k<num_files; k++) {
    sprintf(DF->file_info_list[k].fname,
      "%.128s_tree_SCALE%d_%.128s",
      fname_base_list.fname_bases[get_numa_nodeid(k) % fname_base_list.num_fname_bases],
      SCALE,
      suffix);
  }

  return DF;
}

struct dumpfiles_t *init_dumpfile_info_hops(char *suffix)
{
  int threads = get_numa_num_threads();
  int num_files = threads;

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_HOPS);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);

// allocate file info list and set basic info
  struct dumpfiles_t *DF = (struct dumpfiles_t *)calloc(1, sizeof(struct dumpfiles_t));
  DF->num_files = num_files;
  DF->file_info_list = (struct file_info_t *)calloc(num_files,
    sizeof(struct file_info_t));
  int k;
  for (k=0; k<num_files; k++) {
    sprintf(DF->file_info_list[k].fname,
      "%.128s_hops_SCALE%d_%.128s",
      fname_base_list.fname_bases[get_numa_nodeid(k) % fname_base_list.num_fname_bases],
      SCALE,
      suffix);
  }

  return DF;
}

/* ------------------------------------------------------------
* allcate dumpfile_t and set file name
* ------------------------------------------------------------ */
// set dump file name to dumpfiles_t for Edgelists
struct dumpfiles_t *init_dumpfile_info_edgelist_bucket(char *suffix, int num_bucket)
{

  int num_files = num_bucket;

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_EDGEBCKT);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);

  // allocate file info list and set basic info
  struct dumpfiles_t *DF = (struct dumpfiles_t *)calloc(1, sizeof(struct dumpfiles_t));
  DF->num_files = num_files;
  DF->file_info_list = (struct file_info_t *)calloc(num_files, sizeof(struct file_info_t));

  int k;
  for (k=0; k<num_files; k++) {
    sprintf(DF->file_info_list[k].fname,
      "%.128s_edgesbckt_SCALE%d_%d_%.128s",
      fname_base_list.fname_bases[k % fname_base_list.num_fname_bases],
      SCALE,
      k,
      suffix);
  }

  return DF;

}

/* ------------------------------------------------------------
* flush files
* ------------------------------------------------------------ */
void flush_files(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    fflush(DF->file_info_list[k].fp);
  }
}

/* ------------------------------------------------------------
* free files
* ------------------------------------------------------------ */
void free_files(struct dumpfiles_t *DF) {
  free(DF->file_info_list);
  free(DF);
}

/* ------------------------------------------------------------
* open files
* ------------------------------------------------------------ */
void open_files_new(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
    finfo->fd = open(finfo->fname, O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
#if 0
    printf("file openned(new) [%s]\n",finfo->fname);
#endif
  }
}

/* ------------------------------------------------------------
* open files
* ------------------------------------------------------------ */
void open_files(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
    finfo->fd = open(finfo->fname, O_RDWR);
#if 0
    printf("file openned(read/write mode) [%s]\n",finfo->fname);
#endif
  }
}

/* ------------------------------------------------------------
* open files read mode
* ------------------------------------------------------------ */
void open_files_readmode(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
    finfo->fd = open(finfo->fname, O_RDONLY);
#if 0
printf("file openned(read only mode) [%s]\n",finfo->fname);
#endif
}
}

/* ------------------------------------------------------------
* open files direct
* ------------------------------------------------------------ */
void open_files_direct(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
#ifdef O_DIRECT
    finfo->fd = open(finfo->fname, O_RDWR|O_CREAT|O_TRUNC|O_DIRECT, S_IREAD|S_IWRITE);
#else
#warning O_DIRECT is not define
    printf("open a file NOT O_DIRECT mode [%s]\n", finfo->fname);
    finfo->fd = open(finfo->fname, O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
#endif
#if 0
printf("file openned [%s]\n",finfo->fname);
#endif
}
}

/* ------------------------------------------------------------
* open files direct
* ------------------------------------------------------------ */
void open_files_direct_readmode(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
#ifdef O_DIRECT
    finfo->fd = open(finfo->fname, O_RDONLY|O_DIRECT);
#else
#warning O_DIRECT is not define
    printf("open a file NOT O_DIRECT mode [%s]\n", finfo->fname);
    finfo->fd = open(finfo->fname, O_RDONLY);
#endif
#if 0
printf("file openned [%s]\n",finfo->fname);
#endif
}
}

/* ------------------------------------------------------------
* fopen files
* ------------------------------------------------------------ */
void fopen_files_new(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
#if 0
printf("open file[%s]\n",finfo->fname);
#endif
finfo->fp = fopen(finfo->fname, "wb+");
assert(finfo->fp);
}
}

/* ------------------------------------------------------------
* fopen files read mode
* ------------------------------------------------------------ */
void fopen_files_readmode(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    struct file_info_t *finfo = &DF->file_info_list[k];
#if 0
printf("open file(read mode) [%s]\n",finfo->fname);
#endif
finfo->fp = fopen(finfo->fname, "rb");
assert(finfo->fp);
}
}


/* ------------------------------------------------------------
* close files
* ------------------------------------------------------------ */
void close_files(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    close(DF->file_info_list[k].fd);
  }
}

/* ------------------------------------------------------------
* close files
* ------------------------------------------------------------ */
void fclose_files(struct dumpfiles_t *DF) {
  int k;
  for (k=0; k<DF->num_files; k++) {
    fclose(DF->file_info_list[k].fp);
  }
}

/* ------------------------------------------------------------
* dump edgelist
* ------------------------------------------------------------ */
void dump_edges(struct edgelist_t *list, struct dumpfiles_t *DF) {

  int k;

  printf("dump edges in {\n");
  for (k=0; k<list->num_lists; k++) {
    printf(" %d : [ %s ]\n", k, DF->file_info_list[k].fname);
#if 0
printf("dump edges[%d] into %s(%lld edges.)\n", k, DF->file_info_list[k].fname, list->IJ_list[k].length);
#endif
write_large_size(DF->file_info_list[k].fd, list->IJ_list[k].edges,
 sizeof(struct packed_edge) * list->IJ_list[k].length);
}
printf("}\n");

}


// FIXME: By using Large File Sysytem, does not need this?
/* ------------------------------------------------------------
* read(2) data by blocksize
* ------------------------------------------------------------ */
void read_large_size(int fd, void *buf, size_t size)
{
  size_t block_size = SMALL_IO_BLOCK_SIZE;
  size_t num_loop = size / block_size;
  size_t remains = size - num_loop * block_size;

  size_t k, readed_length=0;
  for (k=0; k<num_loop; k++) {
    readed_length = read(fd, buf+k*block_size, block_size);
    assert(readed_length == block_size);
//printf("count = %lld\n", readed_length);
  }

  if (remains > 0) {
    readed_length = read(fd, buf+num_loop*block_size, remains);
    assert(readed_length == remains);
//printf("count = %lld\n", readed_length);
  }

}

/* ------------------------------------------------------------
* write(2) data by blocksize
* ------------------------------------------------------------ */
void write_large_size(int fd, const void *buf, size_t size)
{
  size_t block_size = SMALL_IO_BLOCK_SIZE;
  size_t num_loop = size / block_size;
  size_t remains = size - num_loop * block_size;

  size_t k, written_length=0;
  for (k=0; k<num_loop; k++) {
    written_length = write(fd, buf+k*block_size, block_size);
    assert(written_length == block_size);
//printf("count = %lld\n", written_length);
  }

  if (remains > 0) {
    written_length = write(fd, buf+num_loop*block_size, remains);
    assert(written_length == remains);
//printf("count = %lld\n", written_length);
  }

}


/* ------------------------------------------------------------
* dump edgelist by fwrite
* ------------------------------------------------------------ */
void dump_edges_fwrite(struct edgelist_t *list, struct dumpfiles_t *DF) {

  int k;

  for (k=0; k<list->num_lists; k++) {
#if 0
printf("dump edges[%d] into %s(%lld edges.)\n", k, DF->file_info_list[k].fname, list->IJ_list[k].length);
#endif
I64_t written_length;
written_length = fwrite(list->IJ_list[k].edges,
  sizeof(struct packed_edge),
  list->IJ_list[k].length,
  DF->file_info_list[k].fp);
assert(written_length == list->IJ_list[k].length);
}

}

/* ------------------------------------------------------------
* dump srcs by fwrite
* ------------------------------------------------------------ */
void dump_srcs_fwrite(struct edgelist_t *list) {

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_SRCS);
  if (fname_base[0] == '\0') {
    get_fname_base_for_single_key(fname_base, CONFIG_EDGELIST);
  }
  char *fname = CALLOCA(strlen(fname_base) + 64);

  sprintf(fname, "%.*s_edges_srcs_SCALE%d",MAX_FNAME, fname_base, SCALE);

  FILE *fp = fopen(fname, "wb");
  assert(fp);

  fwrite(&list->numsrcs, sizeof(I64_t), 1, fp);
  fwrite(list->srcs, sizeof(I64_t), list->numsrcs, fp);

  printf("store srcs in [ %s ]\n", fname);

  fclose(fp);
}

// TODO: make read_srcs
/* ------------------------------------------------------------
* read srcs by fread
* ------------------------------------------------------------ */
void read_srcs_fread(struct edgelist_t *list) {

  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_SRCS);
  char *fname = CALLOCA(strlen(fname_base + 64));
  sprintf(fname, "%.*s_edges_srcs_SCALE%d",MAX_FNAME, fname_base, SCALE);

  FILE *fp = fopen(fname, "rb");
  assert(fp);

  I64_t num_srcs;
  fread(&num_srcs, sizeof(I64_t), 1, fp);
  list->numsrcs = num_srcs;
  fread(list->srcs, sizeof(I64_t), num_srcs, fp);

  printf("read srcs from %s\n", fname);

  fclose(fp);
}

/* ------------------------------------------------------------
* free edges
* ------------------------------------------------------------ */
void free_edges(struct edgelist_t *list) {
  int k;
  for (k = 0; k < list->num_lists; ++k) {
    lfree(list->pool[k]);
  }
}


/* ------------------------------------------------------------
* dump subgraph
* ------------------------------------------------------------ */
void dump_subgraph(struct subgraph_t *subgraph_list, int num_graphs,
  struct dumpfiles_t *DF_s, struct dumpfiles_t *DF_e)
{
  int k;

  printf ("dump graph into {\n");
  for (k=0; k<num_graphs; k++) {
    printf(" %d : [", k);
    //printf("dump graph into %s %s\n", file_info_s->fname, file_info_e->fname);

    // dump "start" data
    if (DF_s) {
      struct file_info_t *file_info_s = &DF_s->file_info_list[k];
      write_large_size(file_info_s->fd,
       subgraph_list[k].start,
       sizeof(I64_t) * (subgraph_list[k].n+1));
      printf(" start = %s;", file_info_s->fname);
    }

    // dump "end" data
    if (DF_e) {
      struct file_info_t *file_info_e = &DF_e->file_info_list[k];
      write_large_size(file_info_e->fd,
       subgraph_list[k].end,
       sizeof(I64_t)*subgraph_list[k].m+1);
      printf(" end = %s;", file_info_e->fname);
    }
    printf(" ]\n");
  }
  printf("}\n");
}

void dump_subgraph_mergingmode(struct subgraph_t *subgraph,
 int num_subgraphs, int num_fullgraphs, int subgraph_no){

  int lgraphs = (num_subgraphs / num_fullgraphs);
  int target_no = subgraph_no / lgraphs;

  printf ("dump graph into {\n");
  char *fname_base = CALLOCA(MAX_FNAME);
  get_fname_base_for_single_key(fname_base, CONFIG_GRAPH);
  struct fname_base_list_t fname_base_list = get_fname_base_list(fname_base);

// dump "start" data
  char *fname_s = CALLOCA(MAX_FNAME);
  sprintf(fname_s,
    "%.128s_BG_start_SCALE%d_%d_%.128s",
    fname_base_list.fname_bases[target_no % fname_base_list.num_fname_bases],
    SCALE,
    target_no,
    "");

  int fd_s;
  if (subgraph_no % lgraphs == 0) {
    fd_s = open(fname_s, O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
  } else {
    fd_s = open(fname_s, O_RDWR);
  }
  if (fd_s != -1) {

    int is_last = (subgraph_no % lgraphs) == (lgraphs - 1);
    size_t length = 0;
    if (is_last) length = subgraph->n+1;
    else         length = subgraph->n;

    lseek(fd_s, 0, SEEK_END);
    write_large_size(fd_s,
     subgraph->start,
     sizeof(I64_t) * length);

#if 0
    for(I64_t k = 0; k < length; k++) {
      fprintf(stderr, "%lld\n", subgraph->start[k]);
    }
#endif
    printf(" start = %s;", fname_s);
  }
  close(fd_s);

// dump "end" data
    char *fname_e = CALLOCA(MAX_FNAME);
  sprintf(fname_e,
    "%.128s_BG_end_SCALE%d_%d_%.128s",
    fname_base_list.fname_bases[target_no % fname_base_list.num_fname_bases],
    SCALE,
    target_no,
    "");
  int fd_e;
  if (subgraph_no % lgraphs == 0) {
    fd_e = open(fname_e, O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
  } else {
    fd_e = open(fname_e, O_RDWR);
  }
  if (fd_e != -1) {
    lseek(fd_e, 0, SEEK_END);
    write_large_size(fd_e,
     subgraph->end,
     sizeof(I64_t)*subgraph->m);
    printf(" end = %s;", fname_e);
  }
#if 0
  for(I64_t k = 0; k < subgraph->m; k++) {
    fprintf(stderr, "%lld\n", subgraph->end[k]);
  }
#endif
  close(fd_e);
  printf("}\n");
}


/* ------------------------------------------------------------
* dump subgraph by fwrite
* ------------------------------------------------------------ */
void dump_subgraph_fwrite(struct subgraph_t *subgraph_list, int num_graphs,
  struct dumpfiles_t *DF_s, struct dumpfiles_t *DF_e)
{
  int k;

  printf ("dump graph into {\n");
  for (k=0; k<num_graphs; k++) {
    printf(" %d : [", k);

      I64_t written_length;

// dump "start" data
      if (DF_s) {
        struct file_info_t *file_info_s = &DF_s->file_info_list[k];
        written_length = fwrite(subgraph_list[k].start,
          sizeof(I64_t),
          subgraph_list[k].n+1,
          file_info_s->fp);
        assert(written_length == (subgraph_list[k].n+1));
        printf(" start = %s;", file_info_s->fname);
      }

// dump "end" data
      if (DF_e) {
        struct file_info_t *file_info_e = &DF_e->file_info_list[k];
        written_length = fwrite(subgraph_list[k].end,
          sizeof(I64_t),
          subgraph_list[k].m+1,
          file_info_e->fp);
        assert(written_length == (subgraph_list[k].m+1));
        printf(" end = %s;", file_info_e->fname);
      }
      printf(" ]\n");
    }
    printf("}\n");
  }

/* ------------------------------------------------------------
* free subgraph
* ------------------------------------------------------------ */
void free_subgraph(struct mempool_t *mempool_list, int num_graphs) {

  int k;

  for (k = 0; k < num_graphs; ++k) {
    lfree(mempool_list[k]);
  }

}


/* ------------------------------------------------------------
*  drop page caches
* ------------------------------------------------------------ */
void drop_pagecache_file(char *fname)
{

#if _BSD_SOURCE || _XOPEN_SOURCE || /* glibc 2.8 以降では: */ _POSIX_C_SOURCE >= 200112L
  int err;
  int fd = open(fname, O_RDONLY);

  if (fd == -1) return;
  err = fsync(fd);

  if (err) perror("fsync");

#if _XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L

  struct stat sb;
  fstat(fd, &sb);

  err = posix_fadvise(fd,
    0,
    sb.st_size,
    POSIX_FADV_DONTNEED);
  if (err)  perror("posix_fadvise : POSIX_FADV_DONTNEED");

#else
#warning posix_fadvise is not defined
#endif

  close(fd);

#else
#warning fsync is not defined
#endif

}


/* -----------------------------
* mmap for mempool_t
* ----------------------------- */

struct mempool_t mmap_pool(size_t sz, int fd, size_t raw_offset_size, size_t *chip_size) {

  size_t pgsz = hugepage_size();

  if (sz == 0) {
    struct stat statbuf;
    assert(fstat (fd, &statbuf) == 0);
    sz = statbuf.st_size;
  }

  size_t offset_size = raw_offset_size & (0xFFFFFFFFFFFFFFFF - pgsz + 1UL);
  size_t chip_sz = raw_offset_size - offset_size;

  struct mempool_t pool;
  pool.memsize = ROUNDUP(sz + chip_sz, pgsz);
  pool.nodeid  = -1;

  // extend file if file size is smaller than mapped size
  struct stat statbuf;
  assert(fstat (fd, &statbuf) == 0);
  if (statbuf.st_size < pool.memsize) {
    if (ftruncate (fd, pool.memsize) != 0) {
      perror("ftruncate at mmap_pool");
    }
  }


  unsigned char *map = (unsigned char *)mmap(0, pool.memsize, PROT_READ | PROT_WRITE,  MAP_SHARED, fd, offset_size);
  if ( map == MAP_FAILED ) {
    perror("mmap_file");
    exit(1);
  }
  pool.pool = (unsigned char *)&map[0];
  if (chip_size != NULL) *chip_size = chip_sz;

  return pool;
}

void unmap_pool(struct mempool_t *pool) {

  if ( !pool       ) return ;
  if ( !pool->pool ) return ;
  if (munmap(pool->pool, pool->memsize) == -1) {
    perror("unmap_pool");
  }
  pool->pool = NULL;

}

/* ------------------------------------------------------------
*  drop page caches for pool
* ------------------------------------------------------------ */
void drop_pagecaches_pool(struct mempool_t *pool)
{

  int err;
  err = msync(pool->pool, pool->memsize, MS_SYNC);
  if (err) {
    perror("drop_pagecaches_pool:msync");
  }

  err = madvise(pool->pool, pool->memsize, MADV_DONTNEED);
  if (err) {
    perror("drop_pagecaches_pool:madvise");
  }

}


/* ------------------------------------------------------------
*  drop page cache and unmap poop
*  close a file
*  remove a file
* ------------------------------------------------------------ */
void remove_file(char *fname)
{
  if ( remove(fname) == -1) {
    perror("remove_file");
  }
}


void erasure_pool_file(struct mempool_t *pool, int fd, char *fname)
{

  drop_pagecaches_pool(pool);
  unmap_pool(pool);

  if (close(fd) == -1) {
    perror("close file");
  } else {
    remove_file(fname);
  }

}


/* ------------------------------------------------------------
*  get proc info
* ------------------------------------------------------------ */

void getprocinfo(char *fname, char *foutname, char *comment)
{
  int fd;
  char path[100];
  char array[4096];
  FILE *fout;

  snprintf(path, 100, "/proc/%s", fname);
  fd = open(path, O_RDONLY);

  if (fd == -1) {
    fprintf(stderr, "Wrong procfile\n");
    return ;
  }

  fout = fopen(foutname, "a+");
  if (fout == NULL) {
    fprintf(stderr, "faild open file %s", fname);
    return ;
  }

  fprintf(fout, "%s\n", comment);

  size_t len;
  lseek(fd, 0, SEEK_SET);
  while (1) {
    len = read(fd, array, 4095);
    if (len == 0)
      break;
    array[len] = '\0';
    fprintf(fout, "%s", array);
  }

  fclose(fout);

}


//
// Note: This is not thread safe
//
size_t get_free_memsize_byte(void)
{
  FILE *fp;
  char array[1025];

  fp = fopen("/proc/vmstat", "rb");
  if (fp == NULL) {
    printf("Can't open /proc/vmstat\n");
    return 0;
  }

  size_t size = 0;
  I64_t value;

  fseek(fp, 0, SEEK_SET);
  while (fscanf(fp, "%1024s %lld", array, &value) != EOF) {

    if (strstr(array, "nr_free_pages")) {
    //size = (size_t)strtoll(array, NULL, 0);
      size = getpagesize() * (U64_t)value;
      break;
    }

  }

  fclose(fp);

  return size;

}

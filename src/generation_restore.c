/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Originally written by Yuichiro Yasui
 * This version considerably modified by The GraphCREST Project
 *
 * Copyright (C) 2011-2013 Yuichiro Yasui
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "generation.h"
#include "dump.h"

/* ------------------------------------------------------------
 * graph_generation
 * ------------------------------------------------------------ */
static struct edgelist_t *allocate_edgelist_info(int scale, int edgefactor, int num_lists, I64_t nbfs);


struct edgelist_t *graph_generation(int scale, int edgefactor, int num_lists, I64_t nbfs) {
    struct edgelist_t *list = NULL;
    double t1, t2;

    /* allocation */
    t1 = get_seconds();
    assert( list = allocate_edgelist_info(scale, edgefactor, num_lists, nbfs) );
    t2 = get_seconds();
    printf("numa node local allocation takes %.3f seconds\n", t2-t1);

    printf("No kronecker graph generation\n");

    read_srcs_fread(list);
    printf("read srcs done.\n");

    return list;
}


static struct edgelist_t *allocate_edgelist_info(int scale, int edgefactor, int num_lists, I64_t nbfs) {
    struct edgelist_t *list = NULL;
    assert( list = (struct edgelist_t *)calloc(1, sizeof(struct edgelist_t)) );

    list->num_nodes = 1ULL << scale;
    list->num_edges = list->num_nodes * edgefactor;
    list->num_lists = num_lists;
    assert( list->IJ_list = (struct IJ_list_t *)calloc(list->num_lists+1, sizeof(struct IJ_list_t)) );
    assert( list->pool    = (struct mempool_t *)calloc(list->num_lists+1, sizeof(struct mempool_t)) );

    list->numsrcs = nbfs;
    assert( list->srcs = (I64_t *)calloc(nbfs, sizeof(I64_t)) );

    int k;
    I64_t chunk = ROUNDUP( list->num_edges/list->num_lists, nbfs );
    for (k = 0; k < list->num_lists; ++k) {
        list->IJ_list[k].offset = chunk * k;
    }
    list->IJ_list[k].offset = list->num_edges;
    for (k = 0; k < list->num_lists; ++k) {
        list->IJ_list[k].length = list->IJ_list[k+1].offset - list->IJ_list[k].offset;
    }
    list->IJ_list[k].length = 0;

    printf("No edges memory allocation\n");

    return list;
}


/* ------------------------------------------------------------
 * dump edgelist
 * ------------------------------------------------------------ */
int dump_edgelist(const char *filename, struct edgelist_t *list) {
    FILE *fp = NULL;
    assert( fp = fopen(filename, "w") );
    if (!fp) {
        return 1;
    } else {
        int k;
        fprintf(fp, "p sp %lld %lld\n", list->num_nodes, list->num_edges);
        for (k = 0; k < list->num_lists; ++k) {
            I64_t e;
            for (e = 0; e < list->IJ_list[k].length; ++e) {
                const I64_t v = get_v0_from_edge(&list->IJ_list[k].edges[e]);
                const I64_t w = get_v1_from_edge(&list->IJ_list[k].edges[e]);
                fprintf(fp, "a %lld %lld 1\n", v+1, w+1);
            }
        }
        fclose(fp);
        return 0;
    }
}




/* ------------------------------------------------------------
 * free edge list
 * ------------------------------------------------------------ */
void free_edgelist(struct edgelist_t *list) {
//    int k;
//    for (k = 0; k < list->num_lists; ++k) {
//        lfree(list->pool[k]);
//    }
    free(list->IJ_list);
    free(list->pool);
    free(list->srcs);
    free(list);
}

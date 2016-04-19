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

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__linux__)
#  include <sys/sysinfo.h>
#elif defined(__WIN32__) || defined(__WIN64__) || defined(__CYGWIN32__) || defined(__CYGWIN64__)
#  include <windows.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#endif
#include <ulibc.h>


/* ------------------------------------------------------------ *
 * pagesize, ramsize
 * ------------------------------------------------------------ */
static size_t   __page_size       = 0;
static size_t   __hugepage_size   = 0;
static size_t   __total_ram       = 0;
static size_t   __total_hugepages = 0;

void __attribute__ ((constructor)) set_pagesize(void) {
  /* set pagesize */
#ifdef WIN32
  __page_size = 4096;
  __hugepage_size = 4096;
#else
  __page_size = getpagesize(); /* sysconf(_SC_PAGESIZE); */
  __hugepage_size = get_meminfo(MF_HUGEPAGESIZE);
#endif

  if ( !__hugepage_size ) __hugepage_size = __page_size;

  /* total memory size */
  __total_ram = get_meminfo(MF_MEMTOTAL);
  __total_hugepages = get_meminfo(MF_HUGEPAGES_TOTAL);
}

/* pagesize */
size_t     page_size(void) { return __page_size;     }
size_t hugepage_size(void) { return __hugepage_size; }

/* memory size */
size_t total_memsize(void) { return __total_ram; }
size_t alloc_memsize(void) { return __total_ram - get_meminfo(MF_MEMFREE); }
size_t  free_memsize(void) { return get_meminfo(MF_MEMFREE); }


/* hugepage size */
size_t total_hugepages(void) { return __total_hugepages; }
size_t alloc_hugepages(void) {
  return __total_hugepages - get_meminfo(MF_HUGEPAGES_FREE);
}
size_t  free_hugepages(void) { return get_meminfo(MF_HUGEPAGES_FREE); }


/* ------------------------------------------------------------ *
 * int is_linux(void)
 * int is_macosx(void)
 * int is_windows(void)
 * ------------------------------------------------------------ */
int is_linux(void) {
#if defined(linux)
  return 1;
#endif
  return 0;
}
int is_macosx(void) {
#if defined(__FreeBSD__) || defined(__APPLE__)
  return 1;
#endif
  return 0;
}
int is_windows(void) {
#if defined(__WIN32__) || defined(__WIN64__) || defined(__CYGWIN32__) || defined(__CYGWIN64__)
  return 1;
#endif
  return 0;
}


static void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx) {
  __asm__ __volatile__
    ("cpuid"
     : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
     : "a" (op)
     : "cc");
}


/* ------------------------------------------------------------ *
 * int apicid(void)
 * ------------------------------------------------------------ */
int get_apicid(void) {
  int eax, ebx, ecx, edx;
  __asm__ __volatile__
    ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "0" (1));
  return ((ebx >> 24) & 0xff);
}

/* ------------------------------------------------------------ *
 * char *cpuname(void)
 * ------------------------------------------------------------ */
static char *cpuid_cpuname(void) {
  int eax, ebx, ecx, edx;
  static int name[12] = {0};
  cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
  if ( (eax & 0xffff) >= 4 ) {
    cpuid(0x80000002, &name[0], &name[1], &name[ 2], &name[ 3]);
    cpuid(0x80000003, &name[4], &name[5], &name[ 6], &name[ 7]);
    cpuid(0x80000004, &name[8], &name[9], &name[10], &name[11]);
  }
  return (char *)name;
}
char *cpuname(void) {
  int i, j, k;
  static char name[48];
  strcpy(name, cpuid_cpuname());
  name[strlen(name)] = '\0';
  for (i = 0; name[i]; ++i) {
    if (name[i] == ' ') {
      for (j = k = i; name[j] == ' '; ++j);
      if (j > ++k) {
	while ((name[k++] = name[j++])) ;
	name[k] = '\0';
      }
    }
  }
  return name;
}


/* ------------------------------------------------------------ *
 * char *cpuname(void)
 * ------------------------------------------------------------ */
int get_num_cores(void) {
#if defined(linux)
  return get_nprocs();
#elif defined(__WIN32__) || defined(__WIN64__) || defined(__CYGWIN32__) || defined(__CYGWIN64__)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#elif defined(__FreeBSD__) || defined(__APPLE__)
  int mib[2], ncpus;
  size_t len;
  mib[0] = CTL_HW;
  mib[1] = HW_NCPU;
  len = sizeof(int);
  sysctl(mib, 2, &ncpus, &len, NULL, 0);
  return ncpus;
#else
  return 1;
#endif
}


/* ------------------------------------------------------------ *
 * double mfreq(void)
 * ------------------------------------------------------------ */
#if defined(__x86_64__)
static inline unsigned long long rpcc(void) {
  unsigned long long a, d;
  __asm__ __volatile__ ("rdtsc" : "=a" (a), "=d" (d));
  return a | (d << 32);
}
#elif defined(__i386__)
static inline unsigned long long rpcc(void) {
  unsigned long long ret;
  __asm__ __volatile__ ("rdtsc" : "=A" (ret));
  return ret;
}
#else
static inline unsigned long long rpcc(void) {
  return 0;
}
#endif

double mfreq(void) {
  unsigned long long ts1, ts2, td1, td2;
  td1 = get_usecs();
  ts1 = rpcc();
  usleep(50000); /* 50 msec. */
  ts2 = rpcc();
  td2 = get_usecs();
  return (double)(ts2-ts1) / (td2-td1);
}


/* ------------------------------------------------------------ *
 * size_t get_ramsize(void)
 * ------------------------------------------------------------ */
size_t get_ramsize(void) {
  size_t ramsz;
#if defined(__APPLE__)
  int mib[2] = {CTL_HW, HW_MEMSIZE};
  size_t len = sizeof(size_t);
  sysctl(mib, 2, &ramsz, &len, NULL, 0);
#elif defined(__linux__)
  struct sysinfo info;
  sysinfo(&info);
  ramsz = info.totalram;
#else
  size_t phys_pg, pgsz;
  phys_pg = sysconf(_SC_PHYS_PAGES);
  pgsz = sysconf(_SC_PAGESIZE);
  ramsz = phys_pg * pgsz;
#endif
  return ramsz;
}


/* ------------------------------------------------------------ *
 * size_t get_meminfo(int flag);
 * ------------------------------------------------------------ */
const struct minfo_t {
  int flag;
  const char *prefix;
  size_t bytes;
} minfo[] = {
  { MF_MEMTOTAL,        "MemTotal",        (1<<10) },
  { MF_MEMFREE,         "MemFree",         (1<<10) },
  { MF_BUFFERS,         "Buffers",         (1<<10) },
  { MF_CACHED,          "Cached",          (1<<10) },
  { MF_SWAPCACHED,      "SwapCached",      (1<<10) },
  { MF_ACTIVE,          "Active",          (1<<10) },
  { MF_INACTIVE,        "Inactive",        (1<<10) },
  { MF_HIGHTOTAL,       "HighTotal",       (1<<10) },
  { MF_HIGHFREE,        "HighFree",        (1<<10) },
  { MF_LOWTOTAL,        "LowTotal",        (1<<10) },
  { MF_LOWFREE,         "LowFree",         (1<<10) },
  { MF_SWAPTOTAL,       "SwapTotal",       (1<<10) },
  { MF_SWAPFREE,        "SwapFree",        (1<<10) },
  { MF_DIRTY,           "Dirty",           (1<<10) },
  { MF_WRITEBACK,       "Writeback",       (1<<10) },
  { MF_ANONPAGES,       "AnonPages",       (1<<10) },
  { MF_MAPPED,          "Mapped",          (1<<10) },
  { MF_SLAB,            "Slab",            (1<<10) },
  { MF_PAGETABLES,      "PageTables",      (1<<10) },
  { MF_NFS_UNSTABLE,    "NFS_Unstable",    (1<<10) },
  { MF_BOUNCE,          "Bounce",          (1<<10) },
  { MF_COMMITLIMIT,     "CommitLimit",     (1<<10) },
  { MF_COMMITTED_AS,    "Committed_AS",    (1<<10) },
  { MF_VMALLOCTOTAL,    "VmallocTotal",    (1<<10) },
  { MF_VMALLOCUSED,     "VmallocUsed",     (1<<10) },
  { MF_VMALLOCCHUNK,    "VmallocChunk",    (1<<10) },
  { MF_HUGEPAGES_TOTAL, "HugePages_Total",     (1) },
  { MF_HUGEPAGES_FREE,  "HugePages_Free",      (1) },
  { MF_HUGEPAGES_RSVD,  "HugePages_Rsvd",      (1) },
  { MF_HUGEPAGESIZE,    "Hugepagesize",    (1<<10) },
  { -1, NULL, 0 }
};


const char *get_meminfo_name(int flag) {
  int i;
  for (i = 0; minfo[i].prefix; ++i) {
    if (flag == minfo[i].flag) {
      return minfo[flag].prefix;
    }
  }
  return NULL;
}


size_t get_meminfo(int flag) {
  FILE *fp = NULL;
  char line[256] = {0}, name[256] = {0}, format[256] = {0};
  size_t val = 0;

  fp = fopen(MEMINFO_FILE, "r");
  if (!fp) {
    return 0;
  }

  const char *s = get_meminfo_name(flag);
  if (s) {
    sprintf(format, "%s: %%lu %%*s\n", s);
    strcpy(name, s);
  } else {
    fclose(fp);
    return 0;
  }


  while ( fgets(line, sizeof(line), fp) ) {
    if ( !strncmp(line, name, strlen(name)) ) {
      if ( sscanf(line, format, &val) != 1 ) {
	printf("error : %s\n", line);
      } else {
	break;
      }
    }
  }

  fclose(fp);
  return val * minfo[flag].bytes;
}




/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

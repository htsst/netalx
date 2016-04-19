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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "ulibc.h"


/* ------------------------------------------------------------
 * unsigned long long bit2num(char *bit)
 * char *num2bit(unsigned long long x, int is_space)
 * ------------------------------------------------------------ */
unsigned long long bit2num(char *bit) {
  unsigned long long n=0;
  char *s;
  for (s = bit; *s != '\0'; ++s) {
    switch (*s) {
    case '1' : n = (n<<1) | 1; break;
    case '0' : n = (n<<1);     break;
    default  : /* skip */      break;
    }
  }
  return n;
}

char *num2bit(unsigned long long x, int is_space) {
  static char s[U64_SHIFT + U64_SHIFT/4 + 1];
  int i, j;

  for (i = j = 0; i < 64; i += 4) {
    s[j++] = ((x >> (U64_SHIFT-(i+0+1))) & 1) ? '1' : '0';
    s[j++] = ((x >> (U64_SHIFT-(i+1+1))) & 1) ? '1' : '0';
    s[j++] = ((x >> (U64_SHIFT-(i+2+1))) & 1) ? '1' : '0';
    s[j++] = ((x >> (U64_SHIFT-(i+3+1))) & 1) ? '1' : '0';
    if (is_space) {
      s[j++] = ' ';
    }
  }
  if (is_space) {
    s[U64_SHIFT+U64_SHIFT/4] = '\0';
  } else {
    s[U64_SHIFT] = '\0';
  }

  return s;
}


/* ------------------------------------------------------------
 * char *ubasename(const char *string)
 * ------------------------------------------------------------ */
char *ubasename(const char *string) {
  char *base = strrchr(string, '/');
  if (!base) {
    return (char *)string;
  } else {
    return ++base;
  }
}


/* ------------------------------------------------------------
 * char *mstrcat(char *first, ...)
 * ------------------------------------------------------------ */
char *mstrcat(char *first, ...) {
  va_list args;
  va_start(args, first);
  while (1) {
    char *s = va_arg(args, char *);
    if (!s) {
      break;
    } else {
      strcat(first, s);
    }
  }
  va_end(args);
  return first;
}


/* ------------------------------------------------------------
 * char *strcatfmt(char *first, ...)
 * ------------------------------------------------------------ */
int strcatfmt(char *string, const char *format, ...) {
  assert( string );
  assert( format );
  int nr = 0;
  va_list args;
  va_start(args, format);
  nr = vsprintf(string + strlen(string), format, args);
  va_end(args);
  return nr;
}


/* ------------------------------------------------------------
 * char *extract_suffix(char *org, char *suffix)
 * ------------------------------------------------------------ */
char *extract_suffix(char *org, char *suffix) {
  char *string = NULL;
  if ( !org || !suffix || strlen(org) <= strlen(suffix) ) {
    return org;
  }

  assert( string = (char *)malloc(strlen(org)+1) );
  strcpy(string, org);

  char *s = strrchr(string, '.');
  if ( !strcmp( s, suffix ) ) {
    *s = '\0';
  }
  return string;
}


/* ------------------------------------------------------------
 * long long getenvi(char *env, long long def)
 * double getenvf(char *env, double def)
 * ------------------------------------------------------------ */
long long getenvi(char *env, long long def) {
  if (env && getenv(env)) {
    return atoll(getenv(env));
  } else {
    return def;
  }
}

double getenvf(char *env, double def) {
  if (env && getenv(env)) {
    return atof(getenv(env));
  } else {
    return def;
  }
}



/* ------------------------------------------------------------
 * void error(const char *format, ... )
 * ------------------------------------------------------------ */
void error(const char *format, ... ) {
  va_list args;
  va_start(args, format);
  cl_fprintf(UCL_RED, stderr, "ERROR: ", args);
  vfprintf(stderr, format, args);
  va_end(args);
  exit(EXIT_FAILURE);
}


/* -----------------------------------------------------------------
 * int cl_sprintf(int color, char *buffer, const char *format, ...)
 * int cl_fprintf(int color, FILE *stream, const char *format, ...)
 * ----------------------------------------------------------------- */
static const char *ansi_colors[] = {
  "\e[0m",	/* default */
  "\e[30m",	/* black */
  "\e[31m",	/* red */
  "\e[32m",	/* green */
  "\e[33m",	/* brown */
  "\e[34m",	/* blue */
  "\e[35m",	/* magenta */
  "\e[36m",	/* cyan */
  "\e[37m",	/* gray */
  "\e[40m",	/* bg black */
  "\e[41m",	/* bg red */
  "\e[42m",	/* bg green */
  "\e[43m",	/* bg brown */
  "\e[44m",	/* bg blue */
  "\e[45m",	/* bg magenta */
  "\e[46m",	/* bg cyan */
  "\e[47m",	/* bg gray */
  NULL
};

int cl_sprintf(int color, char *buffer, const char *format, ...) {
  int nr = 0;
  va_list args;
  char buf[LINE_MAX];
  if (0 <= color && color < UCL_COLORS && buffer && format) {
    va_start(args, format);
    vsprintf(buf, format, args);
    nr = sprintf(buffer, "%s%s%s", ansi_colors[color], buf, ansi_colors[UCL_DEFAULT]);
    va_end(args);
  }
  return nr;
}

int cl_fprintf(int color, FILE *stream, const char *format, ...) {
  int nr = 0;
  va_list args;
  char buffer[LINE_MAX];
  if (0 <= color && color < UCL_COLORS &&
      stream && format) {
    va_start(args, format);
    cl_sprintf(color, buffer, format, args);
    nr = fprintf(stream, "%s", buffer);
    va_end(args);
  }
  return nr;
}


/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */

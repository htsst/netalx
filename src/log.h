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

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include "defs.h"

static FILE *logout = NULL;

#if defined (__cplusplus)
extern "C" {
#endif

void __attribute__ ((constructor))  open_logfile(void) {
  if (!logout) logout = fopen(LOGFILE, "a");
}
void __attribute__  ((destructor)) close_logfile(void) {
  if ( logout) fclose(logout);
}

void logprintf(const char* format, ...) {
  va_list args;
  if (stdout) {
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
  }
  if (logout) {
    va_start(args, format);
    vfprintf(logout, format, args);
    va_end(args);
  }
}

#if defined (__cplusplus)
}
#endif

#endif /* LOG_H */

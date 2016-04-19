 # ------------------------------------------------------------------------
 # This is part of NETALX.
 #
 # Copyright (C) 2012-2015 The GraphCREST Project, Tokyo Institute of Technology
 #
 # NETALX is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation; either version 2 of the License, or
 # (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License along
 # with this program; if not, write to the Free Software Foundation, Inc.,
 # 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 # ------------------------------------------------------------------------

## ------------------------------------------------------------
BUILD_COMPILER = GCC

ifeq ($(BUILD_COMPILER), GCC)
CC     := gcc
CFLAGS := -O2 -std=gnu99 -I./
endif
ifeq ($(BUILD_COMPILER), ICC)
CC     := /opt/intel/composerxe/bin/icc
CFLAGS := -O2
# CFLAGS := -fast -static-intel -openmp
# CFLAGS := -xhost -openmp
endif
## ------------------------------------------------------------
RM		= rm -rf
AR      = ar -rcus
SOURCES = $(wildcard *.c)
OBJECTS = $(subst .c,.o, $(SOURCES))
LIBNAME = libparse_conf_file.a

.PHONY: all lib clean update_make_headers
all: lib

lib: $(LIBNAME)

${LIBNAME}: $(OBJECTS)
	$(RM) $@
	$(AR) $@ $^

update_make_headers:
	$(CXX) -MM -I$(STSLIBCDIR) *.c > make.headers

clean:
	$(RM) $(OBJECTS) $(LIBNAME)

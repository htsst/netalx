 # ------------------------------------------------------------------------ *
 # This is part of NETALX.
 #
 # originally written by: Yuichiro Yasui
 # this version modified by The GraphCREST Project
 #
 # Copyright (C) 2011-2015 Yuichiro Yasui
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

include make.rule

## targets
TARGET_SNGL_BM         := graph500
TARGET_SNGL_BM_EXMEM   := graph500_exmem
TARGET_SNGL_BM_RESTORE := graph500_restore

COMMON_OBJECTS         := main.o common.o statistics.o
BFS_SNGL_BM_OBJS         := $(COMMON_OBJECTS) generation.o construction.o para_bfs_csr_bitmap.o validation.o
BFS_SNGL_BM_EXMEM_OBJS   := $(COMMON_OBJECTS) generation_exmem.o construction_exmem.o para_bfs_csr_bitmap_f_cmpcttree.o validation_fe_cmpcttree.o dump.o external_full_construction_bucket.o
BFS_SNGL_BM_RESTORE_OBJS := $(COMMON_OBJECTS) generation_restore.o construction_restore.o para_bfs_csr_bitmap_f_cmpcttree.o validation_fe_cmpcttree.o dump.o

all: lib $(TARGET_SNGL_BM) $(TARGET_SNGL_BM_EXMEM) $(TARGET_SNGL_BM_RESTORE)
	@echo
	@echo finished successfully: \'$(TARGET_SNGL_BM)\'
	@echo finished successfully: \'$(TARGET_SNGL_BM_EXMEM)\'
	@echo finished successfully: \'$(TARGET_SNGL_BM_RESTORE)\'
	@echo

$(TARGET_SNGL_BM) : $(BFS_SNGL_BM_OBJS)
	$(LINK.o) $^ $(LDLIBS) $(OUTPUT_OPTION)
$(TARGET_SNGL_BM_EXMEM) : $(BFS_SNGL_BM_EXMEM_OBJS)
	$(LINK.cc) $^ $(LDLIBS) $(OUTPUT_OPTION)
$(TARGET_SNGL_BM_RESTORE) : $(BFS_SNGL_BM_RESTORE_OBJS)
	$(LINK.cc) $^ $(LDLIBS) $(OUTPUT_OPTION)

lib:
	@for i in $(LIBDIRS); do $(MAKE) -C $$i; done
distclean:
	@for i in $(LIBDIRS); do $(MAKE) -C $$i clean; done

update_make_headers:
	@$(CC) -MM $(addprefix -I, . $(LIBDIRS)) *.c > make.headers

include make.headers

clean: distclean
	rm -rf \#* *.o *~ *.dSYM gmon.out
	rm -rf  $(TARGET_SNGL_BM) $(TARGET_SNGL_BM_F) $(TARGET_SNGL_BM_EXMEM)

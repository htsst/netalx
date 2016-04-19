ULIBCDIR := .
TESTDIR  := test
-include make.rule

SOURCES  := $(wildcard *.c)
OBJECTS  := $(subst .c,.o, $(SOURCES))

.PHONY: all lib clean update_make_headers
all: lib

lib: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	$(RM) $@
	$(AR) $@ $^
ifdef RANLIB
	$(RANLIB) $@
endif

test: lib
	$(MAKE) -C $(TESTDIR)

update_make_headers:
	$(CC) -MM -I$(ULIBCDIR) *.c > make.headers

-include make.headers

clean:
	$(RM) *~ \#* *.o gmon.out $(LIBNAME)
	$(MAKE) -C $(TESTDIR) clean

### Local variables:
### mode: makefile-bsdmake
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:

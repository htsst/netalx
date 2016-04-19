ULIBCDIR := ..
-include $(ULIBCDIR)/make.rule

SOURCES  := $(wildcard *.c)
TARGET   := $(subst .c,, $(SOURCES))

.PHONY: all clean lib test update_make_headers
all: lib test

lib:
	make -C $(ULIBCDIR)

test: $(TARGET)

update_make_headers:
	$(CC) -MM -I$(ULIBCDIR) *.c > make.headers

-include make.headers

clean:
	$(RM) *~ \#* *.o gmon.out $(TARGET)

### Local variables:
### mode: makefile-bsdmake
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:

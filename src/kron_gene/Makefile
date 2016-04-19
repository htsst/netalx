include ../make.rule

# -*- Makefile -*-
INCLUDE  = .  ../ ../$(ULIBCDIR)
CPPFLAGS = $(addprefix -I, $(INCLUDE))
LDFLAGS  = $(CFLAGS) $(addprefix -L, $(INCLUDE))
AR       = ar -rcus

SOURCES = make_edgelist.c splittable_mrg.c graph_generator.c make_graph.c utils.c prng.c rmat.c
OBJECTS = $(subst .c,.o, $(SOURCES))
LIBNAME = lib_make_kron_edgelist.a

.PHONY: all
all: $(LIBNAME) ../$(ULIBCNAME)

../$(ULIBCNAME): 
	$(MAKE) -C ../$(ULIBCDIR)

$(LIBNAME): $(OBJECTS)
	$(RM) $@
	$(AR) $@ $^
ifdef RANLIB
	$(RANLIB) $@
endif

.PHONY:	clean
clean:
	rm -rf make_edgelist *~ *.dSYM *.o $(OBJECTS) $(LIBNAME)

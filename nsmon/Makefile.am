# $Id: Makefile.am,v 0.1 2001/07/08 22:38:31 vikas Exp $
#
# Makefile.am for 'nsmon'
#

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = nstest

bin_PROGRAMS = nsmon
nsmon_SOURCES = main.c nsmon.c

check_PROGRAMS = nstest
nstest_SOURCES = nsmon.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = nsmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

## for standalone.
nstest.o: nsmon.c
	$(COMPILE) -DTEST -c nsmon.c -o nstest.o

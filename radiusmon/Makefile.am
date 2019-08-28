# $Id: Makefile.am,v 0.1 2001/07/08 22:51:42 vikas Exp $
#
# Makefile.am for 'radiusmon'
#

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = radtest

bin_PROGRAMS = radiusmon
radiusmon_SOURCES = main.c radiusmon.c md5.c

check_PROGRAMS = radtest
radtest_SOURCES = radiusmon.c md5.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = radiusmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

## for standalone.
radtest.o: radiusmon.c
	$(COMPILE) -DTEST -c radiusmon.c -o radtest.o

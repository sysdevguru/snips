# $Id: Makefile.am,v 0.1 2001/07/09 03:55:28 vikas Exp $
#
# Makefile.am for 'tpmon'
#

## how to indicate init file (tpmon)

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = nstest

bin_PROGRAMS = tpmon
tpmon_SOURCES = main.c tpmon.c

check_PROGRAMS = tptest
tptest_SOURCES = tpmon.c tptest.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = tpmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi



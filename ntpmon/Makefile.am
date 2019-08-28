# $Id: Makefile.am,v 0.1 2001/07/08 22:43:52 vikas Exp $
#
# Makefile.am for 'ntpmon'
#

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

bin_PROGRAMS = ntpmon
ntpmon_SOURCES = main.c ntpmon.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = ntpmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

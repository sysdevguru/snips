# $Id: Makefile.am,v 0.1 2001/07/08 22:48:02 vikas Exp $
#
# Makefile.am for 'portmon'
#

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

bin_PROGRAMS = portmon
portmon_SOURCES = main.c portmon.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = portmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

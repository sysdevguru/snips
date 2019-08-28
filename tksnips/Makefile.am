# $Id: Makefile.am,v 0.1 2001/07/09 03:47:49 vikas Exp $
#
# Makefile for 'ndaemon'
#

PROGCDEFS =	-DDATADIR=\"$(DATADIR)\"

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

bin_PROGRAMS = ndaemon tkSnips
ndaemon_SOURCES = ndaemon.c
tkSnips_SOURCES = tkSnips.tix

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = ndaemon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

tkSnips:  tkSnips.tix
	@echo "Doing substitutions on tkSnips" ;\
	 rm -f tkSnips ; \
	 cat tkSnips.tix > tkSnips ;

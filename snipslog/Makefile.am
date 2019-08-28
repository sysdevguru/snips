# $Id: Makefile.am,v 0.1 2001/07/09 03:08:43 vikas Exp $
#
# Makefile.am for 'nsmon'
#

PROGCDEFS =	-DETCDIR=\"$(ETCDIR)\" -DPIDDIR=\"$(PIDDIR)\"

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = snipslogtest

bin_PROGRAMS = snipslogd
snipslogd_SOURCES = snipslogd.c

check_PROGRAMS = snipslogtest
snipslogtest_SOURCES = snipslogtest.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = snipslogd.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

## for standalone.
snipslogtest.o:  snipslogtest.c
	$(COMPILE) -DTEST -c snipslogtest.c

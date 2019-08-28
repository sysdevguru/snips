# $Id: Makefile.am,v 0.1 2001/07/09 04:04:24 vikas Exp $
#
# Makefile.am for 'trapmon'
#

## global variables
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/cmu-snmp/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = nstest

bin_PROGRAMS = trapmon
trapmon_SOURCES = trapmon.c  snips_specific.c
trapmon_LDADD =   -L$(top_srcdir)/cmu-snmp/lib -lsnmp

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = trapmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

install-root: trapmon
	@if [ `whoami` = "root" ]; then \
		chmod 4750 $(bindir)/trapmon ;\
		chown root $(bindir)/trapmon ;\
		echo "set $(bindir)/trapmon to suid root" ;\
	else \
		echo "You need to be root to install trapmon suid" ;\
		exit 1; \
	fi


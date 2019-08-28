# $Id: Makefile.am,v 0.1 2001/07/08 22:55:17 vikas Exp $
#
# Makefile.am for 'pingmon'
#

SUBDIRS = multiping rpcping

PROGCDEFS =	-DIPPING=\"$(IPPING)\"  -DRPCPING=\"$(RPCPING)\"

## SET_THIS
#  You can change the IPPING to the system based '/bin/ping' if needed.
#  These can also be set in the config files.
IPPING =	$(bindir)/multiping
RPCPING =	$(bindir)/rpcping
#IPV6PING =	"ipv6ping"
#OSIPING	    =	"/usr/sunlink/bin/osiping"

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

bin_PROGRAMS = pingmon
pingmon_SOURCES = main.c pingmon.c

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = ippingmon.init rpcpingmon.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@for f in $(INITFILE); do \
	  if test -f $(INITDIR)/$$f ; then : ; else \
		$(LN_S) $(INITFILE_SRC) $(INITDIR)/$$f; \
	  fi ;\
	done

install-exec-local:
	#$(INSTALL) -c -m 751 $(TARG) $(BINDIR)/ ;
	@[ -f $(bindir)/ippingmon ] && rm -f $(bindir)/ippingmon ; \
	 ln -s $(bindir)/pingmon $(bindir)/ippingmon ;\
	 [ -f $(bindir)/rpcpingmon ] && rm -f $(bindir)/rpcpingmon ; \
	 ln -s $(bindir)/pingmon $(bindir)/rpcpingmon

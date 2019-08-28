# $Id: Makefile.am,v 0.2 2001/09/24 15:30:49 vikas Exp $
#

# Need the locations of ROOTDIR, PERL & PING
PERL =	@PERL@
PING = @PING@

# directory for CMU snmp sources
CMUSNMP =	$(top_srcdir)/cmu-snmp
##
#
PROGS = apcmon armon bgpmon bpmon ciscomon modemmon novellmon nrmon \
	hostmon hostmon-collector hostmon-osclients/hostmon-client \
	smbmon sqlmon snmpmon snmpmon-collector snmpgeneric \
	syslogmon upsmon rcisco testlog

CONF = snipsperl.conf

# to create init files to start/stop programs
INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = apcmon armon bgpmon bpmon ciscomon modemmon novellmon nrmon \
		hostmon smbmon sqlmon snmpmon snmpgeneric syslogmon upsmon

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@for f in $(INITFILE); do \
	  if test -f $(INITDIR)/$$f.init ; then : ; else \
		$(LN_S) $(INITFILE_SRC) $(INITDIR)/$$f.init ; \
	  fi ;\
	done

all-local:	snmpprogs
	@echo "SNIPS PERL monitors"
	@echo " Doing substitutions for PERL, ROOTDIR, PING on: "
	@for f in $(PROGS) $(CONF) ; do \
	  if [ -f $$f ]; then \
	   echo "  $$f" | tr -d '\012' ;\
	   sed -e '1s|^#!/.*/perl|#!$(PERL)|' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(DESTDIR)" \3#' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)#\1 q\($(DESTDIR)\) \3#' \
		-e 's#^\($$ping\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(PING)" \3#' \
		-e 's#^\($$ping\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)#\1 q\($(PING)\) \3#' \
		$$f >$$f.seds ;\
	 fi ;\
	done ;\
	echo ""

snmpprogs:
	@if [ -d $(CMUSNMP)/snmpapps ]; then \
	  echo 'Making snmpwalk under $(CMUSNMP)/snmpapps' ;\
	  cd $(CMUSNMP)/snmpapps ;\
	  if [ ! -f Makefile ]; then \
	   ./configure --quiet --prefix=$(CMUSNMP) \
		--disable-shared --with-snmp=$(CMUSNMP) ;\
	  fi ; \
	  make snmpwalk ; \
	  make snmpget ; \
	 else \
	  echo "$(CMUSNMP)/snmpapps  does not exist" ;\
	  exit 1 ;\
	 fi

# ideally hostmon-osclients/ need not go into the BINDIR, but we are
# putting it all there for now.
install:
	-@[ -d $(BINDIR)/hostmon-osclients ] || mkdir $(BINDIR)/hostmon-osclients
	-@echo "Installing files in $(BINDIR)/hostmon-osclients" ;\
	  for f in hostmon-osclients/* ; do \
	   if [ -f $$f ]; then \
	     $(INSTALL) -c -m 755 $$f $(BINDIR)/hostmon-osclients/ ;\
	   fi \
	  done
	-@echo "Installing perl monitors under $(BINDIR)" ;\
	  for f in $(PROGS) ; do \
	    $(INSTALL) -c -m 755 $$f.seds $(BINDIR)/$$f ;\
	  done
	-@for f in $(CONF) ; do \
	   if [ ! -f $(ETCDIR)/$$f ]; then \
		$(INSTALL) -c -m 644 $$f.seds $(ETCDIR)/$$f ;\
	   fi \
	 done
	$(INSTALL) -c -m 755 $(CMUSNMP)/snmpapps/snmpwalk $(BINDIR)/
	$(INSTALL) -c -m 755 $(CMUSNMP)/snmpapps/snmpget  $(BINDIR)/
	$(INSTALL) -c -m 444 $(CMUSNMP)/mib-v2.txt $(ETCDIR)/
	@if [ -f $(ETCDIR)/mibII.txt ]; then \
		mv $(ETCDIR)/mibII.txt $(ETCDIR)/mibII.txt.old ;\
	 fi 
	@echo  "See $(SRCDIR)/perl-monotors/README for further customizations"

CLEANFILES = *.pid *.seds hostmon-osclients/*.seds

clean-local:
	@-cd $(CMUSNMP)/snmpapps; make realclean


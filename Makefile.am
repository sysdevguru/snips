# Which programs are to be compiled and installed.
#	etherload	ethernet load and pps monitor
#	snipslog	the snipslogd logging daemon
#	nsmon		Nameserver monitor
#	perl-monitors	Perl interface and associated monitors
#	pingmon		Reachability monitors (ICMP, RPC, OSI)
#	portmon		Check TCP ports on various machines.
#	radiusmon	Check udp radius monitor
#	snipstv	the snips display for all the monitored data
#	tksnips		Tcl/Tk interface to snips events (needs ndaemon)
#	tpmon		Data thruput monitor
#	trapmon		SNMP trap monitor
#	utility		Small utility programs
#	man		Manual pages
#
SUBDIRS =  lib utility etherload snipslog nsmon ntpmon \
	pingmon portmon radiusmon perl-monitors perl-module \
	snipstv snipsweb tksnips tpmon trapmon \
	man perl-module

##
# This is the list of files that need to be 'tar-red'. Using the FF option
# in the tar command and also excluding the RCS directories further down.
EXTRA_DIST =	BUGS COPYRIGHT INSTALL README README.RRD README.NOCOL \
	Makefile.am autoconf cmu-snmp include contrib docs \
	snips.conf configure.in

##
# Following files are excluded from the distribution
NOTDIST = makefile etherload/docs local-stuff \
	  cmu-snmp/bin cmu-snmp/lib cmu-snmp/man cmu-snmp/etc \
	  CHECK DONE TODO

CLEANFILES = config.cache

# use relaxed rules when building dists
AUTOMAKE_OPTIONS= foreign no-dependencies

# where we keep local rules for automake
ACLOCAL_M4=     $(top_srcdir)/autoconf/aclocal.m4
AUTOHEADER = @AUTOHEADER@ --localdir=$(top_srcdir)/autoconf
AUTOCONF = @AUTOCONF@ --localdir=$(top_srcdir)/autoconf

#	PIDDIR		where programs create their pid files and error files
#	DATADIR		where datafiles for monitoring programs are created
#	INITDIR		dir for startup init scripts (SysV style init.d)
#	MSGSDIR		for message files that the snips display shows in
#			in the 'messages' sub-window
#	EXAMPLESDIR	directory to store example config files
#	DEVICEHELPDIR	dir for storing helpfiles for snipsweb
#	RRD_DBDIR	directory to store RRD data files
#	CGIDIR		dir to install the snipsweb CGI scripts
#	HTMLDIR		dir to install the snipsweb html files.
#
PIDDIR=		$(prefix)/run
INITDIR=	$(prefix)/init.d
MSGSDIR=	$(prefix)/msgs
RRD_DBDIR=	$(prefix)/rrddata
EXAMPLESDIR=	$(prefix)/etc//samples
DEVICEHELPDIR=	$(prefix)/device-help
CGIDIR=		$(prefix)/web/cgi
HTMLDIR=	$(prefix)/web/html

## RRD specific defines (http://www.caida.org)
#RRDCFLAGS = "-DRRDTOOL"
RRDCFLAGS = @RRDCFLAGS@
#RRDLIBDIR = "/usr/local/rrd/lib"
RRDLIBDIR = @RRDLIBDIR@
# RRDLIBS must be AFTER -lsnips
#RRDLIBS = -L/usr/local/rrd/lib -lrrd
RRDLIBS = @RRDLIBS@


all:	Init
	 @echo "BUILDING SNIPS revision $(REV) for $(OS) $$OS" ;\
	 for i in $(SRCS); do \
	  if [ -d $$i ]; then \
	    (echo "" ; echo "MAKING IN $$i" ; echo "----------" ; \
	    cd $$i ; make $(MFLAGS) OS="$$OS" ;) \
	  fi ;\
	 done
	@echo "" ; echo "Now run make install"

Init:
	@if [ "$(OS)" = "" ]; then \
	    OS=`uname -s -r`; \
	  else \
	   if [ "$(OS)" != "`uname -s -r`" ]; then \
	    echo "WARNING- MAKEFILES CONFIGURED FOR $(OS), NOT `uname -s -r`";\
	   fi; \
	 fi

## Ultrix make balked at the tests for directories, hence the second test
install:	installdir  lib
	@[ -d $(ROOTDIR) ] && [ -d $(DATADIR) ] && [ -d $(ETCDIR) ] \
		&& [ -d $(BINDIR) ]
	@-(touch SniPpet ; $(INSTALL) -c -m 750 SniPpet /tmp/;) >/dev/null 2>&1 ;
	@(if [ -f /tmp/SniPpet ]; then rm -f SniPpet /tmp/SniPpet; else \
	  echo "YOU HAVE AN INCOMPATIBLE install, use 'myinstall' instead" ;\
	  echo "Else prefix your shell PATH with BSD style install" ;\
	  rm -f  SniPpet /tmp/SniPpet ; exit 1 ; fi)

	@echo make $(MFLAGS) install ;
	@-for i in $(SRCS); do \
	  ( echo "" ; echo "MAKING IN $$i" ; echo "----------" ; cd $$i ; \
		make $(MFLAGS) install ; make $(MFLAGS) init ) ;\
	done
	@echo ""
	@echo "Installing sample config files under $(EXAMPLESDIR)/..."
	@-for i in */*-confg */*.conf *.conf ; do \
		$(INSTALL) -m 444 -c  $$i $(EXAMPLESDIR)/ ;\
	done
	@if [ ! -f $(ETCDIR)/snips.conf ]; then \
		$(INSTALL) -c -m 755 snips.conf $(ETCDIR)/snips.conf ;\
	 fi
	@echo "" ; echo "           NOW su AND DO A make root "


## install some of the programs as root.
root:
	@-for d in etherload pingmon trapmon; do \
		(cd $d ; make install-root) ; \
	done

Xsnips.tar:
	@-rm -f snips-$(REV).tar.gz
	@TEMPD=/var/tmp/t/snips-$(REV) ;\
	 [ -d $$TEMPD ] || mkdir /var/tmp/t $$TEMPD ;\
	 if [ ! -d $$TEMPD ]; then \
	    echo "Fatal: cannot create directory  $$TEMPD";\
		exit 1 ; \
	 else \
	   rm -f $$TEMPD/XCLUDEFILES;\
	 fi ;\
	 echo "Creating list of exclude files and directories" ;\
	 for i in $(DIST) ; do \
	  find $$i \( -name RCS -o -name '*~' \) -print >>$$TEMPD/XCLUDEFILES;\
	 done ;\
	 for i in $(NOTDIST) ; do \
		echo "$$i" >> $$TEMPD/XCLUDEFILES ;\
	 done ;\
	 echo "Copying src files to tmp location" ;\
	 tar cfX - $$TEMPD/XCLUDEFILES $(DIST) | (cd $$TEMPD; tar xf -) ;\
	 rm -f $$TEMPD/XCLUDEFILES ;\
	 echo "Creating distribution tar file" ;\
	 ( cd $$TEMPD/ ; \
	   cp Makefile.dist Makefile ; rm -f config.cache ;\
	   cd .. ; tar cf - snips-$(REV) ) >snips-$(REV).tar;\
	 ls -l snips-$(REV).tar
	@rm -rf /var/tmp/t

###

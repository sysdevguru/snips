# $Id: Makefile.am,v 1.0 2001/07/09 04:12:47 vikas Exp $

## To compile all the 'misc' utility programs for snips
#  Need TOP and OPSMAIL for doing substitutions...
#

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = *.seds

bin_PROGRAMS = display_snips_datafile  eventselect
display_snips_datafile_SOURCES = display_snips_datafile.c
eventselect_SOURCES = eventselect.c

SHSCRIPTS = crontab.snips snipsprog.init
PERLSCRIPTS = keepalive_monitors.pl logstats.pl logmaint.pl \
		docrypt.pl notifier.pl


# need OPSMAIL defined here
#OPSMAIL = @OPSMAIL@
#ADMINMAIL = @ADMINMAIL@
MAIL=@MAIL@
PSFLAGS=@PSFLAGS@

all-local:
	@echo "Doing substitutions for ROOTDIR, PIDDIR, OPSMAIL, MAIL, PSFLAGS on: " ;\
	 for f in $(SHSCRIPTS) ; do \
	   echo " $$f" | tr -d '\012' ;\
	   sed  \
		-e "s#@ROOTDIR@#$(ROOTDIR)#g" \
		-e "s#@PIDDIR@#$(PIDDIR)#g" \
		-e "s#@OPSMAIL@#$(OPSMAIL)#g" \
		-e "s#@ADMINMAIL@#$(ADMINMAIL)#g" \
		-e "s#@HOST@#`hostname`#g" \
		-e "s#@MAIL@#$(MAIL)#g"  \
		-e "s#@PSFLAGS@#$(PSFLAGS)#g" \
		$$f > $$f.seds ;\
	done ;\
	for f in $(PERLSCRIPTS) ; do \
	   echo " $$f" | tr -d '\012' ;\
	   sed  -e '1s|^#!/.*/perl|#!$(PERL)|' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(ROOTDIR)" \3#' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)#\1 q\($(ROOTDIR)\) \3#' \
		-e 's|^\($$email_program\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)|\1 "$(MAIL)" \3|' \
		-e 's|^\($$email_program\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)|\1 q\($(MAIL)\) \3|' \
		-e 's|^\($$OPSMAIL\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)|\1 q\($(OPSMAIL)\) \3|' \
		-e 's|^\($$ADMINMAIL\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)|\1 q\($(ADMINMAIL)\) \3|' \
		$$f >$$f.seds ;\
	done ;\
	echo ""

install-exec-local: all-local
	-@ for f in $(SHSCRIPTS) $(PERLSCRIPTS) ; do \
	   if [ -f $(bindir)/$$f ]; then \
	   	echo "Not overwriting existing $(bindir)/$$f" ;\
	   else \
	   	$(INSTALL) -c -m 751 $$f.seds $(bindir)/$$f ;\
	   fi ;\
	done


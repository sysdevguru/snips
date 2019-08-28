# $Header: /home/cvsroot/snips/lib/Makefile.am,v 0.1 2001/07/08 22:20:03 vikas Exp $
#
# Makefile.mid for snips 'lib'. This file also builds the CMU-SNMP lib.
#
## include Makefile.head

## global variables
INCLUDES = -I$(top_srcdir)/include

# directory for CMU snmp sources. The SNMP library will be built here
CMUSNMPDIR    = 	$(top_srcdir)/cmu-snmp
CMUSNMPLIB =	$(CMUSNMPDIR)/lib/libsnmp.a
PERLMODULE_MAKEFILE = 	$(top_srcdir)/perl-module/Makefile

## Any program specific defines or libraries. Dir names should not have
#  trailing '/'
PROGCDEFS =	-DETCDIR=\"$(prefix)/etc\"  -DDATADIR=\"$(prefix)/data\" \
		-DPIDDIR=\"$(prefix)/run\"  -DRRD_DBDIR=\"$(prefix)/rrddata\"

ODBCINCLUDES =	-I/usr/local/iodbc/include
ODBCLIBS = -L/usr/local/iodbc/lib -liodbc

lib_LIBRARIES =	libsnips.a
libsnips_a_SOURCES =	bsdsignal.c  daemon.c  eventlog.c  event_utils.c \
			getdate.y  putenv.c netmisc.c  rrd_funcs.c  \
			snips_main.c snips_funcs.c  strfuncs.c

check_PROGRAMS = getdate
getdate_SOURCES = getdate.y

CLEANFILES = getdate odbctest odbc_api.o

## additional things that need to be built by this Makefile
all-local:	$(lib_LIBRARIES) $(CMUSNMPLIB) $(PERLMODULE_MAKEFILE)

$(CMUSNMPLIB):	
	@[ -d $(CMUSNMPDIR) ] || (echo "MISSING SNMP DIR $(CMUSNMPDIR)" && exit 1)
	@echo 'Making the new CMU SNMP library under $(CMUSNMPDIR)'
	@cd $(CMUSNMPDIR)/snmp ; \
	 if [ ! -f Makefile ]; then \
		./configure --quiet --prefix="`pwd`/.." --disable-shared ; \
	 fi ; \
	 make MIBDIR=$(prefix)/etc all ; \
	 if [ ! -f $(CMUSNMPDIR)/lib/libsnmp.a ]; then \
		make install_inc ; make install_static ; \
	 fi

## This Makefile needs to be generated after libsnips.a is made
$(PERLMODULE_MAKEFILE):
	 @if [ ! -f $(PERLMODULE_MAKEFILE) ]; then \
	   echo ""; \
	   echo "Generating  $(PERLMODULE_MAKEFILE)"; \
	   (cd $(top_srcdir)/perl-module; \
	    perl Makefile.PL \
		CC="$(CC)" OPTIMIZE="$(CFLAGS)" LIBS="$(LIBS)" \
		INSTALLSITELIB=$(libdir)/perl	\
		INSTALLSITEARCH=$(libdir)/perl	\
		INSTALLARCHLIB=$(libdir)/perl	\
		INSTALLMAN3DIR=$(mandir) ;	\
	   ) >/dev/null ;\
	 fi

## For testing, to make a standalone version using -DTEST
getdate:  getdate.y
	@/bin/rm -f y.tab.c
	$(YACC) getdate.y
	$(COMPILE) -DTEST y.tab.c -o $@
	@/bin/rm -f y.tab.c

## Putting this rule here just to be able to print out a message about
#  the 13 shift/reduce conflicts from yacc.
getdate.o: getdate.y 
	@/bin/rm -f y.tab.c
	@echo "Expect 13 shift/reduce conflicts from $(YACC) : "
	$(YACC) $<
	$(CC) $(CFLAGS) -c y.tab.c -o $@
	@/bin/rm -f y.tab.c

odbc_api: odbc_api.c
	$(CC) $(CFLAGS) -DTEST $(ODBCINCLUDES) $(ODBCLIBS) -o odbc_api $<

## dont run clean under perl-module. That is done separately.
clean-local:
	@-cd $(CMUSNMPDIR)/snmp ; make clean; make realclean
	@-cd $(CMUSNMPDIR)/snmpapps ; make clean; make realclean
	@-cd $(CMUSNMPDIR); \
	 rm -rf $(CMUSNMPDIR)/lib $(CMUSNMPDIR)/include \
		$(CMUSNMPDIR)/man $(CMUSNMPDIR)/etc
##

# $Id: Makefile.am,v 0.2 2001/09/24 15:24:57 vikas Exp $
#

# Need the locations of ROOTDIR, PERL & PING
ROOTDIR = $(DESTDIR)
PERL =	@PERL@

##
#
CGI =   	snipsweb.cgi rrdgraph.cgi genweb.cgi genweb-filter.cgi
CONFIG =	snipsweb-confg updates webusers
HTMLFILES =	chirp.au critical.wav error.wav warning.wav

all:
	@echo "SNIPS Web Interface"
	@echo " Doing substitutions for PERL, ROOTDIR on: "
	@for f in $(CGI) $(CONFIG); do \
	  if [ -f $$f ]; then \
	   echo "  $$f" | tr -d '\012' ;\
	   sed -e '1s|^#!/.*/perl|#!$(PERL)|' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(ROOTDIR)" \3#' \
		-e 's#^\(\ *$$snipsroot\ *=\ *\)q(\(.*\))\(.*SET_THIS.*\)#\1 q\($(ROOTDIR)\) \3#' \
		-e 's#^\(\ *$$RRD_DBDIR\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(RRD_DBDIR)" \3#' \
		-e 's#^\(\ *$$RRDLIBDIR\ *=\ *\)\"\(.*\)\"\(.*SET_THIS.*\)#\1 "$(RRDLIBDIR)" \3#' \
		$$f >$$f.seds ;\
	 fi ;\
	done ;\
	echo ""

install:	installdir
	@for f in $(CGI) ; do \
		$(INSTALL) -c -m 755 $$f.seds $(CGIDIR)/$$f ; \
	done
	@for f in $(CONFIG) ; do \
	 if [ -f $(ETCDIR)/$$f ]; then \
		echo "Not overwriting existing $(ETCDIR)/$$f" ;\
	 else \
		$(INSTALL) -c -m 755 $$f.seds $(ETCDIR)/$$f ; \
	 fi ;\
	done
	@[ -f $(DEVICEHELPDIR)/default ] || $(INSTALL) -c -m 644 default $(DEVICEHELPDIR)/
	@for f in $(HTMLFILES) ; do \
	 if [ -f $(HTMLDIR)/$$f ]; then \
		echo "Not overwriting existing $(HTMLDIR)/$$f" ;\
	 else \
		$(INSTALL) -c -m 644 $$f $(HTMLDIR)/$$f ; \
	 fi ;\
	done ;\
	cp -r gifs $(HTMLDIR)/gifs
	@echo  "Installed basic web files under " ; \
	 echo  "   $(HTMLDIR) and $(CGIDIR)" ; \
	 echo  "Move into desired web location."; \
	 echo  "Edit the following files under $(ETCDIR)" ; \
	 echo  "   $(CONFIG)" ; \
	 echo  "Also      chown httpd webcookies updates"

clean:
	rm -f $(DIRT) *.pid *.seds


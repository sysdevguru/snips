# $Header: /home/cvsroot/snips/snipstv/Makefile.am,v 0.2 2001/07/17 03:26:00 vikas Exp $
#
# Makefile for SNIPS TextView (snipstv)
#
# 
# The HELPFILE is placed in the ETCDIR

CURSESLIB =	@CURSESLIB@

# Program specific compilation flags and libraries
PROGCDEFS =	-DDATADIR=\"$(DATADIR)\" -DMSGSDIR=\"$(MSGSDIR)\"

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

bin_PROGRAMS =	snipstv
snipstv_SOURCES = 	create_subwins.c  do_filter.c  help_page.c \
	init.c  main.c parse_user_input.c  update_eventwin.c  \
	update_msgtitle.c  update_msgwin.c  update_promptwin.c \
	update_title.c
snipstv_LDADD = @CURSESLIB@

install-data-local:
	[ -r $(bindir)/netconsole ] || ln -s $(bindir)/snipstv  $(bindir)/netconsole
	$(INSTALL) -m 0444 snipstv-help $ $(DESTDIR)$(prefix)/etc

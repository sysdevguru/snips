# $Header: /home/cvsroot/snips/man/Makefile.am,v 0.1 2001/07/08 22:30:16 vikas Exp $
#
# Make for installing the man pages
# This installs all the man pages with the same extension

SUFFIXES = .1 .3 .8 .man .sed .pod

CLEANFILES = *.1sed  *.3sed  *.8sed

MANPAGES1 = snipstv.1
MANPAGES3 = snips-perl.3
MANPAGES8 = 	etherload.8 multiping.8 nsmon.8 ntpmon.8 \
		pingmon.8 portmon.8 radiusmon.8 \
		snips-utility.8 snips.8 snipslogd.8 \
		tpmon.8 trapmon.8

TAIL =	prologue.man

## installation rules
imandir = $(prefix)/man
iman_DATA = $(MAN)
MAN = $(MANPAGES1:.1=.1sed) $(MANPAGES3:.3=.3sed) $(MANPAGES8:.8=.8sed)

EXTRA_DIST = $(MANPAGES1)  $(MANPAGES3)  $(MANPAGES8)


all-local:	man

man:	$(MAN)

.1.1sed .3.3sed .8.8sed:
	  cat $< $(TAIL) | sed \
		-e "s#@ROOTDIR@#$(ROOTDIR)#g" \
		-e "s#@PIDDIR@#$(PIDDIR)#g" \
		-e "s#@ETCDIR@#$(PIDDIR)#g" \
		-e "s#@OPSMAIL@#$(OPSMAIL)#g" \
		-e "s#@ADMINMAIL@#$(ADMINMAIL)#g" \
		> $@


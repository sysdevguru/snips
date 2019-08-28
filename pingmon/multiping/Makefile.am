# $Id: Makefile.am,v 0.1 2001/07/08 23:05:56 vikas Exp $
#
# Makefile for multiping
#
# 

bin_PROGRAMS = multiping
multiping_SOURCES = multiping.c multiping6.c

install-root: multiping
	@if [ `whoami` = "root" ]; then \
		chmod 4750 $(bindir)/multiping ;\
		chown root $(bindir)/multiping ;\
		echo "set $(bindir)/multiping to suid root" ;\
	else \
		echo "You need to be root to install multiping suid" ;\
		exit 1; \
	fi

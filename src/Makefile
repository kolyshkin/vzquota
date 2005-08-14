# Copyright (C) 2000-2005 SWsoft. All rights reserved.
#
# This file may be distributed under the terms of the Q Public License
# as defined by Trolltech AS of Norway and appearing in the file
# LICENSE.QPL included in the packaging of this file.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
INSTALL = install
PREFIX = /usr
SBINDIR = ${PREFIX}/sbin
VARDIR = /var
MANDIR = ${PREFIX}/share/man

#DEBUG = -D_DEBUG -g3 -ggdb3 -p -pg
CFLAGS += -pipe -I../include -g -Wall -Werror -Wformat -D_FILE_OFFSET_BITS=64 \
	-D_LARGEFILE64_SOURCE -DL2 $(DEBUG)


OBJS_main = common.o quota_io.o syscall.o \
	main.o stat.o quotaon.o \
	quotacheck.o
OBJS_check = common.o quota_io.o syscall.o \
	quotacheck.o vzdqcheck.o 
OBJS_dump = common.o quota_io.o syscall.o \
	vzdqdump.o
OBJS_load = common.o quota_io.o syscall.o \
	vzdqload.o

OBJS=$(OBJS_main) $(OBJS_check) $(OBJS_dump) $(OBJS_load)
PROGS=vzquota vzdqcheck vzdqdump vzdqload
MANS=vzquota.8 vzdqcheck.8 vzdqdump.8

all: $(PROGS)

depend: $(OBJS:.o=.c)
	$(CC) $(CFLAGS) -MM $(OBJS:.o=.c) > .depend

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

install: $(PROGS)
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man8
	$(INSTALL) -d $(DESTDIR)$(VARDIR)/vzquota
	for file in $(PROGS); do \
		$(INSTALL) -s -m 755 $$file $(DESTDIR)$(SBINDIR)/$$file; \
	done
	for file in $(MANS); do \
		$(INSTALL) -m 644 ../man/$$file $(DESTDIR)$(MANDIR)/man8/$$file; \
	done
	cd $(DESTDIR)$(MANDIR)/man8; ext=`ls vzdqdump.8*`; ext=$${ext#*.}; \
	ln -fs vzdqdump.$$ext vzdqload.$$ext; chmod 644 vzdqload.$$ext; cd -

clean:
	rm -f $(OBJS) $(PROGS) .depend core *.log
	find . -regex '.*[~#].*' | xargs rm -f

vzquota: $(OBJS_main)
	$(CC) $(CFLAGS) -o $@ $(OBJS_main)

vzdqcheck: $(OBJS_check)
	$(CC) $(CFLAGS) -o $@ $(OBJS_check)

vzdqdump: $(OBJS_dump)
	$(CC) $(CFLAGS) -o $@ $(OBJS_dump)

vzdqload: $(OBJS_load)
	$(CC) $(CFLAGS) -o $@ $(OBJS_load)

-include .depend

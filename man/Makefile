MAN8 = vzdqdump.8 vzdqcheck.8 vzquota.8
MANS = $(MAN8)

MANDIR=/usr/share/man
MAN8DIR=$(MANDIR)/man8

all depend:
.PHONY: all depend

%.8: %.8.in macros.tmac
	cat macros.tmac $< > $@

install: $(MANS)
	$(INSTALL) -d $(DESTDIR)$(MAN8DIR)
	install -m 644 $(MAN8) $(DESTDIR)$(MAN8DIR)/
	cd $(DESTDIR)$(MANDIR)/man8; ext=`ls vzdqdump.8*`; ext=$${ext#*.}; \
	ln -fs vzdqdump.$$ext vzdqload.$$ext; chmod 644 vzdqload.$$ext; cd -
.PHONY: install

clean:
	rm -f  $(MANS)
.PHONY: clean

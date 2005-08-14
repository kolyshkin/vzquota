SRCDIR=src
NAME=vzquota
SPEC=$(NAME).spec
VERSION=$(shell awk '/^Version:/{print $$2}' $(SPEC))
RELEASE=$(shell awk '/^Release:/{print $$2}' $(SPEC))
NAMEVER=$(NAME)-$(VERSION)-$(RELEASE)

TARBALL=$(NAMEVER).tar.bz2

all install depend clean:
	cd $(SRCDIR) && $(MAKE) -f Makefile $@

clean-all: clean
	rm -f $(TARBALL)

tar: $(TARBALL)

$(TARBALL): clean-all
	rm -f ../$(NAMEVER)
	ln -s $(NAME) ../$(NAMEVER)
	tar --directory .. --exclude CVS --exclude doc \
  	   --exclude $(TARBALL) -cvhjf $(TARBALL) $(NAMEVER)
	rm -f ../$(NAMEVER)

-include .depend

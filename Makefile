#  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

SUBDIRS=src man
NAME=vzquota
SPEC=$(NAME).spec
VERSION=$(shell awk '/^Version:/{print $$2}' $(SPEC))
NAMEVER=$(NAME)-$(VERSION)

TARBALL=$(NAMEVER).tar.bz2

all install depend clean:
	@set -e; \
	for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
.PHONY: all install depend clean

clean-all: clean
	rm -f $(TARBALL)

dist: tar
tar: $(TARBALL)
.PHONY: dist tar

$(TARBALL): clean-all
	rm -f ../$(NAMEVER)
	ln -s `pwd | awk -F / '{print $$NF}'` ../$(NAMEVER)
	tar --directory .. --exclude .git --exclude .depend \
		--exclude-from .gitignore \
		-cvhjf ../$(TARBALL) $(NAMEVER)
	rm -f $(TARBALL)
	mv ../$(TARBALL) .
	rm -f ../$(NAMEVER)

rpms: tar
	rpmbuild -ta $(TARBALL) ${RPMB_ARGS}
.PHONY: rpms

-include .depend


DESTDIR =
PREFIX	:= /usr
MANDIR	:= $(PREFIX)/man

.PHONY			: shell-fm manual all clean tags cscope

all				: shell-fm manual

shell-fm		:
	$(MAKE) -C source

manual			:
	$(MAKE) -C manual

install			:
	install -m 0755 -d $(DESTDIR)$(PREFIX)/bin/
	install -m 0755 -d $(DESTDIR)$(MANDIR)/man1/
	install -m 0755 source/shell-fm $(DESTDIR)$(PREFIX)/bin/
	install -m 0755 manual/shell-fm.1.gz $(DESTDIR)$(MANDIR)/man1/

install-strip	: install
	strip $(PREFIX)/bin/shell-fm

uninstall		:
	rm -f $(PREFIX)/bin/shell-fm
	rm -f $(MANDIR)/man1/shell-fm.1.gz
	rmdir --ignore-fail-on-non-empty $(PREFIX)/bin
	rmdir --ignore-fail-on-non-empty $(MANDIR)/man1
	rmdir --ignore-fail-on-non-empty $(MANDIR)
	rmdir --ignore-fail-on-non-empty $(PREFIX)


clean			:
	$(MAKE) -C source clean
	$(MAKE) -C manual clean


tags		: cscope.files
	@rm -f tags
	xargs -n50 ctags -a < cscope.files

cscope		: cscope.files
	cscope -b

cscope.files	:
	find source -name '*.[ch]' > cscope.files


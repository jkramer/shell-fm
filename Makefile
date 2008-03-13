
DESTDIR	:= /usr
MANDIR	:= $(DESTDIR)/man

.PHONY			: shell-fm manual all clean

all				: shell-fm manual

shell-fm		:
	make -C source

manual			:
	make -C manual

install			:
	mkdir -p $(DESTDIR)/bin $(MANDIR)/man1
	install source/shell-fm $(DESTDIR)/bin/shell-fm
	install manual/shell-fm.1.gz $(MANDIR)/man1

install-strip	: install
	strip $(DESTDIR)/bin/shell-fm

uninstall		:
	rm -f $(DESTDIR)/bin/shell-fm
	rm -f $(MANDIR)/man1/shell-fm.1.gz
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/bin
	rmdir --ignore-fail-on-non-empty $(MANDIR)/man1
	rmdir --ignore-fail-on-non-empty $(MANDIR)
	rmdir --ignore-fail-on-non-empty $(DESTDIR)


clean			:
	make -C source clean
	make -C manual clean

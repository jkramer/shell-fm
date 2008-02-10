
DESTDIR	:= /usr

.PHONY		: shell-fm manual all clean

all			: shell-fm manual

shell-fm	:
	make -C source

manual		:
	make -C manual

install		:
	mkdir -p $(DESTDIR)/bin $(DESTDIR)/man/man1
	install source/shell-fm $(DESTDIR)/bin/shell-fm
	strip $(DESTDIR)/bin/shell-fm
	install manual/shell-fm.1.gz $(DESTDIR)/man/man1

uninstall	:
	rm -f $(DESTDIR)/bin/shell-fm
	rm -f $(DESTDIR)/man/man1/shell-fm.1.gz
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/bin
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/man/man1
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/man
	rmdir --ignore-fail-on-non-empty $(DESTDIR)


clean		:
	make -C source clean
	make -C manual clean

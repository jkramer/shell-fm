
DESTDIR	:= /usr
MANDIR	:= $(DESTDIR)/man

.PHONY			: shell-fm manual all clean tags cscope

all				: shell-fm manual

shell-fm		:
	${MAKE} -C source

manual			:
	${MAKE} -C manual

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
	${MAKE} -C source clean
	${MAKE} -C manual clean


tags		: cscope.files
	@rm -f tags
	xargs -n50 ctags -a < cscope.files

cscope		: cscope.files
	cscope -b

cscope.files	:
	find source -name '*.[ch]' > cscope.files


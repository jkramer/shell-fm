
.PHONY		: shell-fm manual all

all			: shell-fm manual

shell-fm	:
	cd source && make && cd ..

manual		:
	cd manual && make && cd ..

install		:
	install source/shell-fm /usr/bin/shell-fm
	strip /usr/bin/shell-fm
	install manual/shell-fm.1.gz /usr/man/man1

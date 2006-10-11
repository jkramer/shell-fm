SUBDIRS = m4 src
ACLOCAL_AMFLAGS = -I m4


### generate tag and cscope files ###

.PHONY: cscope tags

cscope.files:
	find . -type f \( -name '*.c' -o -name '*.h' \) > $@

cscope.out: cscope.files
	cscope -b

tags: cscope.files
	-rm -f $@
	xargs -n50 ctags -a -f $@ < $^

cscope: cscope.out


## vim: noet ts=8 sw=8

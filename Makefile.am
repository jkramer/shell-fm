SUBDIRS = src
ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip


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

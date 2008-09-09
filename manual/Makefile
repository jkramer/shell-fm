
.PHONY	: clean

manual	: shell-fm.1.gz

shell-fm.1.gz : shell-fm.1
	gzip -9 -c $< > $@

clean	:
	rm -f shell-fm.1.gz

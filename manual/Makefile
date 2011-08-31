
.PHONY	: clean

manual	: shell-fm-macports.1.gz

shell-fm-macports.1.gz : shell-fm-macports.1
	gzip -9 -c $< > $@

clean	:
	rm -f shell-fm-macports.1.gz

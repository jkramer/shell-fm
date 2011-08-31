#!/bin/bash
# shell-colors.sh, prints possible fg/bg color combinations
# using escape sequences.
#
# Copyright (C) 2007 by Jonas Kramer.
# Published under the terms of the GNU General Public License (GPL).

echo -ne "FG/BG\t"
for bg in `seq 40 47`; do echo -ne "   $bg   "; done

for fg in `seq 30 37`; do
	echo -ne "\n$fg (N)\t"
	for bg in `seq 40 47`; do echo -ne "\x1B[$bg;${fg}m Normal \x1B[0m"; done
	echo -ne "\n$fg (B)\t"
	for bg in `seq 40 47`; do echo -ne "\x1B[$bg;${fg};1m  Bold  \x1B[0m"; done
done

echo

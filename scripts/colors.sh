#!/bin/bash
# Display ANSI colours.
# Source: http://www.ibiblio.org/pub/Linux/docs/HOWTO/unmaintained/Colour-ls
#
esc="\033["
echo -n "        40       41       42       43"
echo "      44       45       46       47  "
for fore in 30 31 32 33 34 35 36 37; do
    line1=" $fore "
    line2="    "
    for back in 40 41 42 43 44 45 46 47; do
	line1="${line1}${esc}${back};${fore}m Normal  ${esc}0m"
	line2="${line2}${esc}${back};${fore};1m Bold    ${esc}0m"
    done
    echo -e "$line1\n$line2"
done

#!/bin/bash
# vim:syntax=sh tabstop=4

extract ()
{
	KEY="$1"
	VALUE="$(
		sed -e "s/^$KEY\s*=\s*//" <<<"`
			grep -oE "^$KEY[ ]*=[ ]*[^ ]+" $HOME/.shell-fm/shell-fm.rc \
			|  head -n 1
		`"
	)"
	echo "$VALUE"
}


if [ $# -eq 0 ]; then
	echo "Usage: shell-fm-tune lastfm://radio_url [HOST [PORT]]"
	exit -1
fi


RADIO="$1"

IP=
PORT=
CMD=

if [ $# -gt 1 ]; then
	IP="$2"
	if [ $# -gt 2 ]; then PORT="$3"; fi
elif [ -r "$HOME/.shell-fm/shell-fm.rc" ]; then
	IP="`extract "bind"`"
	PORT="`extract "port"`"
fi


if [ -n "`which nc`" ]; then
	CMD="nc"
elif [ -n "`which telnet`" ]; then
	CMD="telnet"
else
	echo "Netcat or telnet must be installed!"
	exit -1
fi

[ -z "$IP" ] && IP="127.0.0.1"
[ -z "$PORT" ] && PORT="54311"
 
echo "play $RADIO" | "$CMD" "$IP" "$PORT" >/dev/null 2>&1

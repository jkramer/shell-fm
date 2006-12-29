#!/bin/bash

IP=127.0.0.1
PORT=54311
RADIO=$1
CMD=telnet

if [ -z "$RADIO" ] ; then
    echo "Usage: shell-fm-tune lastfm://radio_url"
    exit
fi
echo "play $RADIO" | $CMD $IP $PORT >/dev/null 2>&1

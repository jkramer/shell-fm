#!/usr/bin/perl

# Shell.FM UNIX Socket Control
# Copyright (C) by Jonas Kramer. All rights reserved.
# Published under the terms of the GNU General Public License (GPL).
#
#   Usage: $0 <command>
#
# See the shell-fm(1) manual for a list of commands.


use strict;
use warnings;

use IO::Socket::UNIX;
use IO::File;


die "Usage: $0 <command>\n" unless @ARGV;


my $rc = new IO::File("$ENV{HOME}/.shell-fm/shell-fm.rc");

die "Can't open configuration. $!.\n" unless $rc;


my $path;

for($rc->getlines) {
	$path = $1 if /^\s*unix\s*=\s*([^#\s]+)/;
}

$rc->close;

die "No socket path found.\n" unless $path;


my $socket = new IO::Socket::UNIX($path);

die "Failed to create socket. $!.\n" unless $socket;

$socket->print("@ARGV\n") or die("Failed to send command. $!.\n");
$socket->print("detach\n");

my $reply = $socket->getline;

print $reply if $reply;

$socket->close;

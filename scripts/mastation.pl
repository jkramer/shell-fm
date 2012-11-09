#!/usr/bin/env perl

use strict;
use warnings;

use feature qw( :5.10 );

use LWP::UserAgent;
use JSON;
use URI;

my $uri = URI->new('http://www.last.fm/ajax/getResource');
my $ua = LWP::UserAgent->new;
my $json = JSON->new;

my @artists;

for(@ARGV) {
	$uri->query_form(type => 'artist', name => $_);

	my $response = $ua->get($uri);

	if($response->is_success) {
		my $data = $json->decode($response->content);
		push @artists, $data;
	}
}

say 'artists/' . join(',', map { $_->{resource}->{id} } @artists);

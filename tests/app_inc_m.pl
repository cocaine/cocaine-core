#!/usr/bin/perl

use LWP::Simple;

package app_inc_m;

sub fetch_page {
	my $url = @_[0];
	my $page = get($url);
	return $page;
}

1;
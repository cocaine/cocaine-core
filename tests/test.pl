#!/usr/bin/perl

use LWP::Simple;

sub test_handle {
	my $in = @_[0];
	my $page = get($in);

	return $page;
}

1;

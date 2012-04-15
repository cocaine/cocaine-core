#!/usr/bin/perl

use lib qw(/home/rimz/usr/local/lib/perl/5.10.1/
/home/rimz/usr/local/lib/perl/5.10.1/auto/);

require "cocaine_dealer.pm";

my $path = cocaine_dealer::message_path->new("rimz_app", "rimz_func");
my $policy = cocaine_dealer::message_policy->new(0, 0.0, 0.0, 10);

my $config_path = "/home/rimz/cocaine-core/tests/config_example.json";
my $client = cocaine_dealer::client->new($config_path);

my $response = cocaine_dealer::response->new();
my $message = "here's chunk: ";

$client->send_message($message, length($message), $path, $policy, $response);

my $data_container = cocaine_dealer::data_container->new();
while ($response->get($data_container)) {
	print $data_container->data()."\n";
}

1;

# building and installing test env:
# perl Makefile.PL PREFIX=/home/rimz/usr/local && make && make install


#print "--- message path ---\n";
#print "service name: ".$path->service_name()."\n";
#print "handle name: ".$path->handle_name()."\n";
#print "\n--- message policy ---\n";
#print "send to all hosts: ".$policy->send_to_all_hosts()."\n";
#print "urgent: ".$policy->urgent()."\n";
#print "mailboxed: ".$policy->mailboxed()."\n";
#print "timeout: ".$policy->timeout()."\n";
#print "deadline: ".$policy->deadline()."\n";
#print "max timeout retries: ".$policy->max_timeout_retries()."\n";

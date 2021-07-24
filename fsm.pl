#!/usr/bin/perl
use strict;
use warnings;

my @fsm = (
	'0' => { 'g' => '1' },
	'1' => { 'e' => '2' },
	'2' => { 'm' => '3' },
	'3' => { 'i' => '4' },
	'4' => { 'n' => '5' },
	'5' => { 'i' => '6' },
	'6' => { ':' => '7' },
	'7' => { '/' => '8' },
	'8' => { '/' => '9' },

	'9' => { 'a-z' => '10' },
	'9' => { 'A-Z' => '10' },
	'9' => { '0-9' => '10' },
	'9' => { '.'   => '10' },

	'10' => { 'a-z' => '10' },
	'10' => { 'A-Z' => '10' },
	'10' => { '0-9' => '10' },
	'10' => { '.'   => '10' },
	'10' => { ':'   => '11' },
	'10' => { '/'   => '12' },

	'11' => { '0-9' => '11' },
	'11' => { '/'   => '12' },

	'12' => { 'ALL' => '12' },
);

my %STATES;

for (my $i = 0; $i < @fsm; $i += 2) {
	for my $c (keys %{$fsm[$i+1]}) {
		if ($c eq 'ALL') {
			for (my $x = 0; $x < 256; $x++) {
				$STATES{$fsm[$i]}[$x] = $fsm[$i+1]{$c};
			}
		} elsif (length($c) == 1) {
			$STATES{$fsm[$i]}[ord($c)] = $fsm[$i+1]{$c};
		} elsif ($c =~ m/^(.)-(.)$/) {
			my ($a, $b) = (ord($1), ord($2));
			($a, $b) = ($b, $a) if $a > $b;
			for (my $x = $a; $x <= $b; $x++) {
				$STATES{$fsm[$i]}[$x] = $fsm[$i+1]{$c};
			}
		}
	}
}

print "static int STATES[13][256] = {\n";
for (my $st = 0; $st <= 12; $st++) {
	print "\t{";
	for (my $i = 0; $i < 256; $i++) {
		printf "%s %d", $i == 0 ? '' : ',', defined($STATES{$st}[$i]) ? $STATES{$st}[$i] : -1;
	}
	print "},\n";
}
print "};\n";

#use Data::Dumper;
#print Dumper(\%STATES);

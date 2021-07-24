#!/usr/bin/perl
use strict;
use warnings;

my @fsm = (
	'0' => { '/'   => '1' },

	'1' => { 'ALL' => '4' },
	'1' => { '/'   => '1' },
	'1' => { '.'   => '2' },

	'2' => { 'ALL' => '4' },
	'2' => { '.'   => '3' },
	'2' => { '/'   => '1' },

	'3' => { 'ALL' => '4' },
	'3' => { '/'   => '1' },

	'4' => { 'ALL' => '4' },
	'4' => { '/'   => '1' },
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

print "static int STATES[5][256] = {\n";
for (my $st = 0; $st <= 4; $st++) {
	print "\t{";
	for (my $i = 0; $i < 256; $i++) {
		printf "%s %d", $i == 0 ? '' : ',', defined($STATES{$st}[$i]) ? $STATES{$st}[$i] : -1;
	}
	print "},\n";
}
print "};\n";

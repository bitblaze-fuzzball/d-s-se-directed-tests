#!/usr/bin/perl

use strict;

my @all = qw(b1 b2 b3 b4 s1 s2 s3 s4 s5 s6 s7 f1 f2 f3);
my $N = 5;


for my $b (@all) {
    for my $t ("baseline", "guided", "guided2") {
	my $short =
	  {"baseline" => "U ", "guided" => "D ", "guided2" => "D2"}->{$t};
	print "$b-$short: ";
	my $num_seen = 0;
	my($tot_iter, $tot_time);
	for my $i (1 .. $N) {
	    my $fname = "$b-$t-$i.out.gz";
	    my $prog = "zcat";
	    if (not -e $fname) {
                $fname = "$b-$t-$i.out";
                $prog = "cat";
            }
	    next unless -e $fname;
	    $num_seen++;
	    open(LOG, "$prog $fname |");
	    my $last_iter = -1;
	    my $wall_time = 0;
	    while (<LOG>) {
		if (/^Iteration (\d+):/) {
		    $last_iter = $1;
		} elsif (/^### Wallclock time: (.*) s/) {
		    $wall_time = $1;
		}
	    }
	    close LOG;
	    printf "%5d/%7.1f ", $last_iter, $wall_time;
	    $tot_iter += $last_iter;
	    $tot_time += $wall_time;
	}
	if ($num_seen == $N) {
	    printf "| %5d/%7.1f", $tot_iter/$N, $tot_time/$N;
	}
	print "\n";
    }
    print "\n";
}

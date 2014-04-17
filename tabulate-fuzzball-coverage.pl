#!/usr/bin/perl

use strict;

my @all = qw(b1 b2 b3 b4 s1 s2 s3 s4 s5 s6 s7 f1 f2 f3);
my $N = 5;

my %prog_size =
  ("b1" => 1075+2120,
   "b2" => 1290+2178,
   "b3" =>  719+3058,
   "b4" =>  394+3621,
   "s1" =>  929+2021,
   "s2" =>  524+2750,
   "s3" =>  318+1653,
   "s4" =>  370+2447,
   "s5" =>  392+1282,
   "s6" =>  595+2247,
   "s7" =>  957+2595,
   "f1" =>  571+1561,
   "f2" =>  807+2549,
   "f3" =>  684+1639,
  );

for my $b (@all) {
    for my $t ("baseline", "guided") {
	my $short =
	  {"baseline" => "U ", "guided" => "D ", "guided2" => "D2"}->{$t};
	print "$b-$short: ";
	my $num_seen = 0;
	my $tot_cover = 0;
	for my $i (1 .. $N) {
	    my $fname = "$b-$t-$i.out.gz";
	    my $prog = "zcat";
	    if (not -e $fname) {
		$fname = "$b-$t-$i.out";
		$prog = "cat";
	    }
	    next unless -e $fname;
	    open(LOG, "$prog $fname |");
	    my $best_cover = -1;
	    while (<LOG>) {
		if (/^Coverage increased to (\d+) on/) {
		    my $cover = $1;
		    die unless $cover > $best_cover;
		    $best_cover = $cover;
		} elsif (/^Final coverage: (\d+)$/) {
		    my $cover = $1;
		    die unless $cover >= $best_cover;
		    $best_cover = $cover;
		}
	    }
	    close LOG;
	    printf "%5d ", $best_cover;
	    $tot_cover += $best_cover;
	    $num_seen++ unless $best_cover == -1;
	}
	if ($num_seen == $N) {
	    my $avg_cover = $tot_cover/$N;
	    my $pct = 100 * ($avg_cover/$prog_size{$b});
	    printf "| %5d  %3.1f%%", $avg_cover, $pct;
	}
	print "\n";
    }
    print "\n";
}

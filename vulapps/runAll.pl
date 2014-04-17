#!/usr/bin/perl -w

use strict;

my $histDir = ".";
my $sDir = "$histDir/sendmail";
my $bDir = "$histDir/bind";
my $fDir = "$histDir/wu-ftpd";


my $log = "$histDir/runAll.out";
if (-e $log) {
    `/bin/rm $log`;
}

open LF, ">$log" or die;

# SM1 crackaddr
&crun ("$sDir/s1", "ca", "< s1.in");

# SM2 gecos
&crun ("$sDir/s2", "ge", "");

# SM3 mime1
&crun ("$sDir/s3", "m1", "s3.in");

# SM4 mime2
&crun ("$sDir/s4", "m2", "s4.in");

# SM5 prescan
&crun ("$sDir/s5", "prescan", "");

# SM6 
&crun ("$sDir/s6", "ttflag", "-d 4294967200-100");

# SM7
&crun ("$sDir/s7", "txtdns", "");

# BIND1 
&crun ("$bDir/b1", "nxt", "");

# BIND2 
# need to have run the program "create" in this dir fairly recently too..
&crun ("$bDir/b2", "sig", "");

# BIND3 
&crun ("$bDir/b3", "iquery", "b3.in");

# BIND4
&crun ("$bDir/b4", "nsl", "b4.in");

# FTP1 
&crun ("$fDir/f1", "mp", "f1.in");

# FTP2 
# NB: this file must exist..  DOn't move it or change dir structure above it (length of this string is critical) 
# this abs filename is 45 chars (46 with \0) which is why MAXPATHLEN is set to 46. 
&crun ("$fDir/f2", "obo", "/tmp/foo/bar/foo/bar/foo/bar/foo/bar/abcdefgh");

# FTP3
&crun ("$fDir/f3", "rp", "/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aaa/aa/aaa");









sub crun () {
    my ($dir, $obj, $args) = @_;
    
    my $bobj = "$obj-bad";
    my $oobj = "$obj-ok";
    my $ofb = "$bobj.out";
    my $ofo = "$oobj.out";
    &run ("cd $dir; ./$bobj $args > $ofb 2>&1");
    &run ("cd $dir; ./$oobj $args > $ofo 2>&1");
}    








sub run () {
    my ($commands) = @_;

    print LF "\n\n\n$commands\n\n";
    my $rc;
    

    $rc = 0xffff & system ("$commands");
    

    $rc /= 256;
    # seg fault
    if ($rc == 139) {
	print LF "SEGFAULT\n";
    }
    # assert failed 
    elsif ($rc == 134) {
	print LF "ASSERT\n";
    }
    elsif ($rc == 0) {
	print LF "NORMAL\n";
    }
    else {
	print LF "UNKNOWN\n";
    }

    return ($rc);    

}

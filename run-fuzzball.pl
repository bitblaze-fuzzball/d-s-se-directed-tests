#!/usr/bin/perl

use Cwd;
use IPC::Run "run";
use IO::Handle;

my $pwd = getcwd;

my $fuzzball = "$pwd/cfg_fuzzball";
my $stp = "$pwd/fuzzball/stp/stp";
my $static_results = "$pwd/cfg/MIT";
my $apps_dir = "$pwd/vulapps";
my $plt = "$pwd/path-length-test";

my $timeout_secs = 6 * 60 * 60;
my @timeout_cmd = ("/usr/bin/timeout", $timeout_secs + 60);
my $time = "/usr/bin/time";
my $time_fmt = "### Exit status: %x\\n### Wallclock time: %e s\\n" .
  "### Maximum resident size: %M kiB";

my @all = qw(b1 b2 b3 b4 s1 s2 s3 s4 s5 s6 s7 f1 f2 f3);

my %benchmark_dir =
  ("b1" => "bind/b1",
   "b2" => "bind/b2",
   "b3" => "bind/b3",
   "b4" => "bind/b4",
   "s1" => "sendmail/s1",
   "s2" => "sendmail/s2",
   "s3" => "sendmail/s3",
   "s4" => "sendmail/s4",
   "s5" => "sendmail/s5",
   "s6" => "sendmail/s6",
   "s7" => "sendmail/s7",
   "f1" => "wu-ftpd/f1",
   "f2" => "wu-ftpd/f2",
   "f3" => "wu-ftpd/f3");

my %binary_name =
  ("b1" => "nxt-bad-myresolv-mylibc-diet",
   "b2" => "sig-bad-diet",
   "b3" => "iquery-bad-diet",
   "b4" => "nsl-bad-diet",
   "s1" => "ca-bad-mylibc-diet",
   "s2" => "ge-bad-diet",
   "s3" => "m1-bad-diet",
   "s4" => "m2-bad-diet",
   "s5" => "prescan-bad-diet",
   "s6" => "ttflag-bad-mylibc-diet",
   "s7" => "txtdns-bad-diet",
   "f1" => "mp-bad-mylibc-diet",
   "f2" => "obo-bad-diet",
   "f3" => "rp-bad-diet");

my %inputs =
  ("b1" => ["testcase.init"],
   "b2" => ["testcase.init"],
   "b3" => ["testcase.init"],
   "b4" => ["testcase.init"],
   "s1" => ["testcase.init"],
   "s2" => ["testcase.init"],
   "s3" => ["testcase.init"],
   "s4" => ["testcase.benign"],
   "s5" => ["testcase.init"],
   "s6" => ["testcase.init"],
   "s7" => ["testcase.init"],
   "f1" => ["testcase.init"],
   "f2" => ["testcase"],
   "f3" => ["testcase.init"],
  );

my @base_flags =
  ("-linux-syscalls",
   "-trace-syscalls",
   "-stp-path" => $stp,
   "-trace-iterations",
   "-trace-assigns-string",
   "-trace-stopping",
   "-coverage-stats",
   "-time-stats",
   "-zero-memory",
   # "-iteration-limit" => 4000,
   "-finish-on-nonfalse-cond",
   "-total-timeout", => $timeout_secs,
   "-symbolic-syscall-error" => -2, # ENOENT
  );

my %chdir_to =
  ("b1" => "",
   "b2" => "",
   "b3" => "",
   "b4" => "",
   "s1" => "",
   "s2" => "",
   "s3" => "",
   "s4" => "",
   "s5" => "",
   "s6" => "",
   "s7" => "",
   "f1" => "",
   "f2" => "/tmp/foo/bar/foo/bar/foo/bar/foo/ba",
   "f3" => "/tmp/f3",
  );


my %bug_cond =
  (
   "b1" => [0x08048759, "R_EAX:reg32_t >= 0x7fffffff:reg32_t"],
   "b2" => [0x080489dc, "R_EAX:reg32_t >= 0x7fffffff:reg32_t"],
   "b3" => [0x080482b4, "R_EAX:reg32_t >= 512:reg32_t"],
   "b4" => [0x08048230, "R_EAX:reg32_t > 999:reg32_t"],
   "s1" => [0x080481f4, "(R_EBX:reg32_t - 0x804b462:reg32_t) > 31:reg32_t"],
   "s2" => [0x080486aa, "R_EAX:reg32_t > 5:reg32_t"],
   "s3" => [0x0804831f, "(R_EAX:reg32_t - 0xbfffcece:reg32_t) >= 50:reg32_t"],
   "s4" => [0x08048567,
	    "mem[R_EBP:reg32_t - 0x26:reg32_t]:reg32_t <> 0x444f4f47:reg32_t"],
   "s5" => [0x08048269, "(R_ESI:reg32_t - 0xbfffcf09:reg32_t) > 50:reg32_t"],
   "s6" => [0x080483d9, "R_EAX:reg32_t >= 100:reg32_t"],
   "s7" => [0x08048777, "R_EDX:reg32_t > 30:reg32_t"],
   "f1" => [0x08048317,
	    "mem[R_EBP:reg32_t - 12:reg32_t]:reg32_t <> 7:reg32_t"],
   "f2" => [0x08048a20, "R_EAX:reg32_t > 0x2e:reg32_t"],
   "f3" => [0x08048268, "R_EAX:reg32_t <> 10:reg32_t"],
  );

my %symb_buffer =
  (
   #        fuzz-s-addr buf addr    length  flags
   "b1" => [0x080483e8, 0x50001008,  104,     ""],
   "b2" => [0x08048b8d, 0x50001008,  114,     ""],
   "b3" => [0x0804838a, 0x50002008,  511,     ""],
   "b4" => [0x0804861b, 0xbfffcb90,  975,     "-fulllen"],
   "s1" => [0x080487c2, 0xbfffcf14,   35,     ""],
   "s2" => [0x080487c6, 0x50000108,  344,     ""],
   "s3" => [0x08049464, 0x50001008,  177,     ""],
   "s4" => [0x08048e34, 0x50003008,  150,     "-fulllen"],
   "s5" => [0x08048555, 0x50000008,  110,     ""],
   "s6" => [0x08048254, 0x50002008,   20,     ""],
   "s7" => [0x08048893, 0xbfffcb20,   52,     ""],
   "f1" => [0x080483b4, 0xbfffcf5e,   29,     ""],
   "f2" => [0x080481cc, 0xbfffcf4e,   10,     ""],
   "f3" => [0x080482fe, 0x50002008,   41,     "-fulllen"],
  );

my %warn1 =
  (
   #        target (main prog)     warn (may be in library)
   "b1" => [0x0804876a,            0x080493be],
   "b2" => [0x080489ed,            0x08049549],
   "b3" => [0x080482c8,            0x08048e79],
   "b4" => [0x0804822b,            0x08048a90],
   "s1" => [0x080481f4,            0x080481f4],
   "s2" => [0x0804869a,            0x0804869a],
   "s3" => [0x0804831f,            0x0804831f],
   "s4" => [0x08048547,            0x08048547],
   "s5" => [0x08048269,            0x08048269],
   "s6" => [0x080483dd,            0x080483dd],
   "s7" => [0x08048782,            0x0804a200],
   "f1" => [0x0804812d,            0x080484cd],
   "f2" => [0x08048a0d,            0x08049197],
   "f3" => [0x080487b5,            0x08049473],
  );


my %warn2 =
  (
   #        target (main prog)     warn (may be in library)
   "b4" => [0x08048192,            0x08049ab8],
   "s1" => [0x080486d9,            0x080486d9],
   "s2" => [0x08048674,            0x08048e27],
   "s3" => [0x080482ec,            0x080482ec],
   "s4" => [0x0804845b,            0x0804845b],
   "s5" => [0x0804823e,            0x0804823e],
   "s7" => [0x0804879b,            0x0804879b],
   "f1" => [0x080481e7,            0x080484cd],
   "f3" => [0x08048aed,            0x080494d3],
  );

my %warn;

my $s3_hint;
{
    my @a;
    for (0 .. 176) {
	$op = $_ % 3 ? "<>" : "==";
	push @a, "input0_$_:reg8_t $op 10:reg8_t"
    }
    $s3_hint = join(" & ", @a)
}

my %special_flags =
  (
   "b1" => [],
   "b2" => ["-symbolic-syscall-error" => 1290143289 # time()
	   ],
   "b3" => ["-symbolic-word" => "0xbfffcf28=len"
	   ],
   "b4" => [],
   "s1" => [],
   "s2" => ["-skip-call-ret" => "0x08048575=0", # getpwent()
	   ],
   "s3" => [#"-extra-condition" => $s3_hint
	   ],
   "s4" => [],
   "s5" => [],
   "s6" => ["-skip-func-ret" => "0x08049008=10", # Skip printf()
	   ],
   "s7" => ["-skip-call-ret-symbol" => "0x08048ca2=malloc",
	   ],
   "f1" => ["-skip-func-ret" => "0x0804888c=0", # chdir() -> success
	   ],
   "f2" => ["-skip-call-ret" => "0x08048b80=0", # chdir() -> success
	   ],
   "f3" => [#"-skip-func-ret" => "0x08049588=10", # Skip printf()?
	   ],
  );

sub addr { sprintf "0x%08x", $_[0] }

sub fuzzball_cmd {
    my($b, $baseline, $num) = @_;
    my $dir = $benchmark_dir{$b};
    my $prog = $binary_name{$b};
    my @guide_opts;
    if ($baseline) {
	@guide_opts = ();
    } else {
	@guide_opts =
	  (
	   "-cfg" => "$static_results/$dir/$prog.cfg",
	   "-warn-file" => "$static_results/$dir/$prog.slice",
	   "-target-addr" => addr($warn{$b}[0]),
	   "-warn-addr" => addr($warn{$b}[1]),
	  );
    }
    my @cmd =
      (@timeout_cmd, $time, "-f" => $time_fmt,
       $fuzzball, "$apps_dir/$benchmark_dir{$b}/$prog",
       @base_flags,
       "-check-condition-at" => addr($bug_cond{$b}[0]) . ":$bug_cond{$b}[1]",
       "-fuzz-start-addr" => addr($symb_buffer{$b}[0]),
       "-symbolic-cstring$symb_buffer{$b}[3]" =>
          addr($symb_buffer{$b}[1]) . "+$symb_buffer{$b}[2]",
       @{$special_flags{$b}},
       @guide_opts,
       "-random-seed" => $num,
       "--",
       "$apps_dir/$benchmark_dir{$b}/$prog", @{$inputs{$b}});
}

sub plt_cmd {
    my($b) = @_;
    my $dir = $benchmark_dir{$b};
    my $prog = $binary_name{$b};
    my @cmd =
      ($plt,
       "--cfg" => "$static_results/$dir/$prog.cfg",
       "--warn-file" => "$static_results/$dir/$prog.slice",
       "--target-addr" => $warn{$b}[0],
       "--warn-addr" => $warn{$b}[0],
       );
    return @cmd;
}

# This is not correct in all cases, but should be sufficient for
# debug messages
sub shellify_cmd {
    my(@cmd) = @_;
    my @sc;
    for my $c (@cmd) {
	if ($c =~ /^[-a-zA-Z0-9+=.:\/_]+$/) {
	    push @sc, $c;
	} else {
	    push @sc, "'$c'";
	}
    }
    return join(" ", @sc);
}

sub run_a_benchmark {
    my($b, $baseline, $num) = @_;

    my @cmd = fuzzball_cmd($b, $baseline, $num);

    my $dir;
    if ($chdir_to{$b} eq "") {
	$dir = "$apps_dir/$benchmark_dir{$b}";
    } else {
	$dir = $chdir_to{$b};
    }

    my $suffix;
    if ($baseline) {
	$suffix = "baseline";
    } elsif ($ARGV[2] == "1") {
	$suffix = "guided2";
    } else {
	$suffix = "guided";
    }
    if (not $baseline and not exists $warn{$b}) {
	print "No second warning\n";
	return;
    }

    # simulate @cmd | tee $b.out
    open(LOG, ">", "$b-$suffix-$num.out") or die;
    chdir($dir) or die "chdir to $dir failed: $!";
    my $scmd = shellify_cmd(@cmd);
    print LOG $scmd, "\n";
    print $scmd, "\n";
    my $out_fh;
    run \@cmd, '&>',
      sub { my($data) = @_;
	    print LOG $data;
	    print $data;
	};
    chdir($pwd);
}

sub run_plt {
    my($b) = @_;

    my @cmd = plt_cmd($b);
    print STDERR join(" ", @cmd), "\n";
    system(@cmd);
}

if ($ARGV[2] == "1") {
    %warn = %warn2;
} else {
    %warn = %warn1;
}

#run_plt($ARGV[0]);
for my $i (1 .. 5) {
    run_a_benchmark($ARGV[0], $ARGV[1], $i);
}

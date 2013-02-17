#!/usr/bin/env perl

use strict;

my $workdir = ($#ARGV >= 0) ? $ARGV[0] : '.';
my $infile = $workdir . '/hostnames.txt';

if (! -e $infile) {
    print "File [$infile] does not exist; check if you provided a valid workdir.\n";
    print "Usage: $0 [workdir] [outfile]\n";
    exit 1;
}

my $data = '';

# read file
open(INFILE, "$infile");
while(<INFILE>) {
    $data .= $_;
}
close(INFILE);

# remove ^M
$data =~ s/\r//g;

my %macs = ();
my @lines = ();

# 1st pass, saving lines to array and replacing the dest mac
while ($data =~ /(192\.168\.\d+\.\d+)\n(\d+)\n/mg) {
    my $ip = $1;
    my $hostname = $2;

    my $dir = $workdir . '/' . $ip . '/throughput';
    if (-d $dir) {
        for (my @files = glob( $dir . '/*' )) {
            open(INFILE, "$_");
            my $line = <INFILE>;
            if ($line =~ /((\w+:\w+:\w+:\w+:\w+:\w+) received \d+ bytes from \w+:\w+:\w+:\w+:\w+:\w+ in \d+ ms on \d+ \d+ \d+ at \d+:\d+:\d+)/) {
                my $line = $1;
                my $mac = $2;
                print("line: [$line] -- ");
                $line =~ s/$mac/$hostname/;
                print("[$line]\n");
                push(@lines, $line);
                $macs{$mac} = $hostname unless defined($macs{$mac});
                print("mac: $mac = $hostname\n");
            }
            close(INFILE);
        }
    }
}

if ($#lines > 0) {
    # 2nd-pass, replacing source mac and saving to file
    my $outfile = ($#ARGV >= 1) ? $ARGV[1] : 'throughput.txt';
    open(OUTFILE, ">$outfile");
    for my $i (0 .. $#lines) {
        my $line = $lines[$i];
        if ($line =~ /from (\w+:\w+:\w+:\w+:\w+:\w+) in/) {
            my $mac = $1;
            $line =~ s/$mac/$macs{$mac}/g;
        }
        print OUTFILE $line . "\n";
    }
    print "Data written to file $outfile\n";
close(OUTFILE);
} else {
    print "No data written\n";
}
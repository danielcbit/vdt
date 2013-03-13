#!/usr/bin/env perl

use strict;
my $workdir = ($#ARGV >= 0) ? $ARGV[0] : '.';
my $infile = $workdir . '/hostnames_and_mac.txt';

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

my %hostnames = ();
my @lines = ();

# 1st pass, mapping the hostnames/macs
while ($data =~ /192\.168\.\d+\.\d+\n(\w+:\w+:\w+:\w+:\w+:\w+)\n(\d+)/mg) {
    my $mac = $1;
    my $hostname = $2;
    $hostnames{$mac} = $hostname;
}

my $outfile = ($#ARGV >= 1) ? $ARGV[1] : 'throughput.txt';
open(OUTFILE, ">$outfile");

# 2nd pass, replace the macs with the hostname and change the printing format
while ($data =~ /(192\.168\.\d+\.\d+)\n/g) {
    my $ip = $1;
    my $dir = $workdir . '/' . $ip . '/throughput';
    if (-d $dir) {
        for (my @files = glob( $dir . '/*' )) {
            open(INFILE, "$_");
            my $line = <INFILE>;
            if ($line =~ /(\w+:\w+:\w+:\w+:\w+:\w+) received (\d+) bytes from (\w+:\w+:\w+:\w+:\w+:\w+) in (\d+) ms on \d+ \d+ \d+ at (\d+:\d+:\d+)/) {
                my ($dstmac, $srcmac, $bytes, $bytes, $timeinms, $datetime);
                # HOST_1 HOST_2 hora_do_contato qtde_dados tempo_decorrido_em_ms 0.0 0.0
                $dstmac = $1;
                $bytes = $2;
                $srcmac = $3;
                $timeinms = $4/1000;
                $datetime = $5;
                # print("[hostame: $hostname] [srcmac: $srcmac] [dstmac; $dstmac]\n");
                $line = sprintf("%s %s %s %s %.1f 0 0\n", $hostnames{$srcmac}, $hostnames{$dstmac}, $datetime, $bytes, $timeinms);
                push(@lines, $line);
            }
            close(INFILE);
        }
    }
}

if ($#lines > 0) {
    my $outfile = ($#ARGV >= 1) ? $ARGV[1] : 'throughput.txt';
    open(OUTFILE, ">$outfile");
    print OUTFILE @lines;
    close(OUTFILE);
    print "Data written to file '$outfile'\n";
} else {
    "No data written.\n";
}

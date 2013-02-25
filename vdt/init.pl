#!/usr/bin/perl
#
# Main script to start up DieselNet within a guest domain.
#
#Notes:
#  - Called from /etc/rc.local
#  - This script will always be run from the root directory
#  - Scripts may run after the console login prompt is available

$ramdir = "/dieselram"; #ramdisk mount point
$macFile = '/tmp/MAC';
#&ramdisk_init();
#&tmpfs_init();
#&make_dirs();
#&copy_files();
&writeMAC();
&run_scripts();
exit 0;

###BEGIN SUBS###

# Called to issue a warning.
sub warning {
    print STDERR "WARNING: " . $_[0] . "\n";
}

sub getIP
{
my $ip;
my $device = "eth0";
	if ((`/sbin/ifconfig $device`) =~ /inet addr:(\d+\.\d+\.\d+\.\d+)/) {
		$ip = $1;
	}
	print $ip;
	return $ip;
}

# Try to get the MAC from an Atheros minipci device. This relies
# on the default interface being ath0 (not wlan0!).
sub getMAC() {
    if (! -e '/sys/class/net/eth0/address') {
        &stop('Unable to locate sysfs entry for eth0');
    }
    # Madwifi: read the MAC.
    if (!open(ETH, '< /sys/class/net/eth0/address')) {
        &stop('Using madwifi driver but unable to get the MAC address');
    }
    my $mac = <ETH>;
    chomp $mac;
    close(ETH);
    # Validate the MAC.
    $mac =~ s/^\s+//;
    $mac =~ s/\s+$//;
    $mac = uc $mac;
    my $dd = '[0-9A-F]{2,2}';
    if ($mac !~ /^$dd:$dd:$dd:$dd:($dd):($dd)$/) {
        &stop('MAC from madwifi device is malformed');
    }
    return $mac;
}

# Write the eth0 MAC to a file.
sub writeMAC {
    my $mac = &getMAC();
    if (-e $macFile) {
        if (!unlink($macFile)) {
            &warning("Cannot delete existing $macFile; will not write MAC");
            return 0;
        }
    }
    if (!open(M, "> $macFile")) {
        &warning("Unable to create file $macFile; will not write MAC");
        return 0;
    }
    print M $mac;
    close(M);
    return 1;
}

#
# tmpfs_init: Create the tmpfs file system to be mounted at /dieselram. This
#    is mutually exclusive with ramdisk_init().
#
sub tmpfs_init {
    unlink($ramdir) if (-f $ramdir);
    mkdir($ramdir) unless (-d $ramdir);
    my $tmpfsInodes = "500";
    my $tmpfsSz = "64M";
    `mount -n -t tmpfs -o size=$tmpfsSz,nr_inodes=$tmpfsInodes,mode=0755 tmpfs $ramdir`;
}

#
# ramdisk_init: Create the RAM disk, /dieselram.
#
sub ramdisk_init() {
    if (! -e $ramdir) {
        mkdir($ramdir);
    } else {
        my $r = system('df /dev/ram0 | grep /dev/ram0 >/dev/null');
        if ($r == 0) {
            print "ramdisk already exists\n";
            return;
        }
        system("rm -f $ramdir/*");
    }
    `mke2fs /dev/ram0`;
    `mount /dev/ram0 $ramdir`;
}

#
# make_dirs: Create directories if they don't already exist.
#
sub make_dirs() {
    my @dirs = (
        '/logging/prismslogs',
        '/dieselDisk/LiveIPlogs',
        '/dieselDisk/dtnTest'
        );
    foreach my $dir (@dirs) {
        system("mkdir -p $dir") unless ( -e $dir);
    }
    symlink '/logging/prismslogs', '/prismslogging';
    my $oldLogDir = '/dieselDisk/dtnTest/logs';
    rmdir ($oldLogDir) if (-d $oldLogDir);
    symlink '/logging/prismslogs', $oldLogDir;
}

#
# copy_files: Copy files, generally from the /dieselDisk partition to the
# root partition.
#
sub copy_files() {
    system("cp /diesel/MAC $ramdir/MAC") if (-e '/diesel/MAC');
    system('cp /diesel/liveip/default.bound /etc/udhcpc/default.bound');
    system('chmod +x /etc/udhcpc/default.bound');
    system('cp /diesel/liveip/default.reuse /etc/udhcpc/default.reuse');
    system('chmod +x /etc/udhcpc/default.reuse');
    system('cp /diesel/liveip/default.renew /etc/udhcpc/default.renew');
    system('chmod +x /etc/udhcpc/default.renew');
}

#
# run_scripts: This is where all the processes get started.
#
sub run_scripts () {
    my $startThruput = 1;     # Set to 1 to start throughput tests.
    # Start throughput tests.
    if ($startThruput) {
	my $ipaddr = &getIP('eth0');
		#Runs throughput with verbose, disabling bus-host tests and on eth0 only.
        my $optDisable = "-e -v -h -i $ipaddr";
		print $optDisable;
        system("nohup /root/tests/throughput.sh $optDisable > /root/logs/tool.log 2> /root/logs/tool.error &");
    }
}

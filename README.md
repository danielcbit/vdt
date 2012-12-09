VDT - Virtual DTN Testbed

1. What is:
	VDT is a Hybrid DTN Testbed that uses concepts of Virtual Machines and Virtual Networks to provide a scalable, reliable and flexible environment for developing DTN performance evaluations and studies.

2. Install:
	VDT is self contained as a script-like NS3 program. It uses libvirt for managing virtual machines and brctl and tunctl to manage bridges and tap interfaces.

2.1. Dependencies:
	First, some dependencies must be installed in order to sucessfuly build vdt. Here we present the list of packages needed for a sucessful install on Ubuntu. It is assumed that the compiler and headers for building applications (like those on build-essential in ubuntu) are already installed.

2.1.1. KVM
	Packages: kvm libvirt-bin bridge-utils
	After installing these packages it is necessary to add the current user to kvm and libvirtd groups in order to allow the user manage virtual machines. This can be done with:

	sudo usermod -a -G kvm <user>
	sudo usermod -a -G libvirtd <user>

	After adding user to these groups, it is necessary to log out and log in (or reboot) in order to make the changes efective.

2.1.2. libvirt
	Packages: libvirt-dev virsh
	These packages allow the correct build of VDT and offer the virsh line command to manage and see current status of virtual machines.

2.1.3. NS-3
	Packages: libxml2-dev libsqlite3-dev

2.2 Building
	In the VDT source directory all it is needed it to build VDT follows a common NS-3 build step with a minor change.

	1. ./waf configure
	2. ./waf build

	To enable sudo usage for managing tap interfaces:
	3. ./waf configure --enable-sudo
	4. ./waf build (type sudo user password when prompted)

3. Using
	From root directory of VDT run it with the command:

	./waf --run "vdt-tap-wifi-virtual-machine --traceFile=<location to mobility_traces>/mobility_traces.txt --mNodes=<number of mobile nodes> --sNodes=<number of static nodes> --duration=<run duration in seconds> --logFile=main-ns2-mob.log --sXMLTemplate=<location of static node.xml template>/node.xml --mXMLTemplate=<location of mobile node.xml template>/node.xml --srcImage=<location of node image>/hydra-node.image"

A example run line:

	./waf --run "vdt-tap-wifi-virtual-machine --traceFile=/home/daniel/workspace/ns-3-dev/vdt/mobility_traces.txt --mNodes=5 --sNodes=0 --duration=3600.0 --logFile=main-ns2-mob.log --sXMLTemplate='/home/daniel/workspace/ns-3-dev/vdt/node.xml' --mXMLTemplate='/home/daniel/workspace/ns-3-dev/vdt/node.xml' --srcImage='/home/daniel/workspace/ns-3-dev/vdt/setup/hydra-node.image'"

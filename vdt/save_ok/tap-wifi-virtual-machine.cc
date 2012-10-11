/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//
// This is an illustration of how one could use virtualization techniques to
// allow running applications on virtual machines talking over simulated
// networks.
//
// The actual steps required to configure the virtual machines can be rather
// involved, so we don't go into that here.  Please have a look at one of
// our HOWTOs on the nsnam wiki for more details about how to get the 
// system confgured.  For an example, have a look at "HOWTO Use Linux 
// Containers to set up virtual networks" which uses this code as an 
// example.
//
// The configuration you are after is explained in great detail in the 
// HOWTO, but looks like the following:
//
//  +----------+                           +----------+
//  | virtual  |                           | virtual  |
//  |  Linux   |                           |  Linux   |
//  |   Host   |                           |   Host   |
//  |          |                           |          |
//  |   eth0   |                           |   eth0   |
//  +----------+                           +----------+
//       |                                      |
//  +----------+                           +----------+
//  |  Linux   |                           |  Linux   |
//  |  Bridge  |                           |  Bridge  |
//  +----------+                           +----------+
//       |                                      |
//  +------------+                       +-------------+
//  | "tap-left" |                       | "tap-right" |
//  +------------+                       +-------------+
//       |           n0            n1           |
//       |       +--------+    +--------+       |
//       +-------|  tap   |    |  tap   |-------+
//               | bridge |    | bridge |
//               +--------+    +--------+
//               |  wifi  |    |  wifi  |
//               +--------+    +--------+
//                   |             |
//                 ((*))         ((*))
//
//                       Wifi LAN
//
//                        ((*))
//                          |
//                     +--------+
//                     |  wifi  |
//                     +--------+
//                     | access |
//                     |  point |
//                     +--------+
//
#include <iostream>
#include <fstream>
#include <sstream>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <libxml2/libxml/xmlmemory.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/tree.h>


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "pugixml.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TapWifiVirtualMachineExample");

// Prints actual position and velocity when a course change event occurs
static void
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
	Vector pos = mobility->GetPosition (); // Get position
	Vector vel = mobility->GetVelocity (); // Get velocity

	// Prints position and velocities
	*os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
			<< ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
			<< ", z=" << vel.z << std::endl;
}

std::string vmName(int num)
{
	stringstream ss;

	// the number is converted to string with the help of stringstream
	ss << num;
	string ret;
	ss >> ret;

	// Append zero chars
	int str_length = ret.length();
	for (int i = 0; i < 3 - str_length; i++)
		ret = "0" + ret;
	return "hydra-base" + ret;
}

std::string brName(int num)
{
	stringstream ss;

	// the number is converted to string with the help of stringstream
	ss << num;
	string ret;
	ss >> ret;

	// Append zero chars
	int str_length = ret.length();
	for (int i = 0; i < 3 - str_length; i++)
		ret = "0" + ret;
	return "br" + ret;

}

std::string tapName(int num)
{
	stringstream ss;

	// the number is converted to string with the help of stringstream
	ss << num;
	string ret;
	ss >> ret;

	return "tap" + ret;

}

std::string vnetName(int num)
{
	stringstream ss;

	// the number is converted to string with the help of stringstream
	ss << num;
	string ret;
	ss >> ret;

	return "vnet" + ret;

}

struct xml_string_writer: pugi::xml_writer
{
	std::string result;

	virtual void write(const void* data, size_t size)
	{
		result += std::string(static_cast<const char*>(data), size);
	}
};

void print_doc(const char* message, const pugi::xml_document& doc, const pugi::xml_parse_result& result)
{
	int i = 0;
	std::cout << "Name: " << doc.child("domain").child("devices").child("emulator").text().get() << "\n";
	for (i = 0; i<= 2; i++) {
		doc.child("domain").child("name").text().set(vmName(i).c_str());

		//Salva um dump do xml em uma variavel
		//xml_string_writer writer;
		//doc.print(writer);

		//std::cout << writer.result;
		std::cout << "Name: " << doc.child("domain").child("name").child_value() << "\n";
	}
	//imprimindo a interface br
	pugi::xml_attribute atributo = doc.child("domain").child("devices").child("interface").child("source").first_attribute();
	std::cout << "Interface BR" << atributo.value() << "\n";
	atributo.set_value("br1");
	std::cout << "Interface BR mod " << atributo.value() << "\n";

	xml_string_writer writer;
	doc.print(writer);

	std::cout << writer.result;


	pugi::xml_node tools = doc.child("domain").child("os");
	for (pugi::xml_node tool = tools.first_child(); tool; tool = tool.next_sibling())
	{
		std::cout << "Tool:";

		for (pugi::xml_attribute attr = tool.first_attribute(); attr; attr = attr.next_attribute())
		{
			std::cout << " " << attr.name() << "=" << attr.value();
		}

		std::cout << std::endl;
	}
}

/*void
CourseChange (std::string context, Ptr<const MobilityModel> model)
{
	Vector position = model->GetPosition ();
	NS_LOG_UNCOND (context <<
		" x = " << position.x << ", y = " << position.y);
}*/

int 
main (int argc, char *argv[])
{

	std::string traceFile;
	std::string logFile;
	std::string sXMLTemplateFile;
	std::string mXMLTemplateFile;

	int tNodes; //Total number of nodes = mNodes + sNodes.

	int    mNodes;
	double duration;

	int sNodes;

	// Enable logging from the ns2 helper
	//LogComponentEnable ("Ns2MobilityHelper",LOG_LEVEL_DEBUG);

	// Parse command line attribute
	CommandLine cmd;
	cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
	cmd.AddValue ("mNodes", "Number of mobile nodes", mNodes);
	cmd.AddValue ("sNodes", "Number of static nodes", sNodes);
	cmd.AddValue ("duration", "Duration of Simulation", duration);
	cmd.AddValue ("logFile", "Log file", logFile);
	cmd.AddValue ("sXMLTemplate", "Static XML Template", sXMLTemplateFile);
	cmd.AddValue ("mXMLTemplate", "Mobile XML Template", mXMLTemplateFile);
	cmd.Parse (argc,argv);

	// Check command line arguments
	if (traceFile.empty () || mNodes <= 0 || duration <= 0 || sNodes < 0 || logFile.empty () || mXMLTemplateFile.empty ())
	{
		std::cout << "Usage of " << argv[0] << " :\n\n"
				"./waf --run \"tap-wifi-virtual-machine-mobile"
				" --traceFile=/home/mgiachino/ns-3-dev/examples/mobility/default.ns_movements"
				" --mNodes=1 --sNodes=2 --duration=100.0 --logFile=main-ns2-mob.log --sXMLTemplate=template.xml --mXMLTemplate=template.xml\" \n\n"
				"NOTE 1: ns2-traces-file could be an absolute or relative path. You could use the file default.ns_movements\n"
				"        included in the same directory that the present file.\n\n"
				"NOTE 2: Number of mobile nodes present in the trace file must match with the command line argument mNodes and must\n"
				"        be a positive number. Note that you must know it before to be able to load it.\n\n"
				"NOTE 3: Number of static nodes must be a positive number or zero. If zero, it's assumed that all nodes\n"
				"        are mobile.\n\n"
				"NOTE 4: Duration must be a positive number. Note that you must know it before to be able to load it.\n\n"
				"NOTE 5: The XML templates are used to define the configuration of the mobile and static nodes.\n"
				"        The Static Node XML template can be left undefined.  \n";

		return 0;
	}

	// open XML config files
	std::ifstream sXML(sXMLTemplateFile.c_str ());
	std::ifstream mXML(mXMLTemplateFile.c_str ());

	std::string sXMLTemplate((std::istreambuf_iterator<char>(sXML)), std::istreambuf_iterator<char>());
	std::string mXMLTemplate((std::istreambuf_iterator<char>(mXML)), std::istreambuf_iterator<char>());

	tNodes = sNodes + mNodes; //Obtaining the total quantity of nodes

	//libvirt connection pointer
	virConnectPtr conn;

	//lista de domains
	virDomainPtr dom[tNodes];

	//lista que guarda as interfaces de cada dominio
	std::string domainInt[tNodes];

	//abre a conexao com o libvirt
	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	pugi::xml_document document;
	//pugi::xml_parse_result resultado =
	document.load(mXMLTemplate.c_str ());
	pugi::xml_document domainDoc;
	//pugi::xml_parse_result resultado2;

	//Variavel para receber o xml feito o parser
	xml_string_writer writer;

	std::string command1 = "sudo brctl addbr ";
	std::string command2 = "sudo ifconfig ";
	std::string command3 = " up";
	std::string concat;

	//variavel para verificar a quandiade de dominios ja criados e criar apenas se precisar mais
	int numOfDefinedDomains = virConnectNumOfDefinedDomains(conn);

	for (int i = 0; i< tNodes; i++) {

		//Seta o nome da VM para que nao seja duplicado na criacao
		document.child("domain").child("name").text().set(vmName(i).c_str());
		//muda o nome da interface de ponte
		pugi::xml_attribute atributo = document.child("domain").child("devices").child("interface").child("source").first_attribute();
		atributo.set_value(brName(i).c_str());

		concat = command1 + brName(i);
		//Cria interface ponte
		std::cout << "Resultado bridge: "  << system (concat.c_str()) << " \n";

		//sobe interface ponte
		concat = command2 + brName(i) + command3;
		std::cout << "Resultado ifconfig bridge: "  << system (concat.c_str()) << " \n";

		//primeiro verifica se ja existem dominios definidos usando o num de domininos ja definidos.
		//Se i for maior que o num de dominos criados, entao e necessario criar mais dominios, se nao
		//basta inicia-los
		if (i > numOfDefinedDomains) {
			//Salva o XML modificado para uma variavel writer
			document.print(writer, "", pugi::format_raw);

			//usa a variavel writer para passar o novo xml para criacao da vm
			dom[i] = virDomainDefineXML(conn, writer.result.c_str ());
			if (!dom[i]) {
				fprintf(stderr, "Domain creation failed");
				return 1;
			}
		} else {
			dom[i] = virDomainLookupByName(conn,vmName(i).c_str());
		}
		if (!virDomainCreateWithFlags(dom[i], VIR_DOMAIN_START_AUTODESTROY)) {
			fprintf(stderr, "Guest %s has booted ", virDomainGetName(dom[i]));
			//com o dominio carregado, pega a interface dele do xml
			domainDoc.load(virDomainGetXMLDesc(dom[i],0));
			pugi::xml_attribute atributo = domainDoc.child("domain").child("devices").child("interface").child("target").first_attribute();
			std::cout << "Interface: "<< atributo.value() << "\n";
			domainInt[i] = atributo.value();
		}
		//virInterfacePtr interf = virInterfaceDefineXML(conn, mXMLTemplate.c_str (), 0);
		//fprintf(stderr, "Guest Interface is %s", virInterfaceGetName (interf));

		virDomainFree(dom[i]);
		domainDoc.reset();

		//reinicializa o conteudo de result
		writer.result = "";
	}

	//print_doc("UTF8 file from wide stream", document, resultado);




	// Create Ns2MobilityHelper with the specified trace log file as parameter
	Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);

	// open log file for output
	std::ofstream os;
	os.open (logFile.c_str ());



	//
	// We are interacting with the outside, real, world.  This means we have to
	// interact in real-time and therefore means we have to use the real-time
	// simulator and take the time to calculate checksums.
	//
	GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
	GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

	//
	// Create two ghost nodes.  The first will represent the virtual machine host
	// on the left side of the network; and the second will represent the VM on
	// the right side.
	//
	NodeContainer nodes;
	//nodes.Create (2);
	nodes.Create (tNodes);

	// Create all nodes.
	//NodeContainer stas;
	//stas.Create (nodeNum);

	/* if(sNodes > 0)
    ns2.Install (1, (1+mNodes)); // configure movements for mobile nodes from the second(Node number 1) node 
                                  //if there are static ones.
  else*/
	ns2.Install (); // configure movements for each node, while reading trace file.


	//
	// We're going to use 802.11 A so set up a wifi helper to reflect that.
	//
	WifiHelper wifi = WifiHelper::Default ();
	wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate54Mbps"));

	//
	// No reason for pesky access points, so we'll use an ad-hoc network.
	//
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
	wifiMac.SetType ("ns3::AdhocWifiMac");

	//
	// Configure the physcial layer.
	//
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());

	//
	// Install the wireless devices onto our ghost nodes.
	//
	NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

	//
	// We need location information since we are talking about wifi, so add a
	// constant position to the ghost nodes.
	//

	MobilityHelper mobility;
	mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
			"MinX", DoubleValue (0.0),
			"MinY", DoubleValue (0.0),
			"DeltaX", DoubleValue (2.5),
			"DeltaY", DoubleValue (8.9),
			"GridWidth", UintegerValue (3),
			"LayoutType", StringValue ("RowFirst"));

	mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
			"Bounds", RectangleValue (Rectangle (-10, 50, -10, 50)));

	//Mobilidade do no central
	mobility.Install (nodes);


	/*MobilityHelper mobility;
  ObjectFactory pos;
        pos.SetTypeId ("ns3::GridPositionAllocator");
        pos.Set ("MinX", DoubleValue (0.0));
        pos.Set ("MinY", DoubleValue (0.0));
        pos.Set ("DeltaX", DoubleValue (5.0));
        pos.Set ("DeltaY", DoubleValue (5.0));
        pos.Set ("GridWidth", UintegerValue (10));
        pos.Set ("LayoutType", StringValue ("RowFirst"));

        Ptr<Object> posAlloc =(pos.Create());

        mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
            "Speed", RandomVariableValue (UniformVariable (0.3,0.7)),
            "Pause", RandomVariableValue (ConstantVariable(2.0)),
            "PositionAllocator", PointerValue (posAlloc));

        mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("Uniform: 0:10"));   

  mobility.Install (nodes.Get (1));*/


	//Configuracao dos nos fixos na virtualizacao
	MobilityHelper mobility2;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (5.0, 5.0, 0.0));
	positionAlloc->Add (Vector (38.0, 42.0, 0.0));
	mobility2.SetPositionAllocator (positionAlloc);
	mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility2.Install (nodes.Get (0));
	//mobility2.Install (nodes.Get (2));


	InternetStackHelper internet;
	internet.Install (nodes);

	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.2.0.0", "255.255.0.0");
	Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

	//
	// Use the TapBridgeHelper to connect to the pre-configured tap devices for
	// the left side.  We go with "UseLocal" mode since the wifi devices do not
	// support promiscuous mode (because of their natures0.  This is a special
	// case mode that allows us to extend a linux bridge into ns-3 IFF we will
	// only see traffic from one other device on that bridge.  That is the case
	// for this configuration.
	//
	TapBridgeHelper tapBridge (interfaces.GetAddress(1));
	tapBridge.SetAttribute ("Mode", StringValue ("UseLocal"));

	std::cout << "Criando interfacesTap...\n";

	std::string command4 = "sudo tunctl -u daniel -t ";
	std::string command5 = "sudo brctl addif ";
	std::string command6 = " promisc";

	for (int i = 0; i < tNodes; i++) {
		//concat = command4 + tapName(i);
		//system (concat.c_str());
		//std::cout << "Comando1: " << concat << " \n";
		//concat = command5 + brName(i) + " " + tapName(i);
		//system (concat.c_str());
		//std::cout << "Comando2: " << concat << " \n";
		concat = command2 + vnetName(i) + command6;
		system (concat.c_str());
	}
	for (int i = 0; i < tNodes; i++) {
		tapBridge.SetAttribute ("DeviceName", StringValue (tapName(i)));
		tapBridge.Install (nodes.Get (i), devices.Get (i));
	}

	std::cout << "Criando interfacesTap OK...\n";
	//  tapBridge.SetAttribute ("DeviceName", StringValue ("tap0"));


	//
	// Connect the right side tap to the right side wifi device on the right-side
	// ghost node.
	//
	//tapBridge.SetAttribute ("DeviceName", StringValue ("tap-middle"));
	//  tapBridge.SetAttribute ("DeviceName", StringValue ("tap1"));
	//tapBridge.Install (nodes.Get (1), devices.Get (1));

	//tapBridge.SetAttribute ("DeviceName", StringValue ("tap-right"));
	//  tapBridge.SetAttribute ("DeviceName", StringValue ("tap1"));
	//tapBridge.Install (nodes.Get (2), devices.Get (2));


	//
	// Run the simulation for ten minutes to give the user time to play around
	//
	Simulator::Stop (Seconds (duration));


	//Adiconado para geracao de captura pcap. Ddevice vem de NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
	wifiPhy.EnablePcap ("mula", devices.Get (1), true);


	/*std::ostringstream oss;
  oss <<
	"/NodeList/" << nodes.Get (1)-> GetId () <<
	"/$ns3::MobilityModel/CourseChange";

  Config::Connect (oss.str (), MakeCallback (&CourseChange));
	 */

	Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeBoundCallback (&CourseChange, &os));


	Simulator::Run ();
	Simulator::Destroy ();

	os.close(); //close log file
	virConnectClose(conn);
	return 0;
}

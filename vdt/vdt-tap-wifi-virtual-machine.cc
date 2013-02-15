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
#include <string>
#include <fstream>
#include <sstream>
#include <math.h>
#include <csignal>
#include <unistd.h>

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
#include "ns3/netanim-module.h"
#include "ns3/wifi-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "pugixml/pugixml.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TapWifiVirtualMachineExample");


//Essas variaveis globais sao utilizadas para parar o ambiente

//Variavel global que armazena o num de nos total
int globalNumOfNodes;

//libvirt connection pointer
virConnectPtr conn;

//Ofstream do arquivo de log
std::ofstream os;

//Numero da Run atual
int currentRun = 0;

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
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  // Append zero chars
  int str_length = ret.length();
  for (int i = 0; i < 3 - str_length; i++)
    ret = "0" + ret;
  return "hydra-base" + ret;
}

std::string getBusName(std::string fileName)
{
  std::size_t pos;
  std::string busName;
  pos = fileName.find("ns_movements/");
  pos += 13;
  busName = fileName.substr(pos,4);
  return busName;
}

std::string brName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  // Append zero chars
  int str_length = ret.length();
  for (int i = 0; i < 3 - str_length; i++)
    ret = "0" + ret;
  return "br" + ret;

}

std::string tapName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  return "tap" + ret;
}

std::string hostNetName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  return "hostnet" + ret;

}

std::string vnetName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  return "vnet" + ret;
}

std::string animationName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  return "animation" + ret;
}

std::string runFolderName(int num)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  return "run" + ret;

}

std::string ipAddr(int num)
{
  std::stringstream ss1;
  std::stringstream ss2;
  int octeto4;
  int octeto3;

  octeto4 = (num + 1) % 254;
  octeto3 = int(floor((num + 1) / 254));
  ss1 << octeto3;
  std::string ret1;
  ss1 >> ret1;

  ss2 << octeto4;
  std::string ret2;
  ss2 >> ret2;
  return "10.2." + ret1 + "." + ret2;
}


std::string imageName(int num, const std::string fileName)
{
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  size_t found;
  found=fileName.find_last_of(".");

  return fileName.substr(0,found) + ret + "." +  fileName.substr(found+1);

}

std::string getFilenamePath (const std::string& str)
{
  size_t found;
  found=str.find_last_of("/\\");
  return str.substr(0,found) + "/";
}

std::string getFilename (const std::string& str)
{
  size_t found;
  found=str.find_last_of("/\\");
  return str.substr(found+1);
}


void copyImage(const std::string& srcName,const std::string& newName) {
  std::ifstream  src(srcName.c_str());
  std::ofstream  dst(newName.c_str());

  dst << src.rdbuf();
}

bool fileExists(const std::string& fileName)
{
  std::ifstream infile(fileName.c_str());
  return infile.good();
}

void stopSystem(int sig = 0){

  std::string command1 = "sudo ifconfig ";
  std::string command2 = " down";
  std::string command3 = "sudo brctl delif ";
  std::string command4 = "sudo brctl delbr ";
  std::string command5 = "sudo tunctl -d ";

  std::string concat;

  std::cout << "Signal Detected: " << sig  << std::endl;

  //Checar esses passos e erros que podem estar dando
  //Destroi o simulador
  Simulator::Destroy ();

  //Fechar o arquivo de log
  os.close();

  //Salvar arquivo main-ns2-mob.log
  concat =  "mv main-ns2-mob.log /home/daniel/Dropbox/experimentos/rodadas/" + runFolderName(currentRun);
  system (concat.c_str());

  //Fecha a conexao com o libvirt
  virConnectClose(conn);

  for(int i = 0; i < globalNumOfNodes; i++){
      //Da um down na interface tap
      concat = command1 + tapName(i) + command2;
      system(concat.c_str());
      //Remove interface tap da bridge
      concat = command3 + brName(i) + " " + tapName(i);
      system(concat.c_str());
      //Remove interface tap
      concat = command5 + tapName(i);
      system(concat.c_str());
      //Da um down na interface bridge
      concat = command1 + brName(i) + command2;
      system(concat.c_str());
      //Remove interface bridge
      concat = command4 + brName(i);
      system(concat.c_str());
    }
  std::cout << "Exiting..." << std::endl;
  exit(sig);
}

struct xml_string_writer: pugi::xml_writer
{
  std::string result;

  virtual void write(const void* data, size_t size)
  {
    result += std::string(static_cast<const char*>(data), size);
  }
};

int stringToint (std::string string)
{
  int value;
  std::stringstream str(string);
  if (!(str >> value))
    return 0;
  else
    return value;
}

bool validateTracesFile (std::string tracesFile, std::string *fileArray)
{
  // To where each of traces file will be read to.
  std::string tracesLineRead;
  int numberOfNodes = 0;
  int cnt = 0;

  std::ifstream traces(tracesFile.c_str ());

  if (traces.is_open()) {
      if (std::getline(traces, tracesLineRead)){
          numberOfNodes = stringToint(tracesLineRead);
          // check if the number of files
          if (!numberOfNodes || numberOfNodes < globalNumOfNodes) {
              traces.close();
              std::cout << "Wrong number of nodes in traces file.";
              return false;
            } else {
              while (traces.good()) {
                  if (std::getline(traces, tracesLineRead)) {
                      // Debug
                      std::cout << tracesLineRead << std::endl;
                      fileArray[cnt] = tracesLineRead;
                      cnt++;
                    }
                }
              if (cnt == numberOfNodes) {
                  traces.close();
                  std::cout << "Traces File is Valid";
                  return true;
                }
            }
        } else {
          traces.close();
          std::cout << "Could not read number of lines in traces file; Check if file is correct.";
          return false;
        }
    }
  std::cout << "Could not traces file for read. Check if the file path is correct";
  return false;
}

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

void colectLogs()
{
  std::string concat;
  std::string command1 = "./vdt/genHostsFile.sh";
  std::string command2 = "./vdt/slurp_expect.exp root hosts ";
  std::string command3 = " /root/logs/throughput .";
  std::string command4 = "./vdt/pssh_expect.exp root hosts \"rm -rf /root/logs/throughput/*\"";
  std::string command5 = "./vdt/pssh_expect.exp root hosts \"uci get system.@system[0].hostname\"";

  //Criar pasta de destino dos arquivos da rodada
  concat = "mkdir -p /home/daniel/Dropbox/experimentos/rodadas/" + runFolderName(currentRun);
  system (concat.c_str());

  //Gerar arquivo de host
  system (command1.c_str());

  //Copiar arquivos das maquinas virtuais para pasta de experimentos
  concat  = command2 + "/home/daniel/Dropbox/experimentos/rodadas/" + runFolderName(currentRun) + command3;
  system (concat.c_str());

  //Apagar arquivos das maquinas virtuais
  system (command4.c_str());

  //Criar arquivo de hostnames
  concat = command5 + " > " + "/home/daniel/Dropbox/experimentos/rodadas/" + runFolderName(currentRun) + "/hostnames.txt";
  system (concat.c_str());

  //Salvar arquivos pcap
  concat = "mkdir " + runFolderName(currentRun);
  system (concat.c_str());
  concat = "mv *.pcap " + runFolderName(currentRun);
  system (concat.c_str());
}

void stopVirtualMachines()
{
  std::string command1 = "./vdt/genHostsFile.sh";
  std::string command2 = "./vdt/pssh_expect.exp root hosts reboot";

  //Gerar arquivo de host
  system (command1.c_str());

  //Chama o reboot das VMs para desliga-las
  system (command2.c_str());
}

void printUsage (char* argv[])
{
  std::cout << "Usage of " << argv[0] << " :\n\n"
               "NS_GLOBAL_VALUE=\"RngRun=1\"./waf --run \"vdt-tap-wifi-virtual-machine \n"
               "--traceFile='/home/daniel/workspace/ns-3-dev/src/tap-bridge/examples/mobility_traces.txt'\n"
               "--mNodes=3 --sNodes=0 --duration=1000.0 --logFile=main-ns2-mob.log\n"
               "--sXMLTemplate='/home/daniel/workspace/ns-3-dev/src/tap-bridge/examples/node.xml'\n"
               "--mXMLTemplate='/home/daniel/workspace/ns-3-dev/src/tap-bridge/examples/node.xml'\n"
               "--srcImage='/home/daniel/workspace/ns-3-dev/src/tap-bridge/examples/setup/hydra-node.image'\"\n\n"

               "NOTE 1: The RngRun=1 variable sets the run number in the NS3::GlobalValue to that consequent runs can lead to non deterministc results"
               "NOTE 2: ns2-traces-files-descriptors could be an absolute or relative path. You could use the file default.ns_movements\n"
               "        included in the same directory that the present file.\n\n"
               "NOTE 3: Number of mobile nodes present in the trace file must match with the command line argument mNodes and must\n"
               "        be a positive number. Note that you must know it before to be able to load it and this has to match the number\n"
               "        of mobile nodes.\n\n"
               "NOTE 4: Number of static nodes must be a positive number or zero. If zero, it's assumed that all nodes\n"
               "        are mobile.\n\n"
               "NOTE 5: Duration must be a positive number and has to match the duration of the movement files. Note that you must know\n"
               "        it before to be able to load it.\n\n"
               "NOTE 6: The XML templates are used to define the configuration of the mobile and static nodes.\n"
               "        The Static Node XML template can be left undefined.  \n"
               "NOTE 7: The Source Disk Image must be in the folder are used to define the configuration of the mobile and static nodes.\n"
               "        The Static Node XML template can be left undefined.  \n";
}

int
main (int argc, char *argv[])
{

  //Tratar sinais de termino e abort
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = stopSystem;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGABRT, &sigIntHandler, NULL);//If program aborts go to assigned function "myFunction".
  sigaction(SIGTERM, &sigIntHandler, NULL);//If program terminates go to assigned function "myFunction".
  sigaction(SIGINT, &sigIntHandler, NULL);
  sigaction(SIGSEGV, &sigIntHandler, NULL);


  std::string traceFile;
  std::string logFile;
  std::string sXMLTemplateFile;
  std::string mXMLTemplateFile;
  std::string srcImage;

  int mNodes; //Number of Mobile Nodes
  int sNodes; // Number of Static Nodes
  int tNodes; //Total number of nodes = mNodes + sNodes.
  double duration; //Run Duration. Must be the same duration from nslog files

  IntegerValue runValue;
  GlobalValue::GetValueByName(std::string("RngRun"), runValue);
  currentRun = runValue.Get (); //Number of the current run being performed to be performed.


  // Enable logging from the ns2 helper
  //LogComponentEnable ("Ns2MobilityHelper",LOG_LEVEL_DEBUG);

  // Parse command line attribute
  CommandLine cmd;
  cmd.AddValue ("traceFile", "Ns2 movement trace files descriptor", traceFile);
  cmd.AddValue ("mNodes", "Number of mobile nodes", mNodes);
  cmd.AddValue ("sNodes", "Number of static nodes", sNodes);
  cmd.AddValue ("duration", "Duration of Simulation", duration);
  cmd.AddValue ("logFile", "Log file", logFile);
  cmd.AddValue ("sXMLTemplate", "Static XML Template", sXMLTemplateFile);
  cmd.AddValue ("mXMLTemplate", "Mobile XML Template", mXMLTemplateFile);
  cmd.AddValue ("srcImage", "Source Disk Image", srcImage);
  cmd.Parse (argc,argv);

  // Check command line arguments
  if (traceFile.empty () || mNodes <= 0 || duration <= 0 || sNodes < 0 || logFile.empty () || mXMLTemplateFile.empty () || srcImage.empty())
    {
      printUsage (argv);
      return 0;
    }

  // open XML config files
  std::ifstream sXML(sXMLTemplateFile.c_str ());
  std::ifstream mXML(mXMLTemplateFile.c_str ());

  std::string sXMLTemplate((std::istreambuf_iterator<char>(sXML)), std::istreambuf_iterator<char>());
  std::string mXMLTemplate((std::istreambuf_iterator<char>(mXML)), std::istreambuf_iterator<char>());

  // close XML config files
  sXML.close();
  mXML.close();

  tNodes = sNodes + mNodes; //Obtaining the total quantity of nodes

  //Passar o num de nos total para ma variavel global que usuara isso para terminar taps e bridges
  globalNumOfNodes = tNodes;

  //Array para guardar os enderecos dos arquivos de mobilidade
  std::string traceFileArray[tNodes];

  if (!validateTracesFile(traceFile, traceFileArray))
    return 0;

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
  std::string command7 = "sudo brctl setfd ";
  std::string concat;

  //variavel para verificar a quandiade de dominios ja criados e criar apenas se precisar mais
  int numOfDefinedDomains = virConnectNumOfDefinedDomains(conn);

  std::cout << "Num of Defined Domains: " << numOfDefinedDomains << std::endl;

  for (int i = 0; i< tNodes; i++) {

      if(!fileExists(getFilenamePath(srcImage) + imageName(i,getFilename(srcImage)))) {
          //Cria copias da imagem do disco original da VM
          copyImage(srcImage,getFilenamePath(srcImage) + imageName(i,getFilename(srcImage)));

          //configura ip e nome da maquina na imagem
          concat = "sudo /bin/bash " + getFilenamePath(srcImage) + "prepare_image_node.sh " + getBusName(traceFileArray[i]) + " " + getFilenamePath(srcImage) + imageName(i,getFilename(srcImage)) + " " + ipAddr(i);
          system(concat.c_str());

        } else {
          std::cout << "Disk Image File OK" << std::endl;
        }

      //primeiro verifica se ja existem dominios definidos usando o num de domininos ja definidos.
      //Se i for maior que o num de dominos criados, entao e necessario criar mais dominios, se nao
      //basta inicia-los
      if (i > numOfDefinedDomains || numOfDefinedDomains == 0) {

          //Seta o nome da VM para que nao seja duplicado na criacao
          document.child("domain").child("name").text().set(vmName(i).c_str());
          //muda o nome da interface de ponte
          pugi::xml_attribute atributo = document.child("domain").child("devices").child("interface").child("source").first_attribute();
          atributo.set_value(brName(i).c_str());

          //muda o nome da interface de hostnet
          atributo = document.child("domain").child("devices").child("interface").next_sibling("interface").child("target").first_attribute();
          atributo.set_value(hostNetName(i).c_str());

          //muda o nome da imagem de disco
          atributo = document.child("domain").child("devices").child("disk").child("source").first_attribute();
          concat = getFilenamePath(srcImage) + imageName(i,getFilename(srcImage));
          atributo.set_value(concat.c_str());

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

      //Criacao do comando concatenado para criar interface bridge
      concat = command1 + brName(i);

      //Cria interface ponte
      std::cout << "Resultado bridge: "  << system (concat.c_str()) << std::endl;

      //Seta o Foward delay para 0
      concat = command7 + brName(i) + " 0";
      system (concat.c_str());

      //Sobe interface ponte
      concat = command2 + brName(i) + command3;
      std::cout << "Resultado ifconfig bridge: "  << system (concat.c_str()) << std::endl;

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

  std::cout << "Pausa a execucao por 10 segundos para dar tempo de as maquinas darem boot" << std::endl;
  sleep(10);

  // Setup bridge to enable broadcast
  std::cout << "Setuping bridge device to allow broadcast" << std::endl;
  concat = "sudo /bin/bash " + getFilenamePath(srcImage) + "setup_bridge.sh";
  system(concat.c_str());

  //print_doc("UTF8 file from wide stream", document, resultado);

  // open log file for output
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
  nodes.Create (tNodes);

  // Create Ns2MobilityHelper with the specified trace log file as parameter
  int counter = 0;
  Ns2MobilityHelper *traces[mNodes];
  //Hack para evitar unused variable warnnig do NS3
  (void) traces;
  for (NodeList::Iterator j = NodeList::Begin ();
       j != NodeList::End (); ++j)
    {
      traces[counter] = new Ns2MobilityHelper (traceFileArray[counter]);
      std::cout << "Instalando mobilidade nos nos com o trace do NS2. No: " << counter << std::endl;
      traces[counter]->Install(*j);
      counter++;
    }

  /* if(sNodes > 0)
    ns2.Install (1, (1+mNodes)); // configure movements for mobile nodes from the second(Node number 1) node
                                  //if there are static ones.
  else*/
  //Commented to test install if beging and end;
  //ns2.Install (); // configure movements for each node, while reading trace file.
  //Teste do vetor de NS2MobilityHelper
  //ns2.Install (nodes.Begin(), nodes.Begin());

  //
  // We're going to use 802.11 A so set up a wifi helper to reflect that.
  //
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"),
                                "ControlMode", StringValue ("DsssRate11Mbps"));

  //
  // No reason for pesky access points, so we'll use an ad-hoc network.
  //
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  //
  // Configure the physcial layer.
  //
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel",
                                 "m0", DoubleValue(1.5), "m1", DoubleValue(0.75), "m2", DoubleValue(0.75));
  wifiChannel.AddPropagationLoss("ns3::RandomPropagationLossModel");
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

  //  MobilityHelper mobility;
  //  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  //                                 "MinX", DoubleValue (0.0),
  //                                 "MinY", DoubleValue (0.0),
  //                                 "DeltaX", DoubleValue (2.5),
  //                                 "DeltaY", DoubleValue (8.9),
  //                                 "GridWidth", UintegerValue (3),
  //                                 "LayoutType", StringValue ("RowFirst"));

  //Define a mobilidade com RandomWalk2DMobilityModel e velocidade constante de 10m/s
  //  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
  //                             "Bounds", RectangleValue (Rectangle (-10, 50, -10, 50)),
  //                             "Speed",  StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));


  //Configuracao dos nos fixos na virtualizacao
  //  MobilityHelper mobility2;
  //  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  //  positionAlloc->Add (Vector (5.0, 5.0, 0.0));
  //  positionAlloc->Add (Vector (38.0, 42.0, 0.0));
  //  mobility2.SetPositionAllocator (positionAlloc);
  //  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  //Instalacao de mobilidade estatica - Comentado para o teste do vetor de NS2Mobility e para nao
  //deixar o no 0 parado o tempo todo.
  //mobility2.Install (nodes.Get (0));
  //mobility2.Install (nodes.Get (2));


  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.2.0.0", "255.255.255.0");
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

  std::cout << "Criando interfacesTap..." << std::endl;

  std::string command4 = "sudo tunctl -u daniel -t ";
  std::string command5 = "sudo brctl addif ";
  std::string command6 = " promisc";

  for (int i = 0; i < tNodes; i++) {
      concat = command4 + tapName(i);
      system (concat.c_str());
      //std::cout << "Comando1: " << concat << std::endl;
      concat = command5 + brName(i) + " " + tapName(i);
      system (concat.c_str());
      //std::cout << "Comando2: " << concat << std::endl;
      concat = command2 + tapName(i) + command6 + command3;
      system (concat.c_str());
    }
  for (int i = 0; i < tNodes; i++) {
      tapBridge.SetAttribute ("DeviceName", StringValue (tapName(i)));
      tapBridge.Install (nodes.Get (i), devices.Get (i));
    }

  std::cout << "Criando interfacesTap OK..." << std::endl;

  //
  // Run the simulation for ten minutes to give the user time to play around
  //
  Simulator::Stop (Seconds (duration));


  //Adiconado para geracao de captura pcap. Device vem de NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
  for (int i = 0; i < tNodes; i++) {
      wifiPhy.EnablePcap ("mula", devices.Get (i), true);
    }

  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeBoundCallback (&CourseChange, &os));

  std::cout << "System Running..." << std::endl;

  Simulator::Run ();

  AnimationInterface anim (animationName(currentRun));

  //Coleta os logs das Maquinas Virtuais
  colectLogs();

  //Para as maquinas virtuais de forma correta
  stopVirtualMachines();

  //Pausa 5 segundos para dar tempo de as maquinas desligarem
  sleep(5);

  std::cout << "System shutting down" << std::endl;

  stopSystem();

  return 0;
}

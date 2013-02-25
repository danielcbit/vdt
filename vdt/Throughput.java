import java.io.*;
import java.net.*;
import java.util.*;

public class Throughput extends Thread {
	
	//directory where packets are stored
	//private static final String macFile = "/dieselram/MAC";
	private static final String macFile = "/tmp/MAC";

	//directory where Throughput logs are stored
	//private static final String logDirectory = "/logs/throughput";
	private static final String logDirectory = "/root/logs/throughput";

	//directory where Throughput logs are stored
	//private static final String generalLogDirectory = "/logs";
	private static final String generalLogDirectory = "/root/logs";

	// LiveIP flag indicating bus connectivity
	private static final String busConnect = "/dieselram/BUSCONNECT";
	
	// LiveIP flag indicating AP connectivity (use alternate
	// flag for throughput experiments)
	private static final String apConnect = "/dieselram/ALTAPCONNECT";
	
	// IP address / host name for host throughput tests
	private static String hostHost = "128.119.245.66";
	
	// Printable host name for inclusion in logs
	private static String hostName = "prisms";

	//identifies the function of each thread object
	private int threadIdentity = 999;

	//nodes broadcast their desire for packets on this port and
	// use it as the TCP listen socket
	private static final int greetingPort = 24515;
	
	// listen ports used by host, port connected to defines
	// role that is played by the host
	private static final int hostSendPort = 24516;
	private static final int hostRecvPort = 24517;

	//time between greeting broadcasts
	private static final int greetingInterval = 500;

	//size of each chunk sent to receiver
	private static final int chunkSize = 1400;
	
	//size of read buffer
	private static final int recvChunkSize = 4096;

	//time between hearing greeting messages that will cause a disconnect and reset
        private static final int connectionTimeout = 700;
	
	//maximum test duration
	private static final int maxBusTestTime = 60000;
	private static final int maxHostTestTime = 60000;

	//determines if we should be verbose
	private static boolean verbose = false;
	
	// suppress any threads?
	private static boolean suppressBus = false;
	private static boolean suppressHost = false;
	
	// limit beacons to eth0 or wlan0
	private static boolean ethOnly = false;
	private static boolean wlanOnly = false;

	//my MAC
	private static String localMAC = "NoMACAddress";
	private static boolean haveMAC = false;

	public Throughput() {
	}

	public static void main(String[] args) {
		//get passed command line arguments
		for (int a=0; a<args.length; a++) {
			if (args[a].compareTo("-v") == 0) {
				verbose = true;
			} else if (args[a].compareTo("-b") == 0) {
				suppressBus = true;
			} else if (args[a].compareTo("-h") == 0) {
				suppressHost = true;
			} else if (args[a].compareTo("-e") == 0) {
				ethOnly = true;
			} else if (args[a].compareTo("-w") == 0) {
				wlanOnly = true;
			} else if (args[a].compareTo("-i") == 0) {
				a++;
				if (a < args.length) {
					hostHost = args[a];
				}
			} else if (args[a].compareTo("-n") == 0) {
				a++;
				if (a < args.length) {
					hostName = args[a];
				}
			}
		}
		if (verbose) {
			System.out.println("Destination address : " + hostHost);
			System.out.println("Destination host    : " + hostName);
		}
		//make sure the log directories exist
		if (!(new File(generalLogDirectory).exists())) {
			try {
				Runtime.getRuntime().exec("mkdir " + generalLogDirectory);
			} catch (Exception e) { }
		}
		if (!(new File(logDirectory).exists())) {
			try {
				Runtime.getRuntime().exec("mkdir " + logDirectory);
			} catch (Exception e) {	}
		}
		//find our local MAC address
		try {
			BufferedReader fi = new BufferedReader(new FileReader(macFile));
			localMAC = fi.readLine().trim();
			fi.close();
			haveMAC = true;
		} catch (Exception e) {
			System.err.println("Unable to get MAC from " + macFile);
			localMAC = "DE:AD:DE:AD:DE:AD";
			haveMAC = false;
		}
		Throughput Throughput1 = null;
		// Spawn greeting sender
		if (haveMAC && !suppressBus) {
			Throughput1 = new Throughput();
			Throughput1.threadIdentity = 3;
			Throughput1.start();
		}
		// Spawn greeting listener. Should be ok even if we don't
		// have our own MAC. Also, listen for bus tests even if we
		// don't initiate bus tests.
		Throughput1 = new Throughput();
		Throughput1.threadIdentity = 2;
		Throughput1.start();
		// Spawn file receiver
		if (haveMAC && !suppressBus) {
			Throughput1 = new Throughput();
			Throughput1.threadIdentity = 5;
			Throughput1.start();
		}
		// Spawn host test monitor
		if (haveMAC && !suppressHost) {
			Throughput1 = new Throughput();
			Throughput1.threadIdentity = 7;
			Throughput1.start();
		}
	}

	public void run() {
		//spawn the appropriate thread activity based on threadIdentity
		switch (threadIdentity) {
			case 2:
				greetingListener();
				break;
			case 3:
				greetingSender();
				break;
			case 5:
				fileReceiver();
				break;
			case 7:
				hostMonitor();
				break;
			default:
				System.err.println("Attempted to start thread of unknown identity");
				break;
		}
	}

	/**
	 * Thread that spawns file sender when greetings are received.
	 */
	private void greetingListener() {
		if (verbose) {
			System.out.println("Starting greetingListener");
			System.out.println("Local MAC is: " + localMAC);
		}
		// Create the socket that we'll be listening on.
		DatagramSocket dgSock = null;
		try {
			dgSock = new DatagramSocket(greetingPort);
			dgSock.setSoTimeout(0);
			dgSock.setBroadcast(true);
		} catch (Exception e) {
			System.err.println("Unable to create greeting listener socket:");
			System.err.println(e);
			System.exit(1);
		}
		byte[] inbuf1 = new byte[1500];
		DatagramPacket packet = new DatagramPacket(inbuf1, inbuf1.length);
		while (true) {
			//wait for greeting
			String sMAC = null;
			try {
				dgSock.receive(packet);
				sMAC = new String(packet.getData(), 0, packet.getLength());
			} catch (Exception e) {
				System.err.println("Error receiving greeting: " + e);
				continue;
			}
			// If it's not a packet from myself.
            if (sMAC.compareTo(localMAC) != 0) {
				if (verbose) {
					System.out.println("Heard greeting from: " + sMAC);
				}
				fileSender(packet.getAddress().getHostAddress());
				// Clear out any queued beacon messages. Don't start
				// again for another second.
				long start = System.currentTimeMillis();
				while (true) {
					long now = System.currentTimeMillis();
					long dif = 1000 - (now - start);
					if (dif <= 0) {
						break;
					}
					try {
						dgSock.setSoTimeout((int)dif);
						dgSock.receive(packet);
					} catch (Exception e) {
						break;
					}
				}
				try {
					dgSock.setSoTimeout(0);
				} catch (Exception e) { }
			} else {
				if (verbose) {
					System.out.println("Greeting from self");
				}
			}
		}
	}

	/**
	 * Returns a broadcast address for a given interface name.
	 * @param ifName Interface name.
	 * @return Broadcast address as an InetAddress object.
	 */
	private InetAddress getBroadcast(String ifName) throws Exception {
		try {
			NetworkInterface ni = NetworkInterface.getByName(ifName);
			Enumeration <InetAddress> adrEnum = ni.getInetAddresses();
			if (!adrEnum.hasMoreElements()) {
				throw(new Exception("No address"));
			}
			// The assumption is that the netmask is 255.255.255.0.
			while (adrEnum.hasMoreElements()) {
				InetAddress adr = adrEnum.nextElement();
				byte [] a = adr.getAddress();
				if (a.length != 4) {
					// IPv6?
					continue;
				}
				a[3] = (byte)255;
				return InetAddress.getByAddress(a);
			}
		} catch (Exception e) {
			throw(e);
		}
		throw(new Exception("No broadcast address"));
	}

	/**
	 * Thread to broadcast beacon on wlan0 and eth0 subnets.
	 */
	private void greetingSender() {

		if (verbose) {
			System.out.println("starting greetingSender");
		}
		// Create the socket we'll be sending out on.
		DatagramSocket bcSock = null;
		try {
			bcSock = new DatagramSocket();
			bcSock.setBroadcast(true);
		} catch (Exception e) {
			System.err.println("Unable to create broadcast sender socket: " + e);
			System.exit(1);
		}
		// We send our MAC in the broadcast
		byte[] sendBuf = null;
		sendBuf = localMAC.getBytes();
		// Formulate the eth0 broadcast message since this link should
		// not change.
		DatagramPacket packetEth0 = null;
		try {
			InetAddress iaEth0 = getBroadcast("eth0");
			packetEth0 = 
				new DatagramPacket(sendBuf, sendBuf.length, iaEth0, greetingPort);
			if (verbose) {
				System.out.println("Eth0 broadcast address is " + iaEth0.getHostAddress());
			}
		} catch (Exception e) {
			System.err.println("Unable to create broadcast packet: " + e);
			System.exit(1);
		}
		// Loop forever sending beacons
		boolean wlan0Ready = false;
		DatagramPacket packetWlan0 = null;
		while (true) {
			// Send the greeting on eth0
			if (!wlanOnly) {
				try {
					bcSock.send(packetEth0);
				} catch(Exception e) {
					System.err.println("Unable to send eth0 broadcast packet: " + e);
				}
			}
			// Send the greeting on wlan0 if connected to a bus. Our
			// sleep time should be sufficiently short that we'll never
			// miss changing connections from one bus to the next.
			if (!ethOnly) {
				if (wlan0Ready) {
					if (!(new File(busConnect).exists())) {
						packetWlan0 = null;
						wlan0Ready = false;
					}
				} else {
					if (new File(busConnect).exists()) {
						try {
							InetAddress iaWlan0 = getBroadcast("wlan0");
							packetWlan0 =
								new DatagramPacket(sendBuf, sendBuf.length, iaWlan0, greetingPort);
							wlan0Ready = true;
							if (verbose) {
								System.out.println("Initiate wlan0 broadcasts: " + iaWlan0.getHostAddress());
							}
						} catch (Exception e) {	
							if (verbose) {
								System.out.println("Failed setting up wlan0 broadcasts: " + e);
							}
						}
					}
				}
				if (wlan0Ready) {
					try {
						bcSock.send(packetWlan0);
					} catch (Exception e) {
						if (verbose) {
							System.out.println("Failed sending wlan0 broadcasts: " + e);
						}
					}
				}
			}
			// Sleep until it's time to send the next beacon
			try {
				Thread.sleep(greetingInterval);
			} catch (Exception e) { }
		}
	}

	/**
	 * Send random data to peer as quickly as possible.
	 * @param target Destination internet address
	 */
	private void fileSender(String target) {
		if (verbose) {
			System.out.println("Starting fileSender to " + target);
		}
		Random junk = new Random();
		Socket tcpSocket = null;
		int bytesSent = 0;
		try {
			//create junk
			byte[] bytes = new byte[chunkSize];
			junk.nextBytes(bytes);
			//tcpSocket = new Socket(target, greetingPort);
			tcpSocket = new Socket();
			tcpSocket.connect(new InetSocketAddress(target, greetingPort), connectionTimeout);
			tcpSocket.setSoTimeout(connectionTimeout);
			//blast chunkSize chunks to receiver
			long start = System.currentTimeMillis();
			while (true) {
				if (bytesSent == 0) {
					System.arraycopy(localMAC.getBytes(), 0, bytes, 0, localMAC.length());
				}
				tcpSocket.getOutputStream().write(bytes);
				bytesSent += chunkSize;
				if ((System.currentTimeMillis() - start) > maxBusTestTime) {
					// Don't send for more than a minute
					break;
				}
			}
			try {
				tcpSocket.close();
			} catch (Exception ex) {
			}
		}
		//connection lost, cleanup and terminate
		catch (Exception e) {
			try {
				if (tcpSocket != null) {
					tcpSocket.close();
				}
			} catch (Exception ex) {
			}
		}
		if (verbose) {
			System.out.println(
				"Sending connection terminated: " + bytesSent + " bytes sent");
		}
	}

	/**
	 * Thread that receives incoming connection requests and
	 * logs how much was received in what amount of time.
	 */
	private void fileReceiver() {
		Random namer = new Random(System.currentTimeMillis() - 10000);
		if (verbose) {
			System.out.println("Starting fileReceiver");
		}
		// Create the socket we'll listen on to get connections.
		ServerSocket servSock = null;
		try {
			servSock = new ServerSocket(greetingPort);
			servSock.setSoTimeout(0);
		} catch (Exception e) {
			System.err.println("Unable to create TCP listener socket: " + e);
			System.exit(1);
		}
		long bytesReceived = 0;
		long start = 0;
		long end = 0;
		int reportCounter = 0;
		int n = 0;
		Socket tcpSocket = null;
		String foreignMAC = "DE:AD:DE:AD:DE:AD";
		while (true) {
			tcpSocket = null;
			bytesReceived = 0;
			foreignMAC = "DE:AD:DE:AD:DE:AD";
			try {
				// Wait until a connection is received.
				tcpSocket = servSock.accept();
				tcpSocket.setSoTimeout(connectionTimeout);
			} catch (Exception e) {
				System.err.println("TCP accept failure: " + e);
				continue;
			}
			if (verbose) {
				System.out.println("Receiving connection opened from "
						+ tcpSocket.getInetAddress().getHostAddress());
			}
			start = new Date().getTime();
			if (verbose) {
				System.out.println("Start time: " + start);
			}
			byte[] bytes = new byte[recvChunkSize];
			if (verbose) {
				System.out.println("Receiving junk (" + recvChunkSize + ")...");
			}
			// Main data receive loop. Note: we record the time before and
			// after receiving data. If we get a connection timeout then we
			// don't want to include the time waiting -- doing nothing -- in
			// the performance calculation.
			end = 0;
			n = 0;
			while (true) {
				try {
					n = tcpSocket.getInputStream().read(bytes);
				} catch (Exception e) {
					if (verbose && (bytesReceived != 0)) {
						System.out.println("Receiving connection aborted/terminated");
					}
					break;
				}
				end = new Date().getTime();
				if (n < 0) {
					// Normal fin
					if (verbose) {
						System.out.println("Receiving connection returned EOF");
					}
					break;
				}
				if ((bytesReceived == 0) && n >= foreignMAC.length()) {
					foreignMAC = new String(bytes, 0, foreignMAC.length());
				}
				bytesReceived += n;
				if (verbose) {
					reportCounter++;
					if ((reportCounter % 1000) == 0) {
						System.out.println("Received " + bytesReceived
								+ " bytes so far...");
					}
				}
			}
			try {
				tcpSocket.close();
			} catch (Exception ex) { }
			if ((end == 0) || (bytesReceived == 0)) {
				return;
			}
			if (verbose && (bytesReceived != 0)) {
				try {
					System.out.println("End time: " + end);
					System.out.println("Running time: " + (end - start));
					System.out.println("Average speed was "
							+ (bytesReceived / ((end - start) / 1000))
							+ " bytes per second");
				} catch (Exception e) {
				}
			}
			logReceive(bytesReceived, foreignMAC, true, start, end, namer);
		}
	}
	
	/**
	 * Log that we have received data.
	 * @param bytesReceived  Total bytes received
	 * @param peer           Name/MAC of sender
	 * @param bus            True if peer is a bus, otherwise a host
	 * @param start          Test start time in millis
	 * @param end            Test end time
	 * @param namer          Random number generator for thread
	 */
	private void logReceive(long bytesReceived, String peer, boolean bus,
			long start, long end, Random namer) {
		// Log results iff we received some data
		if (bytesReceived <= 0) {
			return;
		}
		// Generate a unique log file name.
		File logFile = null;
		while (true) {
			String logName = System.currentTimeMillis() +
				new Integer(Math.abs(namer.nextInt())).toString();
			logFile = new File(logDirectory + "/" + logName);
			if (!logFile.exists()) {
				break;
			}
		}
		long time = end - start;
		if (verbose) {
			String w = null;
			if (bus) {
				w = "bus";
			} else {
				w = "host";
			}
			System.out.println("Writing log of "
					+ new Long(bytesReceived).toString()
					+ " bytes from " + w + " " + peer + " in "
					+ time + " ms to log file");
		}
		try {
			String current = " not ";
			String longitude = "0";
			String latitude = "0";
			String speed = "0";
			String direction = "0";
			String sats = "0";
			try {
				// Include GPS coordinates
				BufferedReader fr = new BufferedReader(
						new FileReader(new File("/dieselram/GPS")));
				StringTokenizer tok = new StringTokenizer(fr
						.readLine());
				tok.nextToken();
				if (tok.nextToken().compareTo("true") == 0) {
					current = " ";
				}
				tok = new StringTokenizer(fr.readLine());
				tok.nextToken();
				sats = tok.nextToken();
				tok = new StringTokenizer(fr.readLine());
				tok.nextToken();
				longitude = tok.nextToken();
				tok = new StringTokenizer(fr.readLine());
				tok.nextToken();
				latitude = tok.nextToken();
				tok = new StringTokenizer(fr.readLine());
				tok = new StringTokenizer(fr.readLine());
				tok = new StringTokenizer(fr.readLine());
				tok = new StringTokenizer(fr.readLine());
				tok = new StringTokenizer(fr.readLine());
				tok = new StringTokenizer(fr.readLine());
				tok.nextToken();
				speed = tok.nextToken();
				tok = new StringTokenizer(fr.readLine());
				tok.nextToken();
				direction = tok.nextToken();
				fr.close();
			} catch (Exception e) {}
			// Who sent us the data?
			String target = null;
			String testType = null;
			if (bus) {
				if (peer.compareTo("DE:AD:DE:AD:DE:AD") == 0) {
					target = "unknown";
				} else {
					target = resolveMACtoBus(peer);
				}
				testType = "(bus-bus)";
			} else {
				target = peer;
				testType = "(bus-host)";
			}
			// Put it in a file.
			Calendar calendar = Calendar.getInstance();
			FileWriter fw = new FileWriter(logFile);
			fw.write(new String(resolveMACtoBus(localMAC)
				+ " received "
				+ new Long(bytesReceived).toString()
				+ " bytes from " + target + " in "
				+ time + " ms on "
				+ calendar.get(Calendar.HOUR_OF_DAY) + ":"
				+ calendar.get(Calendar.MINUTE) + ":"
				+ calendar.get(Calendar.SECOND)
				+ " at location is" + current
				+ "current longitude is " + longitude
				+ " latitude is " + latitude + " speed is "
				+ speed + " direction is " + direction
				+ " using " + sats + " satellites "
				+ testType));
			fw.close();
		}
		catch (Exception e) { }
	}

//	----------------NOT USED AS THIS IS ONLY FOR HOST - BUS CONTACT------------------------
	/**
	 * Gets the bus AP SSID from the IDs file given the wifi dongle MAC.
	 * @param MAC Wifi dongle MAC
	 * @return SSID in form PVTA_nnnn where nnnn is the bus number.
	 */
	private static String resolveMACtoBus(String MAC) {
		if (MAC == null) {
			return "unkown";
		}
		try {
			String idNum = null;
			BufferedReader fr = new BufferedReader(new FileReader(new File(
					"/diesel/IDs")));
			String temp = fr.readLine();
			while (temp != null) {
				StringTokenizer t = new StringTokenizer(temp);
				String id = t.nextToken();
				if (id.toLowerCase().compareTo(MAC.toLowerCase()) == 0) {
					idNum = t.nextToken();
					break;
				}
				temp = fr.readLine();
			}
			fr.close();
			if (idNum != null) {
				return new String("PVTA_" + idNum);
			}

		} catch (Exception e) { }
		return MAC;
	}
	
	/**
	 * Blast bytes to host. Host will log the results, not us.
	 */
	private void hostUpstream() {
		if (verbose) {
			System.out.println("Starting hostUpstream");
		}
		Random junk = new Random();
		Socket tcpSocket = null;
		int bytesSent = 0;
		try {
			//create junk
			byte[] bytes = new byte[chunkSize];
			junk.nextBytes(bytes);
			//tcpSocket = new Socket(hostHost, hostRecvPort);
			tcpSocket = new Socket();
			tcpSocket.connect(new InetSocketAddress(hostHost, hostRecvPort), connectionTimeout);
			tcpSocket.setSoTimeout(connectionTimeout);

			//blast chunkSize chunks to receiver
			long start = System.currentTimeMillis();
			while (true) {
				if (bytesSent == 0) {
					System.arraycopy(localMAC.getBytes(), 0, bytes, 0, localMAC.length());
				}
				tcpSocket.getOutputStream().write(bytes);
				bytesSent += chunkSize;
				if ((System.currentTimeMillis() - start) > maxHostTestTime) {
					// Don't send for more than a minute
					break;
				}
			}
		}
		//connection lost, cleanup and terminate
		catch (Exception e) { }
		try {
			if (tcpSocket != null) {
				tcpSocket.close();
			}
		} catch (Exception e) { }
		if (verbose) {
			System.out.println("Upstream sent " + bytesSent + " bytes");
		}
	}
	
	/**
	 * Connect to host then start receiving data
	 * @param namer Random number generator to be used
	 */
	private void hostDownstream(Random namer) {
		if (verbose) {
			System.out.println("Starting hostDownstream");
		}
		// Connect to host
		Socket tcpSocket = null;
		try {
			//create junk
			//tcpSocket = new Socket(hostHost, hostSendPort);
			tcpSocket = new Socket();
			tcpSocket.connect(new InetSocketAddress(hostHost, hostSendPort), connectionTimeout);
			tcpSocket.setSoTimeout(connectionTimeout);
		} catch (Exception e) {
			if (tcpSocket != null) {
				try {
					tcpSocket.close();
				} catch (Exception ex) { }
			}
			if (verbose) {
				System.out.println("Unable to establish connection to host");
			}
			return;
		}
		byte[] bytes = new byte[recvChunkSize];
		// Loop getting data
		long start = 0;
		long bytesReceived = 0;
		int reportCounter = 0;
		int n = 0;
		long end = 0;
		while (true) {
			try {
				n = tcpSocket.getInputStream().read(bytes);
			} catch (Exception e) {
				if (verbose && (bytesReceived != 0)) {
					System.out.println("Receiving connection aborted/terminated");
				}
				break;
			}
			end = new Date().getTime();
			if (n < 0) {
				// Normal fin
				if (verbose) {
					System.out.println("Receiving connection returned EOF");
				}
				break;
			}
			if (bytesReceived == 0) {
				start = new Date().getTime();
			}
			bytesReceived += n;
			if (verbose) {
				reportCounter++;
				if ((reportCounter % 1000) == 0) {
					System.out.println("Received " + bytesReceived
							+ " bytes so far...");
				}
			}
		}
		try {
			tcpSocket.close();
		} catch (Exception ex) { }
		// Was it a valid run?
		if ((bytesReceived == 0) || (start == 0) || (end == 0) || (end <= start)) {
			return;
		}
		// Log the results.
		if (verbose) {
			try {
				System.out.println("End time: " + end);
				System.out.println("Running time: " + (end - start));
				System.out.println("Average speed was "
						+ (bytesReceived / ((end - start) / 1000))
						+ " bytes per second");
			} catch (Exception e) {
			}
		}
		logReceive(bytesReceived, hostName, false, start, end, namer);
	}
	
	/**
	 * Wait for connectivity to an open AP and then initiate
	 * bus-host throughput tests.
	 */
	private void hostMonitor() {
		Random rand = new Random();
		Random namer = new Random(System.currentTimeMillis() - 55555);
		File apFile = new File(apConnect);
		while (true) {
			if (apFile.exists()) {
				if (rand.nextBoolean()) {
					hostUpstream();
				} else {
					hostDownstream(namer);
				}
				// Wait until we're done with this AP until we
				// try again.
				while (apFile.exists()) {
					try {
						Thread.sleep(1000);
					} catch (Exception e) { }
				}
			}
			try {
				Thread.sleep(500);
			} catch (Exception e) { }
		}
	}
}



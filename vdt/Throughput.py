import socket
import thread
import time
import os
import hashlib
from datetime import datetime
import struct
import optparse
import sys

parser = optparse.OptionParser()
parser.add_option('-t', help='ip address for host throughput tests', dest='host', action='store_true')

(opts, args) = parser.parse_args()

if ((opts.host is None) or (len(args) == 0)):
    sys.stderr.write("ERROR: a mandatory option is missing\n")
    sys.exit()
else:
    hostHost = args[0]

def getBroadcastAddress():
    brAddr = hostHost.split('.')
    brAddr[3] = '255'
    brAddr = '.'.join(brAddr)

    return brAddr

class Beacon:

    __slots__ = '__sock', '__addr'

    def __init__(self, port):
        "Initialize the beacon for sending and receiving data."
        self.__sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, True)
        self.__sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
        self.__sock.bind(('0.0.0.0', port))
        self.__addr = getBroadcastAddress(), port

    def __del__(self):
        "Shutdown and close the underlying socket."
        self.__sock.shutdown(socket.SHUT_RDWR)
        self.__sock.close()

    def recv(self, size):
        "Receive a broadcast through the underlying soket."
        return self.__sock.recvfrom(size)

    def send(self, data):
        "Send a broadcast through the underlying socket."
        assert self.__sock.sendto(data, self.__addr) == len(data), \
               'Not all data was sent through the socket!'

    def __gettimeout(self):
        return self.__sock.gettimeout()

    def settimeout(self, value):
        self.__sock.settimeout(value)

    def __deltimeout(self):
        self.__sock.setblocking(True)

    timeout = property(__gettimeout, settimeout, __deltimeout,
                       'Timeout on blocking socket operations.')

def getMACAddress():
    macFile = open('/tmp/MAC', 'r')
    return macFile.readlines()[0].strip()

localMAC = getMACAddress()

def run():
    "Test the beacon broadcasting class."
    b = Beacon(50000)
    c = Beacon(50000)
    thread.start_new_thread(greetingSender, (b,))
    thread.start_new_thread(fileReceiver, ())
    greetingListener(c)

def greetingSender(b):
    while True:
        b.send(localMAC)
        time.sleep(0.5)

def greetingListener(b):
    while True:
        data, address = b.recv(1500)
        if (data.decode() != localMAC):
            fileSender(address[0])
            start = datetime.now()
            b.settimeout(0.0)
            while True:
                now = datetime.now()
                diffTime = now - start 
                remaning = 1000 - (diffTime.microseconds / 1000)
                if (remaning <= 0):
                    break
                try:
                    b.recv(1500)
                except socket.error, (value, message):
                    sys.stdout.write("Greeting Listener: " + str(value) + str(message) + "\n")
                    break

            b.settimeout(None)
        else:
            sys.stdout.write("Greeating from self\n")

def fileSender(address):
    TCP_PORT = 5005
    MESSAGE = localMAC + os.urandom(1383)

    sys.stdout.write("Starting fileSender to " + address + "\n")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.7)
    try:
        s.connect((address, TCP_PORT))
    except socket.timeout, (message):
        sys.stdout.write("File Sender: "+  str(message) + "\n")
        return
    except socket.error, (value, message):
        if (value == 107):
            return
        sys.stdout.write("Could not connect. Giving Up" + str(message) + "\n")
        return

    start = int(time.time())
    totalBytesSent = 0
    bytesSent = 0
    while True:
        try:
            bytesSent = s.send(MESSAGE)
            if ((int(time.time()) - start) > 60):
				break

            totalBytesSent += bytesSent
            continue
        except socket.timeout, (message):
            sys.stdout.write("File Sender: "+  str(message) + "\n")
            break
        except socket.error, (value, message):
            sys.stdout.write("Data Send Stopped: " + str(value) + str(message) + " Total: " + str(totalBytesSent) + "\n")
            if(value == 11):
                continue
            else:
                break
    try:
        s.shutdown(socket.SHUT_RDWR)
    except socket.error, (value, message):
        if(value == 107):
            pass
    s.close()
    return

def fileReceiver():
    TCP_PORT = 5005
    BUFFER_SIZE = 4096
    
    namer = hashlib.sha224(str(time.time())).hexdigest()[:10]
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((hostHost, TCP_PORT))
    s.listen(1)
    while 1:
        try:
            conn, addr = s.accept()
        except socket.error, (value,message):
            sys.stdout.write("fileReceiver Error: " + str(value) + str(message) + "\n")

        startTime = datetime.now()
        bytesReceived = 0
        conn.settimeout(0.7)
        while 1:
            try:
                data = conn.recv(BUFFER_SIZE)
            except socket.timeout, (message):
                sys.stdout.write("File Receiver" + str(message) + "\n")
                break
            except socket.error, (value, message):
                sys.stdout.write("Data Receive Error: " + str(value) + str(message) + "\n")
                break

            endTime = datetime.now()
            if (len(data) > 0):
                if (bytesReceived == 0 and len(data) >= 17):
                    peer = data[:17]
                bytesReceived += len(data)
            else:
                break

        conn.shutdown(socket.SHUT_RDWR)
        conn.close()
        sys.stdout.write("File Receiver going to write log\n")
        logReceiver(bytesReceived, peer, True, startTime, endTime, namer)

def logReceiver(bytesReceived, peer, bus, startTime, endTime, namer):
   logDir = "/tmp/logs/throughput/"

   diffTime = endTime - startTime
   diffTime = diffTime.microseconds / 1000

   date = datetime.now()
   date = '{:%m %d %Y at %H:%M:%S}'.format(date)

   log_entry = "{!s} received {!s} bytes from {!s} in {!s} ms on {!s} at location is not current longitude is 0 latitude is 0 speed is 0 direction is 0 using 0 satellites (bus-bus)".format(localMAC, str(bytesReceived), peer, str(diffTime), date)
   
   logFileName = logDir + str(int(time.time())) + namer

   logFile = open(logFileName, 'w+')
   logFile.write(log_entry)
   logFile.close()

if __name__ == '__main__':
    run()

#import socket
#import select
from time import sleep
import json

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
from twisted.internet.task import LoopingCall
import sys, time

UDP_IP="127.0.0.1"
#UDP_IP="0.0.0.0"
UDP_PORT=54321

msg_object = {}
msg_object["command"] = {}
msg_object["command"]["playSound"] = "ref:000CB875"  # vampire feed sound
#msg_object["command"]["playSound"] = "paidthefine"
#msg_object["command"]["playSound"] = "eatpoop"

# defining obse variables
""" "<value>" """ # string
""" "ref:<value>" """ # ref
""" {} """ # string map
""" [] """ # array
""" "<name>":"<value>" """ # map entry

print "UDP target IP:", UDP_IP
print "UDP target port:", UDP_PORT
msg_text = json.dumps(msg_object)
print "message:", msg_text

class HeartbeatSender(DatagramProtocol):
    def __init__(self, data, host, port):
        self.data = data
        self.loopObj = None
        self.host = host
        self.port = port
        self.hbseconds = 5

    def startProtocol(self):
        # Called when transport is connected
        # I am ready to send heart beats
        #self.transport.joinGroup(self.host)
        self.transport.connect(self.host, self.port)
        self.loopObj = LoopingCall(self.sendHeartBeat)
        self.loopObj.start(self.hbseconds, now=False)

    def stopProtocol(self):
        "Called after all transport is teared down"
        pass

    def datagramReceived(self, data, (host, port)):
        print "received %r from %s:%d" % (data, host, port)

    def sendHeartBeat(self):
        self.transport.write(self.data, (self.host, self.port))


# create net object
heartBeatSenderObj = HeartbeatSender(msg_text, UDP_IP, UDP_PORT)
# add to reactor so it can be processed
reactor.listenUDP(0, heartBeatSenderObj)

# start reactor
reactor.run()
